import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
)

from . import CONF_PARENT, PanasonicERV

DEPENDENCIES = ["panasonic_erv"]
SENSORS = {
    "sa_flow": dict(
        unit_of_measurement="CFM",
        accuracy_decimals=0,
        icon="mdi:fan",
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "ea_flow": dict(
        unit_of_measurement="CFM",
        accuracy_decimals=0,
        icon="mdi:fan",
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "indoor_temperature": dict(
        unit_of_measurement="°C",
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "outdoor_temperature": dict(
        unit_of_measurement="°C",
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "indoor_humidity": dict(
        unit_of_measurement="%",
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_HUMIDITY,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "outdoor_humidity": dict(
        unit_of_measurement="%",
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_HUMIDITY,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "power": dict(
        unit_of_measurement="W",
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "valid_frames": dict(
        accuracy_decimals=0,
        state_class=STATE_CLASS_TOTAL_INCREASING,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    "rejected_frames": dict(
        accuracy_decimals=0,
        state_class=STATE_CLASS_TOTAL_INCREASING,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_PARENT): cv.use_id(PanasonicERV),
        **{cv.Optional(key): sensor.sensor_schema(**options) for key, options in SENSORS.items()},
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT])
    for index, key in enumerate(SENSORS):
        if key in config:
            var = await sensor.new_sensor(config[key])
            cg.add(parent.set_sensor(index, var))
