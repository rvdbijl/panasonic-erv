import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_RESTORE_MODE

from . import CONF_PARENT, PanasonicERV, ns

DEPENDENCIES = ["panasonic_erv"]
ERVSwitch = ns.class_("ERVSwitch", switch.Switch)
SCHEMA = switch.switch_schema(
    ERVSwitch, block_inverted=True, default_restore_mode="DISABLED"
).extend(
    {
        cv.Optional(CONF_RESTORE_MODE, default="DISABLED"): cv.enum(
            {"DISABLED": switch.RESTORE_MODES["DISABLED"]}, upper=True
        )
    }
)
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_PARENT): cv.use_id(PanasonicERV),
        cv.Optional("power"): SCHEMA,
        cv.Optional("boost"): SCHEMA,
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT])
    for index, key in enumerate(("power", "boost")):
        if key in config:
            var = await switch.new_switch(config[key])
            cg.add(var.set_parent(parent))
            cg.add(var.set_boost(index == 1))
            cg.add(parent.set_switch(index, var))
