#include "controller.h"
#include "fixtures/status.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <utility>
#include <vector>
using namespace esphome::panasonic_erv;
struct Fake : Listener {
  std::vector<std::vector<uint8_t>> tx;
  std::vector<std::pair<uint32_t, Result>> results;
  std::vector<bool> connections;
  size_t statuses = 0;
  bool send_ok = true;
  bool send_frame(const uint8_t* p, size_t n) override {
    if (!send_ok)
      return false;
    tx.emplace_back(p, p + n);
    return true;
  }
  void on_status(const protocol::Status&, const uint8_t*) override { ++statuses; }
  void on_result(uint32_t id, Result r) override { results.emplace_back(id, r); }
  void on_connection(bool value) override { connections.push_back(value); }
};
using Frame = std::array<uint8_t, 73>;
Frame base() {
  Frame b;
  memcpy(b.data(), fixtures::BASE, 73);
  return b;
}
void checksum(Frame& b) { replay::put16(b.data() + 4, protocol::checksumHypothesis(b.data(), 73)); }
void feed(Controller& c, Frame b, uint32_t t) {
  checksum(b);
  for (uint8_t byte : b)
    c.ingest(byte, t);
}
int main() {
  assert(std::abs(protocol::temperatureC(66) - 18.88889f) < 0.001f);
  assert(std::isnan(protocol::temperatureC(127)) && std::isnan(protocol::temperatureC(255)));
  {
    Fake f;
    Controller c(&f);
    c.tick(0);
    assert(c.polls() == 1 && c.writes() == 0);
    assert(!c.request(replay::Action::SET_HIGH, 10)); // no startup state replay
    feed(c, base(), 100);
    assert(c.ready(100));
    auto bad = base();
    bad[22] ^= 1;
    for (uint8_t b : bad)
      c.ingest(b, 200);
    assert(f.statuses == 1 && c.rejected_frames() > 0);
    // Noise and chunked frames recover without publishing incomplete data.
    c.ingest(0xFF, 250);
    auto good = base();
    for (size_t i = 0; i < 30; ++i) {
      c.ingest(good[i], 300);
    }
    assert(f.statuses == 1);
    for (size_t i = 30; i < good.size(); ++i) {
      c.ingest(good[i], 310);
    }
    assert(f.statuses == 2);
    // A clipped burst must not be joined across a long idle gap.
    for (size_t i = 0; i < 20; ++i)
      c.ingest(good[i], 400);
    c.tick(700);
    feed(c, good, 710);
    assert(f.statuses == 3);
    assert(c.request(replay::Action::SET_HIGH, 800));
    c.tick(1000);
    assert(c.writes() == 1);
    assert(f.tx.back()[14] == 1 && protocol::valid(f.tx.back().data(), 59));
    auto high = base();
    high[14] = 1;
    feed(c, high, 1050);
    assert(c.busy()); // pre-wire-completion data cannot confirm
    feed(c, high, 1200);
    assert(!c.busy() && f.results.back().second == Result::CONFIRMED);
    assert(c.request_field(0, 35, 1300));
    assert(c.request_field(1, 31, 1301));
    c.tick(2000);
    assert(f.tx.back()[32] == 35 && f.tx.back()[30] == 30);
    auto first = high;
    replay::put16(first.data() + 36, 35);
    feed(c, first, 2200);
    c.tick(3000);
    assert(f.tx.back()[32] == 35 && f.tx.back()[30] == 31); // preserves first queued edit
    auto second = first;
    replay::put16(second.data() + 34, 31);
    feed(c, second, 3200);
    assert(!c.busy());
    assert(c.writes() == 3);
    c.tick(14000);
    assert(!c.connected(14000) && !f.connections.back());
    assert(!c.request(replay::Action::ON, 14001));
  }
  {
    Fake f;
    Controller c(&f);
    feed(c, base(), 0);
    assert(c.request(replay::Action::SET_HIGH, 1));
    c.tick(1);
    assert(c.request_field(0, 35, 2)); // dependent edit cancelled with timed-out write
    for (uint32_t t = 1001; t <= 9001; t += 1000)
      c.tick(t);
    assert(c.writes() == 1 && !c.busy());
    bool timeout = false;
    for (auto r : f.results)
      timeout |= r.second == Result::TIMEOUT;
    assert(timeout);
    feed(c, base(), 10001);
    c.tick(11001);
    assert(c.writes() == 1); // reconnect cannot replay old actions
  }
  {
    Fake f;
    Controller c(&f);
    c.set_command_timeout(60000);
    feed(c, base(), 0);
    c.request(replay::Action::SET_HIGH, 1);
    c.tick(1);
    c.tick(10001);
    assert(!c.busy() && f.results.back().second == Result::CONNECTION_LOST);
  }
  {
    Fake f;
    Controller c(&f);
    auto b = base();
    b[43] ^= 1;
    feed(c, b, 1);
    assert(c.connected(1) && !c.ready(1));
    assert(!c.request(replay::Action::ON, 1));
    feed(c, base(), 2);
    assert(!c.request_field(0, 29, 2));
    assert(!c.request_field(6, 35, 2));
    for (int i = 0; i < 8; ++i)
      assert(c.request(replay::Action::ON, 2));
    assert(!c.request(replay::Action::ON, 2)); // bounded queue
  }
  {
    Fake f;
    Controller c(&f);
    const uint32_t start = 0xFFFFFE00u;
    feed(c, base(), start);
    assert(c.request(replay::Action::SET_HIGH, start + 1));
    c.tick(start + 1);
    auto high = base();
    high[14] = 1;
    feed(c, high, start + 200);
    assert(!c.busy());
    c.tick(start + 1001);
    assert(c.polls() == 1);
    assert(c.connected(start + 1001));
    c.tick(start + 12000);
    assert(!c.connected(start + 12000)); // millis rollover
  }
  {
    Fake f;
    Controller c(&f);
    feed(c, base(), 0);
    c.request(replay::Action::SET_HIGH, 1);
    c.request_field(0, 35, 1); // cancel dependent work if transport rejects the write
    f.send_ok = false;
    c.tick(1);
    assert(c.writes() == 0 && !c.busy() && f.results.back().second == Result::REJECTED);
  }
  puts("PASS: framing, queue preservation, bounded queue, freshness, timeout/no retry, "
       "reconnection, rollover, transport failure");
}
