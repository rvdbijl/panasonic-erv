#pragma once
#include "replay_protocol.h"
#include <array>
#include <cstring>

namespace esphome {
namespace panasonic_erv {
// Incremental UART framing, independent of ESPHome and the command scheduler.
// Bytes may arrive across loop calls or in concatenated frames. The 512-byte
// buffer bounds malformed lengths; it is not the expected status-frame size.
class FrameParser {
public:
  template <typename Callback> void feed(uint8_t byte, uint32_t now, Callback callback) {
    // Drop an unfinished burst after 250 ms of read-time silence. These are
    // software read timestamps, not scope-quality wire-edge timestamps.
    if (size_ && uint32_t(now - last_byte_) > 250) {
      size_ = 0;
      ++rejected_;
    }
    last_byte_ = now;
    if (size_ == buffer_.size()) {
      discard_(1);
      ++rejected_;
    }
    buffer_[size_++] = byte;
    // Slide one byte after noise/invalid candidates, preserving any later
    // header already buffered. Incomplete candidates wait for more input.
    while (size_ >= 4) {
      if (!protocol::sync(buffer_.data(), size_)) {
        discard_(1);
        continue;
      }
      if (size_ < 12)
        return;
      const size_t length = protocol::expected(buffer_.data(), size_);
      if (length > buffer_.size()) {
        discard_(1);
        ++rejected_;
        continue;
      }
      if (size_ < length)
        return;
      if (!protocol::valid(buffer_.data(), length)) {
        discard_(1);
        ++rejected_;
        continue;
      }
      // The callback borrows this storage only until it returns: discard_()
      // moves remaining bytes, and future feed() calls overwrite the buffer.
      callback(buffer_.data(), length);
      discard_(length);
    }
  }
  bool partial() const { return size_ > 0; }
  void expire(uint32_t now) {
    if (size_ && uint32_t(now - last_byte_) > 250) {
      size_ = 0;
      ++rejected_;
    }
  }
  // Counts invalid candidates, overflow and partial expiry, not every noise
  // byte and not hardware UART parity/framing errors.
  uint32_t rejected() const { return rejected_; }

private:
  void discard_(size_t count) {
    size_ -= count;
    memmove(buffer_.data(), buffer_.data() + count, size_);
  }
  std::array<uint8_t, 512> buffer_{};
  size_t size_{0};
  uint32_t last_byte_{0}, rejected_{0};
};

enum class Result { QUEUED, SENT, CONFIRMED, TIMEOUT, REJECTED, CONNECTION_LOST };
// The caller owns the transport/listener and must keep it alive while the
// Controller is used. Callbacks are synchronous on the caller's thread;
// send_frame() reports a local transport attempt, not ERV acceptance.
class Listener {
public:
  virtual ~Listener() = default;
  virtual bool send_frame(const uint8_t* bytes, size_t size) = 0;
  virtual void on_status(const protocol::Status& status, const uint8_t* raw) = 0;
  virtual void on_result(uint32_t id, Result result) = 0;
  virtual void on_connection(bool connected) = 0;
};

// All serial scheduling lives here; the ESPHome adapter only feeds bytes and
// calls tick(). One write may await readback while up to eight intents wait.
// Time differences use unsigned subtraction so millis() rollover is handled.
class Controller {
public:
  explicit Controller(Listener* listener) : listener_(listener) {}
  void set_poll_interval(uint32_t ms) { poll_interval_ = ms; }
  void set_status_timeout(uint32_t ms) { status_timeout_ = ms; }
  void set_command_timeout(uint32_t ms) { command_timeout_ = ms; }
  bool connected(uint32_t now) const {
    return have_status_ && uint32_t(now - last_status_) <= status_timeout_;
  }
  // Readability and writability differ: a link may be connected while its
  // settings are unsupported or older than the stricter 5-second write limit.
  bool ready(uint32_t now) const {
    return connected(now) && uint32_t(now - last_status_) <= 5000 &&
           replay::supported(raw_.data(), raw_.size());
  }
  bool busy() const { return pending_ || count_; }
  uint32_t rejected_frames() const { return parser_.rejected(); }
  uint32_t valid_frames() const { return valid_frames_; }
  uint32_t writes() const { return writes_; }
  uint32_t polls() const { return polls_; }
  const uint8_t* raw() const { return raw_.data(); }
  // A true return means queued, not transmitted or confirmed. Both enqueue
  // and dispatch check freshness because the queue can outlive a good status.
  bool request(replay::Action action, uint32_t now) {
    Intent i{};
    i.action = action;
    return enqueue_(i, now);
  }
  // Fields: Low SA,Low EA,High SA,High EA,Boost SA,Boost EA.
  bool request_field(uint8_t field, uint16_t value, uint32_t now) {
    Intent i{};
    i.action = replay::Action::PRESETS;
    i.field = field;
    i.value = value;
    if (field >= 6 || value < 30 || value > 160) {
      listener_->on_result(++next_id_, Result::REJECTED);
      return false;
    }
    return enqueue_(i, now);
  }
  // Unlike a one-field edit, an atomic preset intent intentionally carries
  // all six desired values. Its power/speed/Boost still come from send-time status.
  bool request_presets(const replay::Settings& settings, uint32_t now) {
    Intent i{};
    i.action = replay::Action::PRESETS;
    i.all = true;
    i.presets = settings;
    if (!replay::validSettings(settings)) {
      listener_->on_result(++next_id_, Result::REJECTED);
      return false;
    }
    return enqueue_(i, now);
  }
  void ingest(uint8_t byte, uint32_t now) {
    parser_.feed(byte, now, [this, now](const uint8_t* bytes, size_t size) {
      protocol::Status status;
      if (!protocol::decodeStatus(bytes, size, status))
        return;
      // Copy the parser's borrowed frame before publishing it. Valid frame
      // counts include only status frames, not echoes or other message types.
      raw_ = std::array<uint8_t, 73>{};
      memcpy(raw_.data(), bytes, 73);
      have_status_ = true;
      last_status_ = now;
      ++valid_frames_;
      if (!online_) {
        online_ = true;
        listener_->on_connection(true);
      }
      listener_->on_status(status, raw_.data());
      // 59 bytes * 11 bits (8E1) / 4800 baud is about 135.2 ms. Ignore
      // readback arriving before that minimum send duration; this is a timing
      // guard, not proof of causality or a separate acknowledgement packet.
      if (pending_ && uint32_t(now - sent_at_) >= 136 && replay::matches(sent_, raw_.data(), 73)) {
        pending_ = false;
        listener_->on_result(pending_id_, Result::CONFIRMED);
      }
    });
  }
  void tick(uint32_t now) {
    parser_.expire(now);
    // Cancel outstanding work on observed link loss. Retaining the last raw
    // status for diagnostics must not cause old requests to replay on reconnect.
    if (online_ && !connected(now)) {
      online_ = false;
      listener_->on_connection(false);
      if (pending_) {
        listener_->on_result(pending_id_, Result::CONNECTION_LOST);
        pending_ = false;
      }
      while (count_) {
        listener_->on_result(pop_().id, Result::CONNECTION_LOST);
      }
    }
    if (pending_ && uint32_t(now - sent_at_) >= command_timeout_) {
      listener_->on_result(pending_id_, Result::TIMEOUT);
      pending_ = false;
      // Dependent queued edits must not silently run after a failed transaction.
      while (count_)
        listener_->on_result(pop_().id, Result::REJECTED);
    }
    // Apply the same spacing to polls and writes, and avoid transmitting into
    // an unfinished receive burst. expire() releases partial data after silence.
    if (transmitted_ && uint32_t(now - last_tx_) < poll_interval_)
      return;
    if (parser_.partial())
      return;
    if (!pending_ && count_) {
      Intent intent = pop_();
      if (!ready(now)) {
        listener_->on_result(intent.id, Result::REJECTED);
        return;
      }
      protocol::Status status;
      protocol::decodeStatus(raw_.data(), 73, status);
      // Rebase a single-field edit on the latest status, not its enqueue-time
      // snapshot. Otherwise a second queued edit could undo the first one.
      replay::Settings desired = replay::settings(status);
      if (intent.all)
        desired = intent.presets;
      else if (intent.action == replay::Action::PRESETS) {
        uint16_t* pair =
            intent.field < 2 ? desired.low : (intent.field < 4 ? desired.high : desired.boost);
        pair[intent.field % 2] = intent.value;
      }
      replay::Request request;
      if (!replay::build(raw_.data(), 73, intent.action, &desired, request)) {
        listener_->on_result(intent.id, Result::REJECTED);
        return;
      }
      if (!listener_->send_frame(request.bytes, replay::SIZE)) {
        listener_->on_result(intent.id, Result::REJECTED);
        while (count_)
          listener_->on_result(pop_().id, Result::REJECTED);
        return;
      }
      transmitted_ = true;
      last_tx_ = now;
      sent_at_ = now;
      pending_id_ = intent.id;
      sent_ = request;
      pending_ = true;
      ++writes_;
      listener_->on_result(intent.id, Result::SENT);
    } else {
      // A write takes one transmission opportunity; later ticks keep polling
      // while it awaits readback. A failed write is never automatically resent.
      if (listener_->send_frame(protocol::POLL, sizeof(protocol::POLL))) {
        transmitted_ = true;
        last_tx_ = now;
        ++polls_;
      }
    }
  }

private:
  // field/value applies only to PRESETS with all=false. Mode intents ignore
  // those members. Storing intent rather than encoded bytes preserves later
  // readbacks when building successive queued commands.
  struct Intent {
    uint32_t id{0};
    replay::Action action{replay::Action::ON};
    uint8_t field{0};
    uint16_t value{0};
    bool all{false};
    replay::Settings presets{};
  };
  bool enqueue_(Intent intent, uint32_t now) {
    // Allocate an ID even for rejection so logs identify every request.
    intent.id = ++next_id_;
    if (!ready(now) || count_ == queue_.size()) {
      listener_->on_result(intent.id, Result::REJECTED);
      return false;
    }
    queue_[(head_ + count_) % queue_.size()] = intent;
    ++count_;
    listener_->on_result(intent.id, Result::QUEUED);
    return true;
  }
  Intent pop_() {
    Intent i = queue_[head_];
    head_ = (head_ + 1) % queue_.size();
    --count_;
    return i;
  }
  Listener* listener_;
  FrameParser parser_;
  std::array<uint8_t, 73> raw_{};
  std::array<Intent, 8> queue_{};
  size_t head_{0}, count_{0};
  // have_status_: a snapshot exists; online_: connection notification state;
  // transmitted_: spacing clock initialized; pending_: write awaits readback.
  bool have_status_{false}, online_{false}, transmitted_{false}, pending_{false};
  uint32_t last_status_{0}, last_tx_{0}, sent_at_{0}, next_id_{0}, pending_id_{0};
  uint32_t valid_frames_{0}, writes_{0}, polls_{0};
  uint32_t poll_interval_{1000}, status_timeout_{10000}, command_timeout_{8000};
  replay::Request sent_{};
};
} // namespace panasonic_erv
} // namespace esphome
