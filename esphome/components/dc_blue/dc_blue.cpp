#include "dc_blue.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <esp32-hal-timer.h>

#define DEBUG_PIN 32
#define TIMER_PIN 25

namespace esphome
{
  namespace dc_blue
  {

    static const char *const TAG = "dc_blue";
    DcBlueComponent *instance = NULL;

    hw_timer_t *Timer0_Cfg = NULL;

    volatile uint32_t header = 0xFFFFFFFF;
    volatile uint32_t frame = 0;
    volatile bool waiting_for_header = true;
    volatile bool capturing_frame = false;
    volatile int captured_bytes = 0;
    volatile int timer_isr_calls = 0;

    void IRAM_ATTR Timer0_ISR()
    {
      // digitalWrite(TIMER_PIN, !digitalRead(TIMER_PIN));

      timer_isr_calls++;
      if (timer_isr_calls % 2 != 1)
      {
        return;
      }

      // digitalWrite(DEBUG_PIN, !digitalRead(DEBUG_PIN));

      bool value = instance->data_pin_->digital_read();

      if (waiting_for_header)
      {
        header = header << 1 | value;
        if (header == 0x01)
        {
          header = 0xFFFFFFFF;
          waiting_for_header = false;
          capturing_frame = true;
          captured_bytes = 0;
        }
        return;
      }

      if (capturing_frame)
      {
        frame = frame << 1 | value;
        captured_bytes++;
      }

      if (captured_bytes == 24)
      {
        waiting_for_header = true;
        capturing_frame = false;
        captured_bytes = 0;

        instance->process_queue[instance->process_queue_write] = frame;
        instance->process_queue_write++;
        instance->process_queue_write = instance->process_queue_write % (sizeof(instance->process_queue) / sizeof(instance->process_queue[0]));

        frame = 0;
      }
    }

    void IRAM_ATTR pinChangeIrq(hw_timer_t *timer)
    {
      if (waiting_for_header)
      {
        timer_isr_calls = 0;
        timerRestart(timer);
      }
    }

    void DcBlueComponent::setup()
    {
      instance = this;
      Timer0_Cfg = timerBegin(0, 80, true);

      if (data_pin_ != nullptr)
      {
        data_pin_->setup();
        data_pin_->pin_mode(gpio::FLAG_INPUT);
        data_pin_->attach_interrupt(&pinChangeIrq, Timer0_Cfg, gpio::INTERRUPT_ANY_EDGE);
      }

      timerAttachInterrupt(Timer0_Cfg, &Timer0_ISR, true);
      timerAlarmWrite(Timer0_Cfg, this->symbol_period / 2, true);
      timerAlarmEnable(Timer0_Cfg);

      // pinMode(DEBUG_PIN, OUTPUT);
      // pinMode(TIMER_PIN, OUTPUT);

      this->garage_cover_sensor_->setup();

      ESP_LOGD(TAG, "Assume Light off");
      if (this->light_binary_sensor_ != nullptr)
      {
        this->light_binary_sensor_->publish_state(false);
      }
    }

    void DcBlueComponent::loop()
    {
      if (process_queue_read != process_queue_write)
      {
        ESP_LOGD(TAG, "Reading queue location %d", process_queue_read);
        process_frame(process_queue[process_queue_read]);
        process_queue_read++;
        process_queue_read = process_queue_read % (sizeof(instance->process_queue) / sizeof(instance->process_queue[0]));
      }

      // Check if we need to send a trigger pulse
      process_trigger();
    }

    void DcBlueComponent::process_frame(uint32_t frame)
    {
      switch (frame)
      {
      case 0x002C2425:
        ESP_LOGD(TAG, "Door closed");

        next_direction_ = cover::COVER_OPERATION_OPENING;

        if (this->garage_cover_sensor_ != nullptr)
        {
          if (garage_cover_sensor_->position != cover::COVER_CLOSED ||
              garage_cover_sensor_->current_operation != cover::COVER_OPERATION_IDLE)
          {
            garage_cover_sensor_->position = cover::COVER_CLOSED;
            garage_cover_sensor_->current_operation = cover::COVER_OPERATION_IDLE;
            garage_cover_sensor_->publish_state();
          }
        }
        else
        {
          ESP_LOGD(TAG, "garage_cover is null");
        }
        break;
      case 0x002C0C0D:
        ESP_LOGD(TAG, "Opening/Closing");

        if (this->garage_cover_sensor_ != nullptr)
        {
          if (garage_cover_sensor_->current_operation != next_direction_)
          {
            garage_cover_sensor_->current_operation = next_direction_;
            garage_cover_sensor_->publish_state();
          }
        }
        else
        {
          ESP_LOGD(TAG, "garage_cover is null");
        }
        break;
      case 0x002C0607:
        ESP_LOGD(TAG, "Door open");

        next_direction_ = cover::COVER_OPERATION_CLOSING;

        if (this->garage_cover_sensor_ != nullptr)
        {
          if (garage_cover_sensor_->position != cover::COVER_OPEN ||
              garage_cover_sensor_->current_operation != cover::COVER_OPERATION_IDLE)
          {
            garage_cover_sensor_->position = cover::COVER_OPEN;
            garage_cover_sensor_->current_operation = cover::COVER_OPERATION_IDLE;
            garage_cover_sensor_->publish_state();
          }
        }
        else
        {
          ESP_LOGD(TAG, "garage_cover is null");
        }
        break;

      case 0x00551313:
        ESP_LOGD(TAG, "Light on");
        if (this->light_binary_sensor_ != nullptr)
        {
          this->light_binary_sensor_->publish_state(true);
        }
        else
        {
          ESP_LOGD(TAG, "light_binary_sensor is null");
        }
        break;
      case 0x00551515:
        ESP_LOGD(TAG, "Light off");
        if (this->light_binary_sensor_ != nullptr)
        {
          this->light_binary_sensor_->publish_state(false);
        }
        else
        {
          ESP_LOGD(TAG, "light_binary_sensor is null");
        }
        break;

      case 0x00550B0B:
        ESP_LOGD(TAG, "Unlock");
        break;

      default:
        ESP_LOGD(TAG, "Unknown frame received: %08X", frame);
      }
    }

    void DcBlueComponent::process_trigger()
    {

      if (triggers_needed > 0 && !pin_set && !pin_cleared)
      {
        triggers_needed--;
        trigger_pin_->digital_write(1);
        pin_set = true;
        pin_set_time = millis();
      }

      if (pin_set && (millis() - pin_set_time > trigger_period))
      {
        trigger_pin_->digital_write(0);
        pin_set = false;
        pin_cleared = true;
        pin_cleared_time = millis();
      }

      if (pin_cleared && (millis() - pin_cleared_time > clear_period))
      {
        pin_cleared = false;
      }
    }

    void DcBlueComponent::dump_config()
    {
      ESP_LOGCONFIG(TAG, "DC Blue:");
      LOG_PIN("  Data Pin: ", this->data_pin_);
      ESP_LOGCONFIG(TAG, "  Symbol Period: %d", this->symbol_period);
    }

  } // namespace dc_blue
} // namespace esphome
