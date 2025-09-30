#include "dc_blue.h" 
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "driver/timer.h"

#define DEBUG_PIN 32
#define TIMER_PIN 25

namespace esphome {
  namespace dc_blue {

    static const char *const TAG = "dc_blue";
    DcBlueComponent *instance = NULL;

    // Instead of hw_timer_t*
    static const timer_group_t TIMER_GROUP = TIMER_GROUP_0;
    static const timer_idx_t   TIMER_NUM   = TIMER_0;

    volatile uint32_t header = 0xFFFFFFFF;
    volatile uint32_t frame = 0;
    volatile bool waiting_for_header = true;
    volatile bool capturing_frame = false;
    volatile int captured_bytes = 0;
    volatile int timer_isr_calls = 0;

    bool IRAM_ATTR Timer0_ISR(void *args) {
      timer_isr_calls++;
      if (timer_isr_calls % 2 != 1) {
        return false;
      }

      bool value = instance->data_pin_->digital_read();
      if (instance->inverted_) {
        value = !value;
      }

      if (waiting_for_header) {
        header = (header << 1) | value;
        if (header == 0x01) {
          header = 0xFFFFFFFF;
          waiting_for_header = false;
          capturing_frame = true;
          captured_bytes = 0;
        }
        return false;
      }

      if (capturing_frame) {
        frame = (frame << 1) | value;
        captured_bytes++;
      }

      if (captured_bytes == 24) {
        waiting_for_header = true;
        capturing_frame = false;
        captured_bytes = 0;

        instance->process_queue[instance->process_queue_write] = frame;
        instance->process_queue_write++;
        instance->process_queue_write %= (sizeof(instance->process_queue) / sizeof(instance->process_queue[0]));
        frame = 0;
      }
      return false;  // no context switch
    }

    void IRAM_ATTR pinChangeIrq(void *arg) {
      if (waiting_for_header) {
        timer_isr_calls = 0;
        timer_set_counter_value(TIMER_GROUP, TIMER_NUM, 0);  // ✅ reset counter
      }
    }

    void DcBlueComponent::setup() {
      instance = this;

      // Configure timer
      timer_config_t config = {
        .divider = 80,                 // 1 tick = 1 µs (80 MHz / 80)
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = true,
      };
      timer_init(TIMER_GROUP, TIMER_NUM, &config);
      timer_set_counter_value(TIMER_GROUP, TIMER_NUM, 0);
      timer_set_alarm_value(TIMER_GROUP, TIMER_NUM, this->symbol_period_ / 2);
      timer_enable_intr(TIMER_GROUP, TIMER_NUM);
      timer_isr_callback_add(TIMER_GROUP, TIMER_NUM, &Timer0_ISR, nullptr, 0);
      timer_start(TIMER_GROUP, TIMER_NUM);

      // Data pin with IRQ
      if (data_pin_ != nullptr) {
        data_pin_->setup();
        data_pin_->pin_mode(gpio::FLAG_INPUT);
        data_pin_->attach_interrupt(&pinChangeIrq, nullptr, gpio::INTERRUPT_ANY_EDGE);
      }

      if (trigger_pin_ != nullptr) {
        trigger_pin_->setup();
        trigger_pin_->pin_mode(gpio::FLAG_OUTPUT);
        trigger_pin_->digital_write(0);
      }

      this->garage_cover_sensor_->setup();

      ESP_LOGD(TAG, "Assume Light off");
      if (this->light_binary_sensor_ != nullptr) {
        this->light_binary_sensor_->publish_state(false);
      }

      ESP_LOGD(TAG, "Assume running on AC mains power");
      if (this->ac_power_binary_sensor_ != nullptr) {
        this->ac_power_binary_sensor_->publish_state(true);
      }
    }
  }
}

