import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.cpp_helpers import gpio_pin_expression
from esphome.components import uart
from esphome.const import (
    CONF_DATA_PIN,
    CONF_ID,
)
from esphome.const import CONF_ID
from esphome import pins

CODEOWNERS = ["@jpmeijers"]
DEPENDENCIES = ["binary_sensor"]

dc_blue_component_ns = cg.esphome_ns.namespace("dc_blue")
DcBlueComponent = dc_blue_component_ns.class_("DcBlueComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DcBlueComponent),
        cv.Required(CONF_DATA_PIN): pins.gpio_input_pin_schema,
        cv.Optional("symbol_period", default=900): cv.int_range(min=100, max=10000),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pin = await gpio_pin_expression(config[CONF_DATA_PIN])
    cg.add(var.set_data_pin(pin))

    if "symbol_period" in config:
        cg.add(var.set_symbol_period(config["symbol_period"]))
