#pragma once

#include <atomic>
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
    // Protocol constants
    namespace protocol
    {
      // Command bytes (first byte of frame)
      static constexpr uint8_t CMD_DOOR_STATE = 0x2C;
      static constexpr uint8_t CMD_LIGHT_LOCK = 0x55;

      // Door state values (after masking out AC bit)
      static constexpr uint8_t DOOR_CLOSED = 0x20;
      static constexpr uint8_t DOOR_MOVING = 0x08;
      static constexpr uint8_t DOOR_OPEN = 0x02;

      // Door state flags
      static constexpr uint8_t FLAG_AC_POWER = 0x04;

      // Light/lock state values
      static constexpr uint8_t LIGHT_ON = 0x13;
      static constexpr uint8_t LIGHT_OFF = 0x15;
      static constexpr uint8_t STRIKE_LOCK = 0x0B;
      static constexpr uint8_t MAGNETIC_LOCK = 0x0D;
    } // namespace protocol

    class DcBlueComponent : public Component
    {

    public:
      SUB_BINARY_SENSOR(open)     // CONF_OPEN
      SUB_BINARY_SENSOR(closed)   // CONF_CLOSED
      SUB_BINARY_SENSOR(running)  // CONF_RUNNING
      SUB_BINARY_SENSOR(light)    // CONF_LIGHT
      SUB_BINARY_SENSOR(ac_power) // CONF_AC_POWER

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
      void set_trigger_period(uint32_t trigger_period) { this->trigger_period_ = trigger_period; }
      void set_clear_period(uint32_t clear_period) { this->clear_period_ = clear_period; }

      void process_door_open_event();
      void process_door_closed_event();
      void process_door_state_change_event(float position, cover::CoverOperation next_direction);
      void process_motor_running_event();
      void process_battery_event(bool battery);
      void process_light_event(bool light);

      InternalGPIOPin *data_pin_{nullptr};
      InternalGPIOPin *trigger_pin_{nullptr};
      bool inverted_ = false;

      // Queue for passing frames from ISR to main loop
      // Size must be power of 2 for efficient masking in ISR
      static constexpr uint8_t QUEUE_SIZE = 4;
      static_assert((QUEUE_SIZE & (QUEUE_SIZE - 1)) == 0, "QUEUE_SIZE must be power of 2");
      uint32_t process_queue_[QUEUE_SIZE] = {0};
      volatile uint8_t process_queue_write_ = 0; // Written by ISR, read by loop()
      volatile uint8_t process_queue_read_ = 0;  // Written by loop(), read by ISR for overflow check

    protected:
      void process_frame(uint32_t);
      void process_trigger();

      DcBlueCover *garage_cover_sensor_{nullptr};
      cover::CoverOperation next_direction_{cover::COVER_OPERATION_OPENING};

      int symbol_period_ = 900;        // us
      uint32_t trigger_period_ = 1000; // ms
      uint32_t clear_period_ = 1000;   // ms

      std::atomic<int> triggers_needed{0}; // Modified by DcBlueCover, read by process_trigger()

      // Trigger pulse state machine
      enum class TriggerState : uint8_t
      {
        IDLE,
        PIN_HIGH,
        PIN_LOW_WAIT
      };
      TriggerState trigger_state_ = TriggerState::IDLE;
      uint32_t trigger_state_time_ = 0;
    };

  } // namespace dc_blue
} // namespace esphome
