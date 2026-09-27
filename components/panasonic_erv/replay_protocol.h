#pragma once
#include "protocol.h"

namespace esphome {
namespace panasonic_erv {
namespace replay {
static constexpr size_t SIZE = 59;
// Own-controller Low frame, capture boot 3a7019b65930d6c / seq 766.
static constexpr uint8_t BASE[SIZE] = {
    0xA5, 0xA5, 0x5A, 0x5A, 0x30, 0xC5, 2,  11, 47, 0, 1,  0,   1,  1, 0, 0,  1,  255, 0, 0,
    0,    0,    90,   0,    90,   0,    60, 0,  60, 0, 30, 0,   30, 0, 0, 0,  45, 0,   0, 5,
    50,   2,    2,    3,    0,    0,    1,  2,  1,  0, 0,  255, 0,  0, 0, 50, 0,  59,  1};
enum class Action { SET_LOW, SET_HIGH, BOOST_ON, BOOST_OFF, STANDBY, ON, PRESETS };
struct Settings {
  uint16_t low[2], high[2], boost[2];
}; // normalized SA,EA
inline Settings settings(const protocol::Status& s) {
  Settings out = {};
  memcpy(out.low, s.low, sizeof(out.low));
  memcpy(out.high, s.high, sizeof(out.high));
  memcpy(out.boost, s.boost, sizeof(out.boost));
  return out;
}
inline bool validSettings(const Settings& s) {
  for (size_t i = 0; i < 2; ++i)
    if (s.low[i] < 30 || s.boost[i] > 160 || s.low[i] > s.high[i] || s.high[i] > s.boost[i])
      return false;
  return true;
}
inline void put16(uint8_t* p, uint16_t v) {
  p[0] = v & 255;
  p[1] = v >> 8;
}
// Unknown configuration fields stay at the captured settings. Refuse writes if
// their observed status counterparts differ, instead of guessing a translation.
inline bool supported(const uint8_t* raw, size_t n) {
  protocol::Status s;
  if (!protocol::decodeStatus(raw, n, s) || s.control[0] > 1 || s.control[2] > 1 || raw[41] > 1)
    return false;
  static constexpr uint8_t offsets[] = {13, 15, 16, 38, 39, 40, 42, 43, 44, 45, 51,
                                        61, 62, 63, 64, 65, 67, 69, 70, 71, 72};
  static constexpr uint8_t values[] = {1, 0, 1, 0, 0, 45,  5,  50, 2,  2, 3,
                                       1, 2, 1, 0, 0, 255, 50, 0,  59, 1};
  for (size_t i = 0; i < sizeof(offsets); ++i)
    if (raw[offsets[i]] != values[i])
      return false;
  return validSettings(settings(s));
}
struct Request {
  uint8_t bytes[SIZE];
  uint8_t run, speed, boost;
  Settings presets;
};
inline bool build(const uint8_t* raw, size_t n, Action action, const Settings* requested,
                  Request& out) {
  if (!supported(raw, n))
    return false;
  protocol::Status s;
  protocol::decodeStatus(raw, n, s);
  Request r = {};
  memcpy(r.bytes, BASE, SIZE);
  r.run = s.control[0];
  r.speed = s.control[2];
  r.boost = raw[41];
  r.presets = settings(s);
  switch (action) {
  case Action::SET_LOW:
    r.run = 1;
    r.speed = 0;
    r.boost = 0;
    break;
  case Action::SET_HIGH:
    r.run = 1;
    r.speed = 1;
    r.boost = 0;
    break;
  case Action::BOOST_ON:
    r.run = 1;
    r.boost = 1;
    break;
  case Action::BOOST_OFF:
    r.boost = 0;
    break;
  case Action::STANDBY:
    r.run = 0;
    r.boost = 0;
    break;
  case Action::ON:
    r.run = 1;
    break;
  case Action::PRESETS:
    if (!requested || !validSettings(*requested))
      return false;
    r.presets = *requested;
    r.bytes[17] = 0;
    memset(r.bytes + 18, 255, 4);
    // Last OEM restore/save seq643 uses byte52=1. Other meanings unverified.
    r.bytes[52] = 1;
    break;
  }
  r.bytes[12] = r.run;
  r.bytes[14] = r.speed;
  r.bytes[37] = r.boost;
  r.bytes[38] = r.boost;
  for (size_t i = 0; i < 2; ++i) {
    put16(r.bytes + 22 + 2 * i, r.presets.boost[1 - i]);
    put16(r.bytes + 26 + 2 * i, r.presets.high[1 - i]);
    put16(r.bytes + 30 + 2 * i, r.presets.low[1 - i]);
  }
  put16(r.bytes + 4, protocol::checksumHypothesis(r.bytes, SIZE));
  out = r;
  return true;
}
inline bool matches(const Request& r, const uint8_t* raw, size_t n) {
  protocol::Status s;
  if (!protocol::decodeStatus(raw, n, s))
    return false;
  if (s.control[0] != r.run || s.control[2] != r.speed || raw[41] != r.boost)
    return false;
  for (size_t i = 0; i < 2; ++i)
    if (s.low[i] != r.presets.low[i] || s.high[i] != r.presets.high[i] ||
        s.boost[i] != r.presets.boost[i])
      return false;
  return true;
}
} // namespace replay
} // namespace panasonic_erv
} // namespace esphome
