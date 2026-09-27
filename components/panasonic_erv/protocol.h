#pragma once
#include <cmath>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace esphome {
namespace panasonic_erv {
namespace protocol {
static constexpr uint8_t POLL[] = {0xA5, 0xA5, 0x5A, 0x5A, 0xBA, 0xC0, 0x01, 0x0B, 0, 0, 1, 0};
inline uint16_t le16(const uint8_t* p) { return p[0] | (uint16_t(p[1]) << 8); }
inline bool sync(const uint8_t* p, size_t n) { return n >= 4 && memcmp(p, POLL, 4) == 0; }
inline size_t expected(const uint8_t* p, size_t n) {
  return sync(p, n) && n >= 12 ? 12u + le16(p + 8) : 0;
}
// Matches captured polls, status replies and 59-byte OEM control/config writes.
// Applying this formula to other message families remains a hypothesis.
inline uint16_t checksumHypothesis(const uint8_t* p, size_t n) {
  uint16_t sum = 0xC0AD;
  for (size_t i = 6; i < n; ++i)
    sum += p[i];
  return sum;
}
inline bool valid(const uint8_t* p, size_t n) {
  return n >= 12 && expected(p, n) == n && le16(p + 4) == checksumHypothesis(p, n);
}
struct Status {
  // Public arrays always [SA, EA]; wire pairs are [EA, SA], proven by asymmetric edits.
  uint16_t live[2], boost[2], high[2], low[2];
  // Display-correlated on 2026-09-27 with controller set to Fahrenheit.
  // Celsius display mode and signed temperatures unverified.
  // Standby supplies temperature=127 and RH=255 (unavailable), retained raw here.
  uint8_t outdoorHumidity, indoorHumidity, outdoorTemperatureF, indoorTemperatureF;
  uint16_t powerCandidateW; // offsets 52..53; tracks reported 60/61 W, width provisional
  uint8_t control[3];       // frame 12..14: power, mode (unknown), Low/High
  uint8_t fault[3];         // frame 57..59; F01 = communication error (user confirmed)
};
inline bool decodeStatus(const uint8_t* p, size_t n, Status& out) {
  if (n != 73 || !valid(p, n) || p[6] != 0x03 || p[7] != 0x0B)
    return false;
  Status decoded = {};
  for (size_t i = 0; i < 2; ++i) {
    decoded.live[i] = le16(p + 22 + 2 * (1 - i));
    decoded.boost[i] = le16(p + 26 + 2 * (1 - i));
    decoded.high[i] = le16(p + 30 + 2 * (1 - i));
    decoded.low[i] = le16(p + 34 + 2 * (1 - i));
  }
  decoded.outdoorHumidity = p[47];
  decoded.indoorHumidity = p[48];
  decoded.outdoorTemperatureF = p[49];
  decoded.indoorTemperatureF = p[50];
  decoded.powerCandidateW = le16(p + 52);
  memcpy(decoded.control, p + 12, 3);
  memcpy(decoded.fault, p + 57, 3);
  out = decoded;
  return true;
}
// Only the observed unsigned Fahrenheit encoding is supported. 127 is the
// Standby sentinel; larger values may encode negatives or errors, so suppress.
inline float temperatureC(uint8_t raw) { return raw >= 127 ? NAN : (raw - 32.0f) * 5.0f / 9.0f; }
inline bool commsFault(const Status& status) { return memcmp(status.fault, "F01", 3) == 0; }
inline bool blankFault(const Status& status) {
  return status.fault[0] == 0 && status.fault[1] == 0 && status.fault[2] == 0;
}
struct ScanCounts {
  size_t accepted = 0, rejected = 0, noise = 0;
};
// Called for an idle-delimited burst. Resync after junk, bad checksums or lengths;
// never publish a partial frame and never combine data across an idle boundary.
template <class Callback> ScanCounts scanBurst(const uint8_t* data, size_t size, Callback accept) {
  ScanCounts counts;
  size_t pos = 0;
  while (pos < size) {
    size_t left = size - pos;
    if (!sync(data + pos, left)) {
      ++pos;
      ++counts.noise;
      continue;
    }
    const size_t length = expected(data + pos, left);
    if (left < 12 || length > 512 || length > left || !valid(data + pos, length)) {
      ++counts.rejected;
      ++pos;
      continue;
    }
    accept(data + pos, length);
    ++counts.accepted;
    pos += length;
  }
  return counts;
}

} // namespace protocol
} // namespace panasonic_erv
} // namespace esphome
