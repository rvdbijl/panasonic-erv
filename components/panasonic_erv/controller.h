#pragma once
#include "replay_protocol.h"
#include <array>
#include <cstring>

namespace esphome {
namespace panasonic_erv {
class FrameParser {
public:
  template <typename Callback> void feed(uint8_t byte, uint32_t now, Callback callback) {
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
class Listener {
public:
  virtual ~Listener() = default;
  virtual bool send_frame(const uint8_t* bytes, size_t size) = 0;
  virtual void on_status(const protocol::Status& status, const uint8_t* raw) = 0;
  virtual void on_result(uint32_t id, Result result) = 0;
  virtual void on_connection(bool connected) = 0;
};

class Controller {
public:
  explicit Controller(Listener* listener) : listener_(listener) {}
  void set_poll_interval(uint32_t ms) { poll_interval_ = ms; }
  void set_status_timeout(uint32_t ms) { status_timeout_ = ms; }
  void set_command_timeout(uint32_t ms) { command_timeout_ = ms; }
  bool connected(uint32_t now) const {
    return have_status_ && uint32_t(now - last_status_) <= status_timeout_;
  }
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
      if (pending_ && uint32_t(now - sent_at_) >= 136 && replay::matches(sent_, raw_.data(), 73)) {
        pending_ = false;
        listener_->on_result(pending_id_, Result::CONFIRMED);
      }
    });
  }
  void tick(uint32_t now) {
    parser_.expire(now);
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
      if (listener_->send_frame(protocol::POLL, sizeof(protocol::POLL))) {
        transmitted_ = true;
        last_tx_ = now;
        ++polls_;
      }
    }
  }

private:
  struct Intent {
    uint32_t id{0};
    replay::Action action{replay::Action::ON};
    uint8_t field{0};
    uint16_t value{0};
    bool all{false};
    replay::Settings presets{};
  };
  bool enqueue_(Intent intent, uint32_t now) {
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
  bool have_status_{false}, online_{false}, transmitted_{false}, pending_{false};
  uint32_t last_status_{0}, last_tx_{0}, sent_at_{0}, next_id_{0}, pending_id_{0};
  uint32_t valid_frames_{0}, writes_{0}, polls_{0};
  uint32_t poll_interval_{1000}, status_timeout_{10000}, command_timeout_{8000};
  replay::Request sent_{};
};
} // namespace panasonic_erv
} // namespace esphome
