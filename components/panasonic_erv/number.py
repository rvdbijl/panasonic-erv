import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import ENTITY_CATEGORY_CONFIG

from . import CONF_PARENT, PRESETS, PanasonicERV, ns

DEPENDENCIES = ["panasonic_erv"]
PresetNumber = ns.class_("PresetNumber", number.Number)
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_PARENT): cv.use_id(PanasonicERV),
        **{
            cv.Optional(key): number.number_schema(
                PresetNumber,
                unit_of_measurement="CFM",
                icon="mdi:fan",
                entity_category=ENTITY_CATEGORY_CONFIG,
            )
            for key in PRESETS
        },
    }
)


# PRESETS order is shared with C++ field indexes. These expose saved targets,
# not measured flow; the parent publishes readback after command processing.
async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT])
    for index, key in enumerate(PRESETS):
        if key in config:
            var = await number.new_number(config[key], min_value=30, max_value=160, step=1)
            cg.add(var.set_parent(parent))
            cg.add(var.set_field(index))
            cg.add(parent.set_preset_number(index, var))
