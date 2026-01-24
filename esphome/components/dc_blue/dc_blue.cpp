#include "dc_blue.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32_FRAMEWORK_ARDUINO
#include <esp32-hal-timer.h>
#else
// ESP-IDF native timer API
#include "driver/gptimer.h"
#include "esp_attr.h"
#endif

namespace esphome
{
  namespace dc_blue
  {

    static const char *const TAG = "dc_blue";

    // Instance pointer - volatile since accessed from ISR context
    static DcBlueComponent *volatile instance = nullptr;

#ifdef USE_ESP32_FRAMEWORK_ARDUINO
    static hw_timer_t *Timer0_Cfg = nullptr;
#else
    static gptimer_handle_t gptimer = nullptr;
#endif

    // ISR state variables - all volatile for ISR/main thread synchronization
    static volatile uint32_t header = 0xFFFFFFFF;
    static volatile uint32_t frame = 0;
    static volatile bool waiting_for_header = true;
    static volatile bool capturing_frame = false;
    static volatile uint8_t captured_bytes = 0; // Only needs 0-24, uint8_t sufficient
    static volatile uint8_t timer_isr_calls = 0;

    // Queue mask for efficient modulo in ISR - DRAM_ATTR ensures availability when cache disabled
    static constexpr DRAM_ATTR uint8_t QUEUE_MASK = DcBlueComponent::QUEUE_SIZE - 1;
    static_assert((DcBlueComponent::QUEUE_SIZE & QUEUE_MASK) == 0, "QUEUE_SIZE must be power of 2");

    // Counter for dropped frames due to queue overflow (set in ISR, read/cleared in loop)
    static volatile uint8_t frames_dropped = 0;

#ifdef USE_ESP32_FRAMEWORK_ARDUINO
    static void IRAM_ATTR Timer0_ISR()
#else
    static bool IRAM_ATTR Timer0_ISR(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
#endif
    {
      // Sample on odd calls only (half-bit timing)
      timer_isr_calls++;
      if ((timer_isr_calls & 1) == 0)
      {
#ifdef USE_ESP32_FRAMEWORK_ARDUINO
        return;
#else
        return true;
#endif
      }

      // Safety check - should never happen but prevents null dereference
      if (instance == nullptr)
      {
#ifdef USE_ESP32_FRAMEWORK_ARDUINO
        return;
#else
        return true;
#endif
      }

      bool value = instance->data_pin_->digital_read();
      if (instance->inverted_)
      {
        value = !value;
      }

      if (waiting_for_header)
      {
        header = (header << 1) | value;
        if (header == 0x01)
        {
          header = 0xFFFFFFFF;
          waiting_for_header = false;
          capturing_frame = true;
          captured_bytes = 0;
        }
      }
      else if (capturing_frame)
      {
        frame = (frame << 1) | value;
        captured_bytes++;

        if (captured_bytes >= 24)
        {
          // Use bitmask for fast modulo (works because QUEUE_SIZE is power of 2)
          uint8_t write_idx = instance->process_queue_write_;
          uint8_t next_write_idx = (write_idx + 1) & QUEUE_MASK;

          // Check for queue overflow (would overwrite unread data)
          if (next_write_idx != instance->process_queue_read_)
          {
            instance->process_queue_[write_idx] = frame;
            instance->process_queue_write_ = next_write_idx;
          }
          else
          {
            // Queue full - drop frame and increment counter
            frames_dropped++;
          }

          // Reset state for next frame
          waiting_for_header = true;
          capturing_frame = false;
          captured_bytes = 0;
          frame = 0;
        }
      }

#ifndef USE_ESP32_FRAMEWORK_ARDUINO
      return true;
#endif
    }

#ifdef USE_ESP32_FRAMEWORK_ARDUINO
    static void IRAM_ATTR pinChangeIrq(hw_timer_t *timer)
    {
      if (waiting_for_header)
      {
        timer_isr_calls = 0;
        timerRestart(timer);
      }
    }
#else
    // Timer handle for ESP-IDF - DRAM_ATTR ensures it's accessible when cache is disabled
    static DRAM_ATTR gptimer_handle_t idf_gptimer_handle;

    static void IRAM_ATTR pinChangeIrq(gptimer_handle_t *timer_ptr)
    {
      if (waiting_for_header && timer_ptr != nullptr)
      {
        timer_isr_calls = 0;
        // Stop timer, reset count, start timer for clean synchronization
        // This is more reliable than just resetting the count, as it ensures
        // the alarm state is properly reset. These functions are ISR-safe.
        gptimer_stop(*timer_ptr);
        gptimer_set_raw_count(*timer_ptr, 0);
        gptimer_start(*timer_ptr);
      }
    }
#endif

    void DcBlueComponent::setup()
    {
      instance = this;

#ifdef USE_ESP32_FRAMEWORK_ARDUINO
      // Arduino framework timer initialization
      Timer0_Cfg = timerBegin(1000000);

      if (data_pin_ != nullptr)
      {
        data_pin_->setup();
        data_pin_->pin_mode(gpio::FLAG_INPUT);
        data_pin_->attach_interrupt(&pinChangeIrq, Timer0_Cfg, gpio::INTERRUPT_ANY_EDGE);
      }

      if (trigger_pin_ != nullptr)
      {
        trigger_pin_->setup();
        trigger_pin_->pin_mode(gpio::FLAG_OUTPUT);
        trigger_pin_->digital_write(0);
      }

      timerAttachInterrupt(Timer0_Cfg, &Timer0_ISR);
      timerAlarm(Timer0_Cfg, this->symbol_period_ / 2, true, 0);
#else
      // ESP-IDF native timer initialization
      gptimer_config_t timer_config = {
          .clk_src = GPTIMER_CLK_SRC_DEFAULT,
          .direction = GPTIMER_COUNT_UP,
          .resolution_hz = 1000000, // 1MHz, 1 tick = 1us
          .intr_priority = 0,
          .flags = {
              .intr_shared = false,
          },
      };
      ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

      gptimer_alarm_config_t alarm_config = {
          .alarm_count = static_cast<uint64_t>(this->symbol_period_ / 2),
          .reload_count = 0,
          .flags = {
              .auto_reload_on_alarm = true,
          },
      };
      ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

      gptimer_event_callbacks_t cbs = {
          .on_alarm = Timer0_ISR,
      };
      ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));

      // Store timer handle for use in pin change ISR
      idf_gptimer_handle = gptimer;

      if (data_pin_ != nullptr)
      {
        data_pin_->setup();
        data_pin_->pin_mode(gpio::FLAG_INPUT);
        data_pin_->attach_interrupt(pinChangeIrq, &idf_gptimer_handle, gpio::INTERRUPT_ANY_EDGE);
      }

      if (trigger_pin_ != nullptr)
      {
        trigger_pin_->setup();
        trigger_pin_->pin_mode(gpio::FLAG_OUTPUT);
        trigger_pin_->digital_write(0);
      }

      ESP_ERROR_CHECK(gptimer_enable(gptimer));
      ESP_ERROR_CHECK(gptimer_start(gptimer));
#endif

      if (this->garage_cover_sensor_ != nullptr)
      {
        this->garage_cover_sensor_->setup();
      }
      else
      {
        ESP_LOGE(TAG, "garage_cover_sensor is null - cover will not function");
      }

      ESP_LOGD(TAG, "Assume Light off");
      if (this->light_binary_sensor_ != nullptr)
      {
        this->light_binary_sensor_->publish_state(false);
      }

      ESP_LOGD(TAG, "Assume running on AC mains power");
      if (this->ac_power_binary_sensor_ != nullptr)
      {
        this->ac_power_binary_sensor_->publish_state(true);
      }
    }

    void DcBlueComponent::loop()
    {
      // Check for dropped frames (queue overflow) - atomic exchange to avoid missing increments
      uint8_t dropped = frames_dropped;
      if (dropped > 0)
      {
        // Note: Small race window here, but worst case we report drops on next iteration
        frames_dropped -= dropped;
        ESP_LOGW(TAG, "Queue overflow: %d frame(s) dropped", dropped);
      }

      // Read volatile write index once to avoid multiple volatile reads
      uint8_t write_idx = process_queue_write_;

      // Process all pending frames in queue
      while (process_queue_read_ != write_idx)
      {
        ESP_LOGV(TAG, "Reading queue location %d", process_queue_read_);
        uint32_t frame_to_process = process_queue_[process_queue_read_];
        process_queue_read_ = (process_queue_read_ + 1) & QUEUE_MASK;
        process_frame(frame_to_process);
      }

      // Check if we need to send a trigger pulse
      process_trigger();
    }

    void DcBlueComponent::process_frame(uint32_t frame)
    {
      uint8_t command = (frame >> 16) & 0xFF;
      uint8_t state_byte = (frame >> 8) & 0xFF;
      uint8_t checksum = frame & 0xFF;

      // Expected checksum for known commands: LSB must be set (checksum = state_byte | 0x01)
      uint8_t expected_checksum = state_byte | 0x01;

      if (command == protocol::CMD_DOOR_STATE)
      {
        if (checksum != expected_checksum)
        {
          ESP_LOGW(TAG, "Invalid door frame checksum: %08X (expected %02X)", frame, expected_checksum);
          return;
        }

        // Bit 2 indicates AC power (1=AC, 0=battery)
        bool ac_power = (state_byte & protocol::FLAG_AC_POWER) != 0;
        uint8_t door_state = state_byte & ~protocol::FLAG_AC_POWER;

        this->process_battery_event(!ac_power);

        switch (door_state)
        {
        case protocol::DOOR_CLOSED:
          ESP_LOGD(TAG, "Door closed%s", ac_power ? "" : " - battery");
          this->process_door_closed_event();
          break;
        case protocol::DOOR_MOVING:
          ESP_LOGD(TAG, "Opening/Closing%s", ac_power ? "" : " - battery");
          this->process_motor_running_event();
          break;
        case protocol::DOOR_OPEN:
          ESP_LOGD(TAG, "Door open%s", ac_power ? "" : " - battery");
          this->process_door_open_event();
          break;
        default:
          ESP_LOGW(TAG, "Unknown door state: %02X (frame: %08X)", door_state, frame);
        }
      }
      else if (command == protocol::CMD_LIGHT_LOCK)
      {
        if (checksum != expected_checksum)
        {
          ESP_LOGW(TAG, "Invalid light/lock frame checksum: %08X (expected %02X)", frame, expected_checksum);
          return;
        }

        switch (state_byte)
        {
        case protocol::LIGHT_ON:
          ESP_LOGD(TAG, "Light on");
          this->process_light_event(true);
          break;
        case protocol::LIGHT_OFF:
          ESP_LOGD(TAG, "Light off");
          this->process_light_event(false);
          break;
        case protocol::STRIKE_LOCK:
          ESP_LOGD(TAG, "Strike lock");
          break;
        case protocol::MAGNETIC_LOCK:
          ESP_LOGD(TAG, "Magnetic lock");
          break;
        default:
          ESP_LOGW(TAG, "Unknown light/lock state: %02X (frame: %08X)", state_byte, frame);
        }
      }
      else
      {
        // Unknown command - log for analysis (checksum algorithm unknown)
        ESP_LOGW(TAG, "Unknown command: %02X data: %02X checksum: %02X (frame: %08X)",
                 command, state_byte, checksum, frame);
      }
    }

    void DcBlueComponent::process_door_open_event()
    {
      this->process_door_state_change_event(cover::COVER_OPEN, cover::COVER_OPERATION_CLOSING);
    }

    void DcBlueComponent::process_door_closed_event()
    {
      this->process_door_state_change_event(cover::COVER_CLOSED, cover::COVER_OPERATION_OPENING);
    }

    void DcBlueComponent::process_door_state_change_event(float position, cover::CoverOperation next_direction)
    {
      next_direction_ = next_direction;

      if (this->garage_cover_sensor_ == nullptr)
      {
        return;
      }

      if (garage_cover_sensor_->position != position ||
          garage_cover_sensor_->current_operation != cover::COVER_OPERATION_IDLE)
      {
        garage_cover_sensor_->position = position;
        garage_cover_sensor_->current_operation = cover::COVER_OPERATION_IDLE;
        garage_cover_sensor_->publish_state();
      }
    }

    void DcBlueComponent::process_motor_running_event()
    {
      if (this->garage_cover_sensor_ == nullptr)
      {
        return;
      }

      if (garage_cover_sensor_->current_operation != next_direction_)
      {
        garage_cover_sensor_->current_operation = next_direction_;
        garage_cover_sensor_->publish_state();
      }
    }

    void DcBlueComponent::process_battery_event(bool on_battery)
    {
      if (this->ac_power_binary_sensor_ == nullptr)
      {
        return;
      }

      // ac_power sensor should be true when NOT on battery
      bool ac_power = !on_battery;
      if (this->ac_power_binary_sensor_->state != ac_power)
      {
        this->ac_power_binary_sensor_->publish_state(ac_power);
      }
    }

    void DcBlueComponent::process_light_event(bool light)
    {
      if (this->light_binary_sensor_ == nullptr)
      {
        return;
      }

      if (this->light_binary_sensor_->state != light)
      {
        this->light_binary_sensor_->publish_state(light);
      }
    }

    void DcBlueComponent::process_trigger()
    {
      if (this->trigger_pin_ == nullptr)
      {
        return;
      }

      uint32_t now = millis();

      switch (trigger_state_)
      {
      case TriggerState::IDLE:
        if (triggers_needed > 0)
        {
          ESP_LOGD(TAG, "Setting trigger pin");
          triggers_needed--;
          trigger_pin_->digital_write(1);
          trigger_state_ = TriggerState::PIN_HIGH;
          trigger_state_time_ = now;
        }
        break;

      case TriggerState::PIN_HIGH:
        if (now - trigger_state_time_ > this->trigger_period_)
        {
          ESP_LOGD(TAG, "Clearing trigger pin");
          trigger_pin_->digital_write(0);
          trigger_state_ = TriggerState::PIN_LOW_WAIT;
          trigger_state_time_ = now;
        }
        break;

      case TriggerState::PIN_LOW_WAIT:
        if (now - trigger_state_time_ > this->clear_period_)
        {
          trigger_state_ = TriggerState::IDLE;
        }
        break;
      }
    }

    void DcBlueComponent::dump_config()
    {
      ESP_LOGCONFIG(TAG, "DC Blue:");
      LOG_PIN("  Trigger Pin: ", this->trigger_pin_);
      LOG_PIN("  Data Pin: ", this->data_pin_);
      ESP_LOGCONFIG(TAG, "  Symbol Period: %d us", this->symbol_period_);
      ESP_LOGCONFIG(TAG, "  Inverted: %s", this->inverted_ ? "true" : "false");
      ESP_LOGCONFIG(TAG, "  Trigger period: %d ms", this->trigger_period_);
      ESP_LOGCONFIG(TAG, "  Clear period: %d ms", this->clear_period_);
    }

  } // namespace dc_blue
} // namespace esphome
