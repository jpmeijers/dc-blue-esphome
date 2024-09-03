#pragma once

#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"

namespace esphome
{
  namespace dc_blue
  {

    class DcBlueComponent : public Component
    {

    public:
      // ========== INTERNAL METHODS ==========
      void setup() override;
      void loop() override;
      void dump_config() override;
      void set_data_pin(InternalGPIOPin *data_pin) { this->data_pin_ = data_pin; }
      void set_symbol_period(int period) { this->symbol_period = period; }
      static void interrupt_handler();
      InternalGPIOPin *data_pin_{nullptr};
      uint32_t process_queue[4];
      int process_queue_write = 0;
      int process_queue_read = 0;
      int symbol_period = 900;

    protected:
      void process_frame(uint32_t);
    };

  } // namespace dc_blue
} // namespace esphome
