import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.cpp_helpers import gpio_pin_expression
from esphome.const import (
    CONF_DATA_PIN,
    CONF_TRIGGER_PIN,
    CONF_ID,
    CONF_INVERTED,
)
from esphome import pins
from .constants import (
    CONF_SYMBOL_PERIOD,
    CONF_TRIGGER_PERIOD,
    CONF_CLEAR_PERIOD,
)

CODEOWNERS = ["@jpmeijers"]

dc_blue_component_ns = cg.esphome_ns.namespace("dc_blue")
DcBlueComponent = dc_blue_component_ns.class_("DcBlueComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DcBlueComponent),
        cv.Required(CONF_DATA_PIN): pins.gpio_input_pin_schema,
        cv.Required(CONF_TRIGGER_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_SYMBOL_PERIOD, default=970): cv.int_range(min=100, max=10000),
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
        cv.Optional(CONF_TRIGGER_PERIOD, default=1000): cv.int_range(min=100, max=10000),
        cv.Optional(CONF_CLEAR_PERIOD, default=1000): cv.int_range(min=100, max=10000),
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pin = await gpio_pin_expression(config[CONF_DATA_PIN])
    cg.add(var.set_data_pin(pin))

    pin = await gpio_pin_expression(config[CONF_TRIGGER_PIN])
    cg.add(var.set_trigger_pin(pin))

    cg.add(var.set_symbol_period(config[CONF_SYMBOL_PERIOD]))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_trigger_period(config[CONF_TRIGGER_PERIOD]))
    cg.add(var.set_clear_period(config[CONF_CLEAR_PERIOD]))
