// Datasheet https://wiki.dfrobot.com/_A02YYUW_Waterproof_Ultrasonic_Sensor_SKU_SEN0311

#include "dc_blue.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <esp32-hal-timer.h>

namespace esphome
{
  namespace dc_blue
  {

    static const char *const TAG = "dc_blue";
    DcBlueComponent *instance = NULL;

    hw_timer_t *Timer0_Cfg = NULL;

    uint32_t header = 0xFFFFFFFF;
    uint32_t frame = 0;
    bool waiting_for_header = true;
    bool capturing_frame = false;
    int captured_bytes = 0;

    unsigned long ticks = 0;

    void IRAM_ATTR Timer0_ISR()
    {
      ticks++;
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
        instance->process_queue_write = instance->process_queue_write % (sizeof(instance->process_queue)/sizeof(instance->process_queue[0]));

        frame = 0;
      }
    }

    void IRAM_ATTR pinChangeIrq(hw_timer_t * timer) {
      // timerWrite(timer, sample_period/2);
    }

    void DcBlueComponent::setup()
    {
      instance = this;
      Timer0_Cfg = timerBegin(0, 80, true);

      if (data_pin_ != nullptr)
      {
        data_pin_->setup();
        // data_pin_->attach_interrupt(&pinChangeIrq, Timer0_Cfg, gpio::INTERRUPT_ANY_EDGE);
      }

      timerAttachInterrupt(Timer0_Cfg, &Timer0_ISR, true);
      timerAlarmWrite(Timer0_Cfg, this->symbol_period, true);
      timerAlarmEnable(Timer0_Cfg);
    }

    void DcBlueComponent::loop()
    {
      if (process_queue_read != process_queue_write)
      {
        ESP_LOGD(TAG, "Reading queue location %d", process_queue_read);
        process_frame(process_queue[process_queue_read]);
        process_queue_read++;
        process_queue_read = process_queue_read % (sizeof(instance->process_queue)/sizeof(instance->process_queue[0]));
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
