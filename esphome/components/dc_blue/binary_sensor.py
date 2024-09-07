import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.components.dc_blue import DcBlueComponent
from esphome.components.dc_blue.constants import (
    CONF_DC_BLUE_ID,
    CONF_OPEN,
    CONF_CLOSED,
    CONF_RUNNING,
    CONF_LIGHT,
)
import esphome.config_validation as cv

DEPENDENCIES = ["dc_blue"]


CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_OPEN): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_CLOSED): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_RUNNING): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_LIGHT): binary_sensor.binary_sensor_schema(),
    }
).extend(
    {
        cv.GenerateID(CONF_DC_BLUE_ID): cv.use_id(DcBlueComponent),
    }
)


async def to_code(config):
    platform = await cg.get_variable(config[CONF_DC_BLUE_ID])

    if open_config := config.get(CONF_OPEN):
        sens = await binary_sensor.new_binary_sensor(open_config)
        cg.add(platform.set_open_binary_sensor(sens))

    if closed_config := config.get(CONF_CLOSED):
        sens = await binary_sensor.new_binary_sensor(closed_config)
        cg.add(platform.set_closed_binary_sensor(sens))

    if running_config := config.get(CONF_RUNNING):
        sens = await binary_sensor.new_binary_sensor(running_config)
        cg.add(platform.set_running_binary_sensor(sens))

    if light_config := config.get(CONF_LIGHT):
        sens = await binary_sensor.new_binary_sensor(light_config)
        cg.add(platform.set_light_binary_sensor(sens))
