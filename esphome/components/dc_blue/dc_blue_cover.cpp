#include "esphome/core/log.h"
#include "dc_blue_cover.h"

namespace esphome
{
    namespace dc_blue
    {

        static const char *const TAG = "dc_blue";

        void DcBlueCover::setup()
        {
            ESP_LOGD(TAG, "Assuming door CLOSED and IDLE");
            this->position = cover::COVER_CLOSED;
            this->current_operation = cover::COVER_OPERATION_IDLE;
            this->publish_state();
        }

        cover::CoverTraits DcBlueCover::get_traits()
        {
            auto traits = cover::CoverTraits();
            traits.set_supports_stop(true);
            traits.set_supports_position(false);
            traits.set_supports_toggle(true);
            traits.set_supports_tilt(false);
            traits.set_is_assumed_state(false);
            return traits;
        }

        void DcBlueCover::control(const cover::CoverCall &call)
        {
            if (call.get_stop())
            {
                ESP_LOGD(TAG, "Got stop command");
                // this->start_direction_(COVER_OPERATION_IDLE);
            }
            else if (call.get_toggle().has_value())
            {
                ESP_LOGD(TAG, "Got toggle command");
                // toggle action logic: OPEN - STOP - CLOSE
                // if (this->last_command_ != COVER_OPERATION_IDLE)
                // {
                //     this->start_direction_(COVER_OPERATION_IDLE);
                // }
                // else
                // {
                //     this->toggles_needed_++;
                // }
            }
        }
    } // namespace dc_blue
} // namespace esphome