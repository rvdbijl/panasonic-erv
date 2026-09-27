import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select

from . import CONF_PARENT, PanasonicERV, ns

DEPENDENCIES = ["panasonic_erv"]
ModeSelect = ns.class_("ModeSelect", select.Select)
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_PARENT): cv.use_id(PanasonicERV),
        cv.Required("mode"): select.select_schema(ModeSelect, icon="mdi:fan"),
    }
)


# Option order must match ModeSelect::control()'s action array in C++.
# Low/High cancel Boost; Boost retains the underlying Low/High selection.
async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT])
    var = await select.new_select(config["mode"], options=["Standby", "Low", "High", "Boost"])
    cg.add(var.set_parent(parent))
    cg.add(parent.set_mode_select(var))
