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
      digitalWrite(TIMER_PIN, !digitalRead(TIMER_PIN));

      timer_isr_calls++;
      if (timer_isr_calls % 2 != 1)
      {
        return;
      }

      digitalWrite(DEBUG_PIN, !digitalRead(DEBUG_PIN));

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
      timer_isr_calls = 0;
      timerRestart(timer);
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

      pinMode(DEBUG_PIN, OUTPUT);
      pinMode(TIMER_PIN, OUTPUT);
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
    }

    void DcBlueComponent::process_frame(uint32_t frame)
    {
      ESP_LOGD(TAG, "Frame received: %08X", frame);
    }

    void DcBlueComponent::dump_config()
    {
      ESP_LOGCONFIG(TAG, "DC Blue:");
      LOG_PIN("  Data Pin: ", this->data_pin_);
      ESP_LOGCONFIG(TAG, "  Symbol Period: %d", this->symbol_period);
    }

  } // namespace dc_blue
} // namespace esphome
