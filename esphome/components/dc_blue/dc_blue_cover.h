#pragma once

#include "esphome/components/cover/cover.h"
#include "esphome/components/cover/cover_traits.h"

namespace esphome
{
    namespace dc_blue
    {
        class DcBlueCover : public cover::Cover
        {

        public:
            void setup();
            cover::CoverTraits get_traits() override;

        protected:
            void control(const cover::CoverCall &call) override;
        };

    } // namespace dc_blue
} // namespace esphome