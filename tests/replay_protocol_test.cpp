#include "replay_protocol.h"
#include <cassert>
namespace replay = esphome::panasonic_erv::replay;
namespace protocol = esphome::panasonic_erv::protocol;
#include <cstdio>
#include <cstring>
int main() {
  { // high boot=3a7019b65930d6c seq=142
    const uint8_t before[] = {165, 165, 90, 90, 5,  198, 3,  11,  61, 0,  1,  0,  1, 1,  0,
                              0,   1,   0,  0,  0,  0,   0,  30,  0,  30, 0,  90, 0, 90, 0,
                              60,  0,   60, 0,  30, 0,   30, 0,   0,  0,  45, 0,  5, 50, 2,
                              2,   0,   72, 66, 64, 66,  3,  32,  0,  0,  90, 0,  0, 0,  0,
                              3,   1,   2,  1,  0,  0,   0,  255, 0,  50, 0,  59, 1};
    const uint8_t expected[] = {165, 165, 90,  90, 49, 197, 2,   11, 47, 0,  1,  0,  1,  1,  1,
                                0,   1,   255, 0,  0,  0,   0,   90, 0,  90, 0,  60, 0,  60, 0,
                                30,  0,   30,  0,  0,  0,   45,  0,  0,  5,  50, 2,  2,  3,  0,
                                0,   1,   2,   1,  0,  0,   255, 0,  0,  0,  50, 0,  59, 1};
    const uint8_t after[] = {165, 165, 90, 90, 6,  198, 3,  11,  61, 0,  1,  0,  1, 1,  1,
                             0,   1,   0,  0,  0,  0,   0,  30,  0,  30, 0,  90, 0, 90, 0,
                             60,  0,   60, 0,  30, 0,   30, 0,   0,  0,  45, 0,  5, 50, 2,
                             2,   0,   72, 66, 64, 66,  3,  32,  0,  0,  90, 0,  0, 0,  0,
                             3,   1,   2,  1,  0,  0,   0,  255, 0,  50, 0,  59, 1};
    protocol::Status s;
    assert(protocol::decodeStatus(before, sizeof(before), s));
    replay::Settings desired = replay::settings(s);
    replay::Request request;
    assert(replay::build(before, sizeof(before), replay::Action::SET_HIGH, &desired, request));
    assert(protocol::valid(request.bytes, replay::SIZE));
    assert(memcmp(request.bytes, expected, replay::SIZE) == 0);
    assert(replay::matches(request, after, sizeof(after)));
    // Invalid/truncated source cannot be used to build a write.
    assert(!replay::build(before, sizeof(before) - 1, replay::Action::SET_LOW, nullptr, request));
    uint8_t bad[73];
    memcpy(bad, before, 73);
    bad[12] ^= 1;
    assert(!replay::build(bad, 73, replay::Action::SET_LOW, nullptr, request));
    memcpy(bad, before, 73);
    bad[43] ^= 1;
    replay::put16(bad + 4, protocol::checksumHypothesis(bad, 73));
    assert(!replay::build(bad, 73, replay::Action::SET_LOW, nullptr, request));
    desired.low[0] = 29;
    assert(!replay::build(before, 73, replay::Action::PRESETS, &desired, request));
  }
  { // low boot=3a7019b65930d6c seq=766
    const uint8_t before[] = {165, 165, 90, 90, 85, 198, 3,  11,  61, 0,  1,  0,  1, 1,  1,
                              0,   1,   0,  0,  0,  0,   0,  49,  0,  60, 0,  90, 0, 90, 0,
                              60,  0,   60, 0,  30, 0,   30, 0,   0,  0,  45, 0,  5, 50, 2,
                              2,   0,   74, 67, 63, 66,  3,  60,  0,  0,  90, 0,  0, 0,  0,
                              3,   1,   2,  1,  0,  0,   0,  255, 0,  50, 0,  59, 1};
    const uint8_t expected[] = {165, 165, 90,  90, 48, 197, 2,   11, 47, 0,  1,  0,  1,  1,  0,
                                0,   1,   255, 0,  0,  0,   0,   90, 0,  90, 0,  60, 0,  60, 0,
                                30,  0,   30,  0,  0,  0,   45,  0,  0,  5,  50, 2,  2,  3,  0,
                                0,   1,   2,   1,  0,  0,   255, 0,  0,  0,  50, 0,  59, 1};
    const uint8_t after[] = {165, 165, 90, 90, 84, 198, 3,  11,  61, 0,  1,  0,  1, 1,  0,
                             0,   1,   0,  0,  0,  0,   0,  49,  0,  60, 0,  90, 0, 90, 0,
                             60,  0,   60, 0,  30, 0,   30, 0,   0,  0,  45, 0,  5, 50, 2,
                             2,   0,   74, 67, 63, 66,  3,  60,  0,  0,  90, 0,  0, 0,  0,
                             3,   1,   2,  1,  0,  0,   0,  255, 0,  50, 0,  59, 1};
    protocol::Status s;
    assert(protocol::decodeStatus(before, sizeof(before), s));
    replay::Settings desired = replay::settings(s);
    replay::Request request;
    assert(replay::build(before, sizeof(before), replay::Action::SET_LOW, &desired, request));
    assert(protocol::valid(request.bytes, replay::SIZE));
    assert(memcmp(request.bytes, expected, replay::SIZE) == 0);
    assert(replay::matches(request, after, sizeof(after)));
    // Invalid/truncated source cannot be used to build a write.
    assert(!replay::build(before, sizeof(before) - 1, replay::Action::SET_LOW, nullptr, request));
    uint8_t bad[73];
    memcpy(bad, before, 73);
    bad[12] ^= 1;
    assert(!replay::build(bad, 73, replay::Action::SET_LOW, nullptr, request));
    memcpy(bad, before, 73);
    bad[43] ^= 1;
    replay::put16(bad + 4, protocol::checksumHypothesis(bad, 73));
    assert(!replay::build(bad, 73, replay::Action::SET_LOW, nullptr, request));
    desired.low[0] = 29;
    assert(!replay::build(before, 73, replay::Action::PRESETS, &desired, request));
  }
  { // boost_on boot=3a7019b65930d6c seq=988
    const uint8_t before[] = {165, 165, 90, 90, 3,  198, 3,  11,  61, 0,  1,  0,  1, 1,  0,
                              0,   1,   0,  0,  0,  0,   0,  30,  0,  28, 0,  90, 0, 90, 0,
                              60,  0,   60, 0,  30, 0,   30, 0,   0,  0,  45, 0,  5, 50, 2,
                              2,   0,   73, 66, 63, 66,  3,  32,  0,  0,  90, 0,  0, 0,  0,
                              3,   1,   2,  1,  0,  0,   0,  255, 0,  50, 0,  59, 1};
    const uint8_t expected[] = {165, 165, 90,  90, 50, 197, 2,   11, 47, 0,  1,  0,  1,  1,  0,
                                0,   1,   255, 0,  0,  0,   0,   90, 0,  90, 0,  60, 0,  60, 0,
                                30,  0,   30,  0,  0,  0,   45,  1,  1,  5,  50, 2,  2,  3,  0,
                                0,   1,   2,   1,  0,  0,   255, 0,  0,  0,  50, 0,  59, 1};
    const uint8_t after[] = {165, 165, 90, 90, 13, 198, 3,  11,  61, 0,  1,  0,  1, 1,  0,
                             0,   1,   0,  0,  0,  0,   0,  30,  0,  30, 0,  90, 0, 90, 0,
                             60,  0,   60, 0,  30, 0,   30, 0,   0,  0,  45, 1,  5, 50, 2,
                             2,   6,   73, 66, 63, 66,  3,  33,  0,  0,  90, 0,  0, 0,  0,
                             3,   1,   2,  1,  0,  0,   0,  255, 0,  50, 0,  59, 1};
    protocol::Status s;
    assert(protocol::decodeStatus(before, sizeof(before), s));
    replay::Settings desired = replay::settings(s);
    replay::Request request;
    assert(replay::build(before, sizeof(before), replay::Action::BOOST_ON, &desired, request));
    assert(protocol::valid(request.bytes, replay::SIZE));
    assert(memcmp(request.bytes, expected, replay::SIZE) == 0);
    assert(replay::matches(request, after, sizeof(after)));
    // Invalid/truncated source cannot be used to build a write.
    assert(!replay::build(before, sizeof(before) - 1, replay::Action::SET_LOW, nullptr, request));
    uint8_t bad[73];
    memcpy(bad, before, 73);
    bad[12] ^= 1;
    assert(!replay::build(bad, 73, replay::Action::SET_LOW, nullptr, request));
    memcpy(bad, before, 73);
    bad[43] ^= 1;
    replay::put16(bad + 4, protocol::checksumHypothesis(bad, 73));
    assert(!replay::build(bad, 73, replay::Action::SET_LOW, nullptr, request));
    desired.low[0] = 29;
    assert(!replay::build(before, 73, replay::Action::PRESETS, &desired, request));
  }
  { // boost_off boot=3a7019b65930d6c seq=1136
    const uint8_t before[] = {165, 165, 90, 90, 155, 198, 3,  11,  61, 0,  1,  0,  1, 1,  0,
                              0,   1,   0,  0,  0,   0,   0,  53,  0,  91, 0,  90, 0, 90, 0,
                              60,  0,   60, 0,  30,  0,   30, 0,   0,  0,  45, 1,  5, 50, 2,
                              2,   6,   73, 67, 63,  66,  3,  90,  0,  0,  90, 0,  0, 0,  0,
                              3,   1,   2,  1,  0,   0,   0,  255, 0,  50, 0,  59, 1};
    const uint8_t expected[] = {165, 165, 90,  90, 48, 197, 2,   11, 47, 0,  1,  0,  1,  1,  0,
                                0,   1,   255, 0,  0,  0,   0,   90, 0,  90, 0,  60, 0,  60, 0,
                                30,  0,   30,  0,  0,  0,   45,  0,  0,  5,  50, 2,  2,  3,  0,
                                0,   1,   2,   1,  0,  0,   255, 0,  0,  0,  50, 0,  59, 1};
    const uint8_t after[] = {165, 165, 90, 90, 148, 198, 3,  11,  61, 0,  1,  0,  1, 1,  0,
                             0,   1,   0,  0,  0,   0,   0,  55,  0,  89, 0,  90, 0, 90, 0,
                             60,  0,   60, 0,  30,  0,   30, 0,   0,  0,  45, 0,  5, 50, 2,
                             2,   0,   73, 67, 63,  66,  3,  90,  0,  0,  90, 0,  0, 0,  0,
                             3,   1,   2,  1,  0,   0,   0,  255, 0,  50, 0,  59, 1};
    protocol::Status s;
    assert(protocol::decodeStatus(before, sizeof(before), s));
    replay::Settings desired = replay::settings(s);
    replay::Request request;
    assert(replay::build(before, sizeof(before), replay::Action::BOOST_OFF, &desired, request));
    assert(protocol::valid(request.bytes, replay::SIZE));
    assert(memcmp(request.bytes, expected, replay::SIZE) == 0);
    assert(replay::matches(request, after, sizeof(after)));
    // Invalid/truncated source cannot be used to build a write.
    assert(!replay::build(before, sizeof(before) - 1, replay::Action::SET_LOW, nullptr, request));
    uint8_t bad[73];
    memcpy(bad, before, 73);
    bad[12] ^= 1;
    assert(!replay::build(bad, 73, replay::Action::SET_LOW, nullptr, request));
    memcpy(bad, before, 73);
    bad[43] ^= 1;
    replay::put16(bad + 4, protocol::checksumHypothesis(bad, 73));
    assert(!replay::build(bad, 73, replay::Action::SET_LOW, nullptr, request));
    desired.low[0] = 29;
    assert(!replay::build(before, 73, replay::Action::PRESETS, &desired, request));
  }
  { // standby boot=3a7019b65930d6c seq=1282
    const uint8_t before[] = {165, 165, 90, 90, 8,  198, 3,  11,  61, 0,  1,  0,  1, 1,  0,
                              0,   1,   0,  0,  0,  0,   0,  30,  0,  30, 0,  90, 0, 90, 0,
                              60,  0,   60, 0,  30, 0,   30, 0,   0,  0,  45, 0,  5, 50, 2,
                              2,   0,   74, 67, 63, 66,  3,  33,  0,  0,  90, 0,  0, 0,  0,
                              3,   1,   2,  1,  0,  0,   0,  255, 0,  50, 0,  59, 1};
    const uint8_t expected[] = {165, 165, 90,  90, 47, 197, 2,   11, 47, 0,  1,  0,  0,  1,  0,
                                0,   1,   255, 0,  0,  0,   0,   90, 0,  90, 0,  60, 0,  60, 0,
                                30,  0,   30,  0,  0,  0,   45,  0,  0,  5,  50, 2,  2,  3,  0,
                                0,   1,   2,   1,  0,  0,   255, 0,  0,  0,  50, 0,  59, 1};
    const uint8_t after[] = {165, 165, 90,  90,  244, 200, 3,   11,  61, 0,  1,  0,  0, 1,  0,
                             0,   1,   0,   0,   0,   0,   0,   30,  0,  30, 0,  90, 0, 90, 0,
                             60,  0,   60,  0,   30,  0,   30,  0,   0,  0,  45, 0,  5, 50, 2,
                             2,   0,   255, 255, 127, 127, 3,   33,  0,  0,  90, 0,  0, 0,  0,
                             3,   1,   2,   1,   0,   0,   255, 255, 0,  50, 0,  59, 1};
    protocol::Status s;
    assert(protocol::decodeStatus(before, sizeof(before), s));
    replay::Settings desired = replay::settings(s);
    replay::Request request;
    assert(replay::build(before, sizeof(before), replay::Action::STANDBY, &desired, request));
    assert(protocol::valid(request.bytes, replay::SIZE));
    assert(memcmp(request.bytes, expected, replay::SIZE) == 0);
    assert(replay::matches(request, after, sizeof(after)));
    // Invalid/truncated source cannot be used to build a write.
    assert(!replay::build(before, sizeof(before) - 1, replay::Action::SET_LOW, nullptr, request));
    uint8_t bad[73];
    memcpy(bad, before, 73);
    bad[12] ^= 1;
    assert(!replay::build(bad, 73, replay::Action::SET_LOW, nullptr, request));
    memcpy(bad, before, 73);
    bad[43] ^= 1;
    replay::put16(bad + 4, protocol::checksumHypothesis(bad, 73));
    assert(!replay::build(bad, 73, replay::Action::SET_LOW, nullptr, request));
    desired.low[0] = 29;
    assert(!replay::build(before, 73, replay::Action::PRESETS, &desired, request));
  }
  { // on boot=3a7019b65930d6c seq=1452
    const uint8_t before[] = {165, 165, 90,  90,  154, 200, 3,   11,  61, 0,  1,  0,  0, 1,  0,
                              0,   1,   0,   0,   0,   0,   0,   0,   0,  0,  0,  90, 0, 90, 0,
                              60,  0,   60,  0,   30,  0,   30,  0,   0,  0,  45, 0,  5, 50, 2,
                              2,   0,   255, 255, 127, 127, 3,   3,   0,  0,  90, 0,  0, 0,  0,
                              3,   1,   2,   1,   0,   0,   255, 255, 0,  50, 0,  59, 1};
    const uint8_t expected[] = {165, 165, 90,  90, 48, 197, 2,   11, 47, 0,  1,  0,  1,  1,  0,
                                0,   1,   255, 0,  0,  0,   0,   90, 0,  90, 0,  60, 0,  60, 0,
                                30,  0,   30,  0,  0,  0,   45,  0,  0,  5,  50, 2,  2,  3,  0,
                                0,   1,   2,   1,  0,  0,   255, 0,  0,  0,  50, 0,  59, 1};
    const uint8_t after[] = {165, 165, 90, 90, 173, 197, 3,  11,  61, 0,  1,  0,  1, 1,  0,
                             0,   1,   0,  0,  0,   0,   0,  0,   0,  0,  0,  90, 0, 90, 0,
                             60,  0,   60, 0,  30,  0,   30, 0,   0,  0,  45, 0,  5, 50, 2,
                             2,   0,   74, 66, 63,  66,  3,  3,   0,  0,  90, 0,  0, 0,  0,
                             3,   1,   2,  1,  0,   0,   0,  255, 0,  50, 0,  59, 1};
    protocol::Status s;
    assert(protocol::decodeStatus(before, sizeof(before), s));
    replay::Settings desired = replay::settings(s);
    replay::Request request;
    assert(replay::build(before, sizeof(before), replay::Action::ON, &desired, request));
    assert(protocol::valid(request.bytes, replay::SIZE));
    assert(memcmp(request.bytes, expected, replay::SIZE) == 0);
    assert(replay::matches(request, after, sizeof(after)));
    // Invalid/truncated source cannot be used to build a write.
    assert(!replay::build(before, sizeof(before) - 1, replay::Action::SET_LOW, nullptr, request));
    uint8_t bad[73];
    memcpy(bad, before, 73);
    bad[12] ^= 1;
    assert(!replay::build(bad, 73, replay::Action::SET_LOW, nullptr, request));
    memcpy(bad, before, 73);
    bad[43] ^= 1;
    replay::put16(bad + 4, protocol::checksumHypothesis(bad, 73));
    assert(!replay::build(bad, 73, replay::Action::SET_LOW, nullptr, request));
    desired.low[0] = 29;
    assert(!replay::build(before, 73, replay::Action::PRESETS, &desired, request));
  }
  { // save_exhaust_presets boot=3bd3c11cdaf1b2f4 seq=531
    const uint8_t before[] = {165, 165, 90, 90, 58, 198, 3,  11,  61, 0,  1,  0,  1, 1,  0,
                              0,   1,   3,  31, 0,  35,  0,  28,  0,  34, 0,  90, 0, 80, 0,
                              60,  0,   50, 0,  30, 0,   35, 0,   0,  0,  45, 0,  5, 50, 2,
                              2,   0,   74, 66, 63, 66,  3,  27,  0,  0,  90, 0,  0, 0,  0,
                              3,   1,   2,  1,  0,  0,   1,  255, 0,  50, 0,  59, 1};
    const uint8_t expected[] = {165, 165, 90, 90,  32,  200, 2,   11, 47, 0,  1,  0,  1,  1,  0,
                                0,   1,   0,  255, 255, 255, 255, 85, 0,  80, 0,  65, 0,  50, 0,
                                31,  0,   35, 0,   0,   0,   45,  0,  0,  5,  50, 2,  2,  3,  0,
                                0,   1,   2,  1,   0,   0,   255, 1,  0,  0,  50, 0,  59, 1};
    const uint8_t after[] = {165, 165, 90, 90, 56, 198, 3,  11,  61, 0,  1,  0,  1, 1,  0,
                             0,   1,   0,  31, 0,  35,  0,  28,  0,  32, 0,  85, 0, 80, 0,
                             65,  0,   50, 0,  31, 0,   35, 0,   0,  0,  45, 0,  5, 50, 2,
                             2,   0,   74, 66, 63, 66,  3,  29,  0,  0,  90, 0,  0, 0,  0,
                             3,   1,   2,  1,  0,  0,   1,  255, 0,  50, 0,  59, 1};
    protocol::Status s;
    assert(protocol::decodeStatus(before, sizeof(before), s));
    replay::Settings desired = replay::settings(s);
    for (size_t i = 0; i < 2; ++i) {
      desired.boost[i] = protocol::le16(expected + 22 + 2 * (1 - i));
      desired.high[i] = protocol::le16(expected + 26 + 2 * (1 - i));
      desired.low[i] = protocol::le16(expected + 30 + 2 * (1 - i));
    }
    replay::Request request;
    assert(replay::build(before, sizeof(before), replay::Action::PRESETS, &desired, request));
    assert(protocol::valid(request.bytes, replay::SIZE));
    assert(memcmp(request.bytes, expected, replay::SIZE) == 0);
    assert(replay::matches(request, after, sizeof(after)));
    // Invalid/truncated source cannot be used to build a write.
    assert(!replay::build(before, sizeof(before) - 1, replay::Action::SET_LOW, nullptr, request));
    uint8_t bad[73];
    memcpy(bad, before, 73);
    bad[12] ^= 1;
    assert(!replay::build(bad, 73, replay::Action::SET_LOW, nullptr, request));
    memcpy(bad, before, 73);
    bad[43] ^= 1;
    replay::put16(bad + 4, protocol::checksumHypothesis(bad, 73));
    assert(!replay::build(bad, 73, replay::Action::SET_LOW, nullptr, request));
    desired.low[0] = 29;
    assert(!replay::build(before, 73, replay::Action::PRESETS, &desired, request));
  }
  { // restore_all_original_presets boot=3bd3c11cdaf1b2f4 seq=643
    const uint8_t before[] = {165, 165, 90, 90, 111, 198, 3,  11,  61, 0,  1,  0,  1, 1,  0,
                              0,   1,   3,  30, 0,   30,  0,  52,  0,  34, 0,  85, 0, 80, 0,
                              65,  0,   50, 0,  31,  0,   35, 0,   0,  0,  45, 0,  5, 50, 2,
                              2,   0,   74, 67, 63,  66,  3,  61,  0,  0,  90, 0,  0, 0,  0,
                              3,   1,   2,  1,  0,   0,   0,  255, 0,  50, 0,  59, 1};
    const uint8_t expected[] = {165, 165, 90, 90,  46,  200, 2,   11, 47, 0,  1,  0,  1,  1,  0,
                                0,   1,   0,  255, 255, 255, 255, 90, 0,  90, 0,  60, 0,  60, 0,
                                30,  0,   30, 0,   0,   0,   45,  0,  0,  5,  50, 2,  2,  3,  0,
                                0,   1,   2,  1,   0,   0,   255, 1,  0,  0,  50, 0,  59, 1};
    const uint8_t after[] = {165, 165, 90, 90, 119, 198, 3,  11,  61, 0,  1,  0,  1, 1,  0,
                             0,   1,   0,  30, 0,   30,  0,  52,  0,  34, 0,  90, 0, 90, 0,
                             60,  0,   60, 0,  30,  0,   30, 0,   0,  0,  45, 0,  5, 50, 2,
                             2,   0,   74, 67, 63,  66,  3,  58,  0,  0,  90, 0,  0, 0,  0,
                             3,   1,   2,  1,  0,   0,   0,  255, 0,  50, 0,  59, 1};
    protocol::Status s;
    assert(protocol::decodeStatus(before, sizeof(before), s));
    replay::Settings desired = replay::settings(s);
    for (size_t i = 0; i < 2; ++i) {
      desired.boost[i] = protocol::le16(expected + 22 + 2 * (1 - i));
      desired.high[i] = protocol::le16(expected + 26 + 2 * (1 - i));
      desired.low[i] = protocol::le16(expected + 30 + 2 * (1 - i));
    }
    replay::Request request;
    assert(replay::build(before, sizeof(before), replay::Action::PRESETS, &desired, request));
    assert(protocol::valid(request.bytes, replay::SIZE));
    assert(memcmp(request.bytes, expected, replay::SIZE) == 0);
    assert(replay::matches(request, after, sizeof(after)));
    // Invalid/truncated source cannot be used to build a write.
    assert(!replay::build(before, sizeof(before) - 1, replay::Action::SET_LOW, nullptr, request));
    uint8_t bad[73];
    memcpy(bad, before, 73);
    bad[12] ^= 1;
    assert(!replay::build(bad, 73, replay::Action::SET_LOW, nullptr, request));
    memcpy(bad, before, 73);
    bad[43] ^= 1;
    replay::put16(bad + 4, protocol::checksumHypothesis(bad, 73));
    assert(!replay::build(bad, 73, replay::Action::SET_LOW, nullptr, request));
    desired.low[0] = 29;
    assert(!replay::build(before, 73, replay::Action::PRESETS, &desired, request));
  }
  replay::Settings s = {{30, 30}, {60, 60}, {160, 160}};
  assert(replay::validSettings(s));
  s.boost[1] = 161;
  assert(!replay::validSettings(s));
  s.boost[1] = 160;
  s.high[0] = 29;
  assert(!replay::validSettings(s));
  puts("PASS: 8 exact OEM frames and readbacks, preserved presets, checksum/length/config guards, "
       "CFM bounds");
}
