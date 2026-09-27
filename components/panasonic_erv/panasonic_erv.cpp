#include "panasonic_erv.h"
#include "esphome/core/log.h"
#include <cmath>
#include <driver/gpio.h>

namespace esphome {
namespace panasonic_erv {
static const char* const TAG = "panasonic_erv";
// YAML numbers and templated actions arrive as floats. Validate before casting
// so fractional/NaN/out-of-range values cannot silently become integer presets.
static bool valid_cfm(float value) {
  return std::isfinite(value) && value >= 30 && value <= 160 && std::floor(value) == value;
}
void PanasonicERV::setup() {
  if (uart_component_->is_failed()) {
    on_connection(false);
    mark_failed();
    return;
  }
  // UART setup can enable pull-up. Set only pull mode afterwards, preserving
  // the UART GPIO matrix routing and inversion. Pin comes from the chosen UART.
  if (gpio_set_pull_mode(static_cast<gpio_num_t>(rx_gpio_->get_pin()),
                         rx_pull_down_ ? GPIO_PULLDOWN_ONLY : GPIO_FLOATING) != ESP_OK) {
    ESP_LOGE(TAG, "Unable to configure UART RX pull mode");
    on_connection(false);
    mark_failed();
    return;
  }
  // Boot only establishes availability; it does not restore or send an ERV
  // operating state. Controller::tick() schedules polling from loop().
  on_connection(false);
  if (text_[1])
    // This initial message is replaced by control-request events, not by
    // polls. Connected/control_ready are the authoritative link diagnostics.
    text_[1]->publish_state("Idle; awaiting status");
}
void PanasonicERV::dump_config() {
  ESP_LOGCONFIG(TAG, "Panasonic FV-16VEC1S external component");
  ESP_LOGCONFIG(TAG, "  Dedicated 4800/8E1 UART; RX pull-down: %s",
                rx_pull_down_ ? "enabled" : "disabled");
  LOG_PIN("  RX pin: ", rx_gpio_);
}
void PanasonicERV::loop() {
  // Bound work if a disconnected/noisy line floods the UART.
  uint8_t byte;
  size_t budget = 512;
  while (budget-- && available() && read_byte(&byte))
    controller_.ingest(byte, millis());
  // Consume buffered readback before deciding whether to dispatch queued work.
  controller_.tick(millis());
  // Refresh readiness even when no new frames arrive, so it can age out.
  if (uint32_t(millis() - last_diagnostics_) >= 1000) {
    last_diagnostics_ = millis();
    publish_diagnostics_();
  }
}
bool PanasonicERV::send_frame(const uint8_t* bytes, size_t size) {
  if (uart_component_->is_failed() || !parent_->is_connected())
    return false;
  ESP_LOGD(TAG, "TX %s (%u bytes)", size == 12 ? "poll" : "write", static_cast<unsigned>(size));
  write_array(bytes, size);
  // The UART backend may block while writing. Avoid an additional flush;
  // Controller spaces transmissions >=1 second and verifies ERV readback.
  return !uart_component_->is_failed();
}
bool PanasonicERV::request(replay::Action action) { return controller_.request(action, millis()); }
bool PanasonicERV::request_field(uint8_t field, float value) {
  if (!valid_cfm(value)) {
    ESP_LOGW(TAG, "Rejected non-integer/out-of-range preset");
    publish_controls_();
    return false;
  }
  return controller_.request_field(field, static_cast<uint16_t>(value), millis());
}
bool PanasonicERV::set_presets(float low_sa, float low_ea, float high_sa, float high_ea,
                               float boost_sa, float boost_ea) {
  for (float value : {low_sa, low_ea, high_sa, high_ea, boost_sa, boost_ea})
    if (!valid_cfm(value)) {
      ESP_LOGW(TAG, "Invalid preset group");
      return false;
    }
  replay::Settings values{{static_cast<uint16_t>(low_sa), static_cast<uint16_t>(low_ea)},
                          {static_cast<uint16_t>(high_sa), static_cast<uint16_t>(high_ea)},
                          {static_cast<uint16_t>(boost_sa), static_cast<uint16_t>(boost_ea)}};
  return controller_.request_presets(values, millis());
}
// All measurements originate from checksum-valid status. Wire Fahrenheit is
// converted to Celsius here; invalid RH is published as NaN, not a real reading.
void PanasonicERV::on_status(const protocol::Status& s, const uint8_t* raw) {
  last_status_ = s;
  last_boost_ = raw[41];
  have_status_ = true;
  const float indoor = protocol::temperatureC(s.indoorTemperatureF);
  const float outdoor = protocol::temperatureC(s.outdoorTemperatureF);
  // This order must match sensor.py and the sensors_ slots in the header.
  const float values[] = {float(s.live[0]),
                          float(s.live[1]),
                          indoor,
                          outdoor,
                          s.indoorHumidity <= 100 ? float(s.indoorHumidity) : NAN,
                          s.outdoorHumidity <= 100 ? float(s.outdoorHumidity) : NAN,
                          float(s.powerCandidateW)};
  for (size_t i = 0; i < 7; ++i)
    if (sensors_[i])
      sensors_[i]->publish_state(values[i]);
  const bool fault = !protocol::blankFault(s);
  if (binary_[1])
    binary_[1]->publish_state(fault);
  if (text_[0]) {
    // Keep unknown/non-printable fault bytes visible. A zero-initialized
    // fourth byte makes the three-byte wire code safe as a C string.
    char code[4] = {0};
    for (size_t i = 0; i < 3; ++i)
      code[i] = s.fault[i] >= 32 && s.fault[i] < 127 ? s.fault[i] : '?';
    text_[0]->publish_state(fault ? code : "None");
  }
  publish_controls_();
  publish_diagnostics_();
}
// Publish the last ERV readback, including after a rejected request. Unknown
// binary flags do not invent a mode; numbers remain the reported preset values.
void PanasonicERV::publish_controls_() {
  if (!have_status_)
    return;
  const auto& s = last_status_;
  if (mode_ && s.control[0] <= 1 && s.control[2] <= 1 && last_boost_ <= 1)
    mode_->publish_state(!s.control[0]  ? "Standby"
                         : last_boost_  ? "Boost"
                         : s.control[2] ? "High"
                                        : "Low");
  for (size_t i = 0; i < 6; ++i)
    if (numbers_[i])
      numbers_[i]->publish_state(i < 2 ? s.low[i] : (i < 4 ? s.high[i - 2] : s.boost[i - 4]));
  if (switches_[0] && s.control[0] <= 1)
    switches_[0]->publish_state(s.control[0]);
  if (switches_[1] && last_boost_ <= 1)
    switches_[1]->publish_state(last_boost_);
}
void PanasonicERV::on_connection(bool connected) {
  if (binary_[0])
    binary_[0]->publish_state(connected);
  // Invalidate measured values on link loss, but retain last-known control
  // and fault states. Automations must gate commands on the link diagnostics.
  if (!connected) {
    status_set_warning("No recent valid ERV status");
    for (size_t i = 0; i < 7; ++i)
      if (sensors_[i])
        sensors_[i]->publish_state(NAN);
    if (binary_[2])
      binary_[2]->publish_state(false);
  } else
    status_clear_warning();
}
void PanasonicERV::publish_diagnostics_() {
  if (binary_[2])
    binary_[2]->publish_state(controller_.ready(millis()));
  if (sensors_[7])
    sensors_[7]->publish_state(controller_.valid_frames());
  if (sensors_[8])
    sensors_[8]->publish_state(controller_.rejected_frames());
}
// The text sensor holds only the latest event; logs emit each request ID/result.
// A timeout can be followed by rejections of queued edits.
void PanasonicERV::on_result(uint32_t id, Result result) {
  const char* label = "Unknown";
  switch (result) {
  case Result::QUEUED:
    label = "Queued";
    break;
  case Result::SENT:
    label = "Sent; awaiting readback";
    break;
  case Result::CONFIRMED:
    label = "Readback matches";
    break;
  case Result::TIMEOUT:
    label = "Timeout; not retried";
    break;
  case Result::REJECTED:
    label = "Rejected: busy, stale, range/order or unsupported settings";
    break;
  case Result::CONNECTION_LOST:
    label = "Connection lost; cancelled";
    break;
  }
  char message[128];
  snprintf(message, sizeof(message), "#%lu %s", static_cast<unsigned long>(id), label);
  ESP_LOGI(TAG, "%s", message);
  if (text_[1])
    text_[1]->publish_state(message);
  if (result == Result::REJECTED || result == Result::TIMEOUT)
    publish_controls_();
}
void ModeSelect::control(size_t index) {
  // Positional contract with select.py's option list; change them together.
  static const replay::Action actions[] = {replay::Action::STANDBY, replay::Action::SET_LOW,
                                           replay::Action::SET_HIGH, replay::Action::BOOST_ON};
  if (index < 4)
    parent_->request(actions[index]);
}
void PresetNumber::control(float value) { parent_->request_field(field_, value); }
void ERVSwitch::write_state(bool state) {
  parent_->request(boost_ ? (state ? replay::Action::BOOST_ON : replay::Action::BOOST_OFF)
                          : (state ? replay::Action::ON : replay::Action::STANDBY));
}
} // namespace panasonic_erv
} // namespace esphome
