import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from . import CONF_PARENT, PanasonicERV

DEPENDENCIES = ["panasonic_erv"]
# C++ text_ slots: the literal ERV fault code and the latest local transaction
# event. Command results describe queue/readback progress, not physical airflow.
SENSORS = ("fault_code", "command_result")
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_PARENT): cv.use_id(PanasonicERV),
        **{
            cv.Optional(key): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            )
            for key in SENSORS
        },
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT])
    for index, key in enumerate(SENSORS):
        if key in config:
            var = await text_sensor.new_text_sensor(config[key])
            cg.add(parent.set_text_sensor(index, var))
