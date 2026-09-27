#pragma once
#include "controller.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome {
namespace panasonic_erv {
class PanasonicERV;
// Thin ESPHome entity adapters submit requests to the hub. Only the hub's
// status callback publishes accepted state; these controls are not optimistic.
class ModeSelect : public select::Select {
public:
  void set_parent(PanasonicERV* p) { parent_ = p; }

protected:
  void control(size_t index) override;
  PanasonicERV* parent_{};
};
class PresetNumber : public number::Number {
public:
  void set_parent(PanasonicERV* p) { parent_ = p; }
  void set_field(uint8_t field) { field_ = field; }

protected:
  void control(float value) override;
  PanasonicERV* parent_{};
  uint8_t field_{};
};
class ERVSwitch : public switch_::Switch {
public:
  void set_parent(PanasonicERV* p) { parent_ = p; }
  void set_boost(bool boost) { boost_ = boost; }

protected:
  void write_state(bool state) override;
  PanasonicERV* parent_{};
  bool boost_{};
};
// Integration boundary: owns the pure Controller, borrows UART/GPIO/entity
// objects registered by ESPHome, and translates status/results into entities.
class PanasonicERV : public Component, public uart::UARTDevice, public Listener {
public:
  PanasonicERV() : controller_(this) {}
  void setup() override;
  void loop() override;
  void dump_config() override;
  // DATA runs after UART hardware setup, allowing our RX pull mode to be
  // applied without reinitializing pin routing or inversion.
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_poll_interval(uint32_t ms) { controller_.set_poll_interval(ms); }
  void set_status_timeout(uint32_t ms) { controller_.set_status_timeout(ms); }
  void set_command_timeout(uint32_t ms) { controller_.set_command_timeout(ms); }
  void set_uart_component(Component* component) { uart_component_ = component; }
  void set_rx_gpio(InternalGPIOPin* pin) { rx_gpio_ = pin; }
  void set_rx_pull_down(bool enabled) { rx_pull_down_ = enabled; }
  void set_sensor(uint8_t index, sensor::Sensor* s) { sensors_[index] = s; }
  void set_binary_sensor(uint8_t index, binary_sensor::BinarySensor* s) { binary_[index] = s; }
  void set_text_sensor(uint8_t index, text_sensor::TextSensor* s) { text_[index] = s; }
  void set_mode_select(ModeSelect* s) { mode_ = s; }
  void set_preset_number(uint8_t index, PresetNumber* n) { numbers_[index] = n; }
  void set_switch(uint8_t index, ERVSwitch* s) { switches_[index] = s; }
  // Submission APIs return queue admission, not eventual ERV confirmation.
  // Individual preset fields follow PRESETS order from __init__.py.
  bool request(replay::Action action);
  bool request_field(uint8_t field, float value);
  bool set_presets(float low_sa, float low_ea, float high_sa, float high_ea, float boost_sa,
                   float boost_ea);
  bool send_frame(const uint8_t* bytes, size_t size) override;
  void on_status(const protocol::Status& status, const uint8_t* raw) override;
  void on_result(uint32_t id, Result result) override;
  void on_connection(bool connected) override;

protected:
  void publish_controls_();
  void publish_diagnostics_();
  Controller controller_;
  Component* uart_component_{};
  InternalGPIOPin* rx_gpio_{};
  bool rx_pull_down_{true}, have_status_{false};
  protocol::Status last_status_{};
  uint8_t last_boost_{};
  uint32_t last_diagnostics_{};
  // Index contracts with the Python platform modules; keep both sides aligned.
  // sensors: SA flow, EA flow, indoor/outdoor temp, indoor/outdoor RH, watts,
  //          valid status frames, parser rejections.
  // binary: connected, fault, control_ready; text: fault_code, command_result.
  // numbers: Low SA/EA, High SA/EA, Boost SA/EA; switches: power, boost.
  // Null entries mean the corresponding optional entity was not configured.
  std::array<sensor::Sensor*, 9> sensors_{};
  std::array<binary_sensor::BinarySensor*, 3> binary_{};
  std::array<text_sensor::TextSensor*, 2> text_{};
  std::array<PresetNumber*, 6> numbers_{};
  std::array<ERVSwitch*, 2> switches_{};
  ModeSelect* mode_{};
};

// Evaluate YAML literals/lambdas at action execution, then submit one atomic
// preset request. The synchronous action returns without waiting for readback.
template <typename... Ts> class SetPresetsAction : public Action<Ts...> {
public:
  explicit SetPresetsAction(PanasonicERV* parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(float, low_sa)
  TEMPLATABLE_VALUE(float, low_ea)
  TEMPLATABLE_VALUE(float, high_sa)
  TEMPLATABLE_VALUE(float, high_ea)
  TEMPLATABLE_VALUE(float, boost_sa)
  TEMPLATABLE_VALUE(float, boost_ea)
  void play(const Ts&... x) override {
    parent_->set_presets(low_sa_.value(x...), low_ea_.value(x...), high_sa_.value(x...),
                         high_ea_.value(x...), boost_sa_.value(x...), boost_ea_.value(x...));
  }

protected:
  PanasonicERV* parent_;
};
} // namespace panasonic_erv
} // namespace esphome
