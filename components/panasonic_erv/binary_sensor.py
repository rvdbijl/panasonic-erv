import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_PROBLEM,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from . import CONF_PARENT, PanasonicERV

DEPENDENCIES = ["panasonic_erv"]
# Keep this order aligned with binary_ in C++. Connected means recent status;
# control_ready additionally requires fresh, supported settings (not a free
# queue slot). Fault retains its last readback during a communications outage.
SENSORS = {
    "connected": DEVICE_CLASS_CONNECTIVITY,
    "fault": DEVICE_CLASS_PROBLEM,
    "control_ready": "",
}
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_PARENT): cv.use_id(PanasonicERV),
        **{
            cv.Optional(key): binary_sensor.binary_sensor_schema(
                device_class=device_class, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            )
            for key, device_class in SENSORS.items()
        },
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT])
    for index, key in enumerate(SENSORS):
        if key in config:
            var = await binary_sensor.new_binary_sensor(config[key])
            cg.add(parent.set_binary_sensor(index, var))
