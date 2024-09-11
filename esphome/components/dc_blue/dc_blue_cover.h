#pragma once

#include "esphome/components/cover/cover.h"
#include "esphome/components/cover/cover_traits.h"
#include "esphome/core/gpio.h"

namespace esphome
{
    namespace dc_blue
    {
        class DcBlueCover : public cover::Cover
        {

        public:
            void setup();
            cover::CoverTraits get_traits() override;
            void set_triggers_needed(int *triggers_needed) { this->triggers_needed_ = triggers_needed; }

        protected:
            void control(const cover::CoverCall &call) override;
            int *triggers_needed_;
        };

    } // namespace dc_blue
} // namespace esphome