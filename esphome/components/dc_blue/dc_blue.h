#pragma once

#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/cover/cover.h"
#include "dc_blue_cover.h"

namespace esphome
{
  namespace dc_blue
  {

    class DcBlueComponent : public Component
    {

    public:
      SUB_BINARY_SENSOR(open)    // CONF_OPEN
      SUB_BINARY_SENSOR(closed)  // CONF_CLOSED
      SUB_BINARY_SENSOR(running) // CONF_RUNNING
      SUB_BINARY_SENSOR(light)   // CONF_LIGHT

      DcBlueCover *create_garage_cover_sensor()
      {
        this->garage_cover_sensor_ = new DcBlueCover();
        this->garage_cover_sensor_->set_triggers_needed(&this->triggers_needed);
        return this->garage_cover_sensor_;
      }

      // ========== INTERNAL METHODS ==========
      void setup() override;
      void loop() override;
      void dump_config() override;

      void set_data_pin(InternalGPIOPin *data_pin) { this->data_pin_ = data_pin; }
      void set_trigger_pin(InternalGPIOPin *trigger_pin) { this->trigger_pin_ = trigger_pin; }
      void set_symbol_period(int symbol_period) { this->symbol_period_ = symbol_period; }
      void set_inverted(bool inverted) { this->inverted_ = inverted; }
      void set_trigger_period(unsigned long trigger_period) { this->trigger_period_ = trigger_period; }
      void set_clear_period(unsigned long clear_period) { this->clear_period_ = clear_period; }

      static void interrupt_handler();
      InternalGPIOPin *data_pin_{nullptr};
      InternalGPIOPin *trigger_pin_{nullptr};
      bool inverted_ = false;

      uint32_t process_queue[4];
      int process_queue_write = 0;
      int process_queue_read = 0;

    protected:
      void process_frame(uint32_t);
      void process_trigger();

      DcBlueCover *garage_cover_sensor_{nullptr};
      cover::CoverOperation next_direction_{cover::COVER_OPERATION_OPENING};

      int symbol_period_ = 900;             // us
      unsigned long trigger_period_ = 1000; // ms
      unsigned long clear_period_ = 1000;

      int triggers_needed = 0;

      bool pin_set = false;
      unsigned long pin_set_time;
      unsigned long pin_cleared_time;
      bool pin_cleared = false;
    };

  } // namespace dc_blue
} // namespace esphome
