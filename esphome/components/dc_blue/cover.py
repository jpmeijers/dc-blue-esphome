import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover, binary_sensor
from esphome.components.dc_blue import DcBlueComponent
from esphome.components.dc_blue.constants import (
    CONF_DC_BLUE_ID,
)
from esphome.const import (
    CONF_ID,
)

import esphome.config_validation as cv

DEPENDENCIES = ["dc_blue", "cover"]


CONFIG_SCHEMA = cover.COVER_SCHEMA.extend(
    {
        cv.GenerateID(CONF_DC_BLUE_ID): cv.use_id(DcBlueComponent),
    }
)


async def to_code(config):
    platform = await cg.get_variable(config[CONF_DC_BLUE_ID])
    
    sens = platform.create_garage_cover_sensor()
    # sens = cg.new_Pvariable(config[CONF_ID])
    await cover.register_cover(sens, config)
    # cg.add(platform.set_garage_cover_sensor(sens))
