import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from . import DcBlueComponent
from .constants import (
    CONF_DC_BLUE_ID,
    CONF_LIGHT,
    CONF_AC_POWER,
)

DEPENDENCIES = ["dc_blue", "binary_sensor"]


CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_LIGHT): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_AC_POWER): binary_sensor.binary_sensor_schema(),
    }
).extend(
    {
        cv.GenerateID(CONF_DC_BLUE_ID): cv.use_id(DcBlueComponent),
    }
)


async def to_code(config):
    platform = await cg.get_variable(config[CONF_DC_BLUE_ID])

    if light_config := config.get(CONF_LIGHT):
        sens = await binary_sensor.new_binary_sensor(light_config)
        cg.add(platform.set_light_binary_sensor(sens))
    
    if ac_power_config := config.get(CONF_AC_POWER):
        sens = await binary_sensor.new_binary_sensor(ac_power_config)
        cg.add(platform.set_ac_power_binary_sensor(sens))
