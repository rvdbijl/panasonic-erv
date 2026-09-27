"""Panasonic FV-16VEC1S UART integration; external component entry point."""

from copy import deepcopy

import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome import automation
from esphome.components import uart
from esphome.components.esp32 import gpio as esp32_gpio
from esphome.const import CONF_ID, CONF_RX_PIN, CONF_UART_ID
from esphome.core import CORE

CODEOWNERS = ["@rvdbijl"]
DEPENDENCIES = ["uart", "esp32"]
# The hub's C++ header references these entity classes even when a user creates
# no entities. AUTO_LOAD supplies their types; it does not create entities.
AUTO_LOAD = ["sensor", "binary_sensor", "text_sensor", "select", "number", "switch"]
MULTI_CONF = True
CONF_PARENT = "panasonic_erv_id"
ns = cg.esphome_ns.namespace("panasonic_erv")
PanasonicERV = ns.class_("PanasonicERV", cg.Component, uart.UARTDevice)
SetPresetsAction = ns.class_("SetPresetsAction", automation.Action)
# Shared index contract with PresetNumber and Controller::request_field():
# public order is SA/EA within each mode, unlike the ERV's EA/SA wire order.
PRESETS = ("low_sa", "low_ea", "high_sa", "high_ea", "boost_sa", "boost_ea")


# Leave at least two transmission intervals before declaring status or command
# timeout. The Controller also independently enforces a 5-second write freshness.
def validate_timing(config):
    interval = config["poll_interval"].total_milliseconds
    if config["status_timeout"].total_milliseconds < 2 * interval:
        raise cv.Invalid("status_timeout must be at least twice poll_interval")
    if config["command_timeout"].total_milliseconds < 2 * interval:
        raise cv.Invalid("command_timeout must be at least twice poll_interval")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PanasonicERV),
            cv.Optional("poll_interval", default="1s"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=cv.TimePeriod(milliseconds=1000),
                    max=cv.TimePeriod(milliseconds=5000),
                ),
            ),
            cv.Optional("status_timeout", default="10s"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(max=cv.TimePeriod(seconds=60)),
            ),
            cv.Optional("command_timeout", default="8s"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(max=cv.TimePeriod(seconds=60)),
            ),
            cv.Optional("rx_pull_down", default=True): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA),
    cv.only_on_esp32,
    cv.require_esphome_version(2026, 9, 0),
    validate_timing,
)
# Standard UART final validation checks the referenced bus and reserves RX/TX
# use so two consumers cannot compete for the same stream. GPIO numbers and
# inversion remain ordinary uart: options instead of library constants.
_UART_VALIDATE = uart.final_validate_device_schema(
    "panasonic_erv",
    baud_rate=4800,
    require_tx=True,
    require_rx=True,
    data_bits=8,
    parity="EVEN",
    stop_bits=1,
)


def final_validate(config):
    _UART_VALIDATE(config)
    full = fv.full_config.get()
    hub = full.get_config_for_path(full.get_path_for_id(config[CONF_UART_ID])[:-1])
    if str(hub[CONF_ID].type) != str(uart.IDFUARTComponent):
        raise cv.Invalid("panasonic_erv requires a dedicated native ESP32 UART")
    # Check the extra electrical requirement imposed by setup(). Work on a
    # copy: validation must not mutate the shared UART pin configuration.
    if config["rx_pull_down"]:
        pin = deepcopy(hub[CONF_RX_PIN])
        pin["mode"]["pulldown"] = True
        pin["mode"]["pullup"] = False
        esp32_gpio.validate_supports(pin)
    return config


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    # Reuse the configured UART's pin object: no second, hard-coded GPIO setting.
    hub = next(item for item in CORE.config["uart"] if item[CONF_ID] == config[CONF_UART_ID])
    pin = await cg.get_variable(hub[CONF_RX_PIN][CONF_ID])
    cg.add(var.set_rx_gpio(pin))
    # Keep a Component pointer as well as UARTDevice's transport pointer so
    # the C++ adapter can detect a failed UART setup before trying writes.
    parent = await cg.get_variable(config[CONF_UART_ID])
    cg.add(var.set_uart_component(parent))
    for name in ("poll_interval", "status_timeout", "command_timeout", "rx_pull_down"):
        cg.add(getattr(var, "set_" + name)(config[name]))


# All six targets form one save intent. Lambdas are allowed, so range/integer
# checks also run in C++ after values are evaluated, before any wire encoding.
@automation.register_action(
    "panasonic_erv.set_presets",
    SetPresetsAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(PanasonicERV),
            **{cv.Required(key): cv.templatable(cv.int_range(min=30, max=160)) for key in PRESETS},
        }
    ),
    synchronous=True,
)
async def set_presets_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    for key in PRESETS:
        value = await cg.templatable(config[key], args, cg.float_)
        cg.add(getattr(var, "set_" + key)(value))
    return var
