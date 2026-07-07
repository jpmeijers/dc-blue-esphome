import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover
from . import dc_blue_component_ns, DcBlueComponent
from esphome.const import CONF_ID
from .constants import (
    CONF_DC_BLUE_ID,
)

DEPENDENCIES = ["dc_blue", "cover"]

DcBlueCover = dc_blue_component_ns.class_("DcBlueCover", cover.Cover, cg.Component)

CONFIG_SCHEMA = cover.cover_schema(DcBlueCover).extend(
    {
        cv.GenerateID(CONF_DC_BLUE_ID): cv.use_id(DcBlueComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cover.register_cover(var, config)

    parent = await cg.get_variable(config[CONF_DC_BLUE_ID])
    cg.add(parent.set_cover(var))
