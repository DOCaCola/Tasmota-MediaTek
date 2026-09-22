// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace mt7697 { namespace remote {
uint16_t le16(const uint8_t* p);
void rc4(const uint8_t* key, size_t key_size, const uint8_t* in, uint8_t* out, size_t size);
void wrap_token(bool second, const uint8_t mac[6], uint16_t pid,
                const uint8_t in[12], uint8_t out[12]);

// Views are valid only while the owned advertising packet remains alive.
struct Beacon {
  uint16_t pid = 0, flags = 0;
  uint8_t sequence = 0, mac[6] = {};
  const uint8_t* frame = nullptr;
  size_t size = 0, payload_offset = 0;
};
bool parse(const uint8_t* ad, size_t size, const uint8_t address[6], Beacon& out);
bool pairing_request(const Beacon& beacon);
// Legacy MiBeacon uses a 12-byte key and a truncated one-byte CCM tag.
// Returns zero unless the whole authenticated object stream is well formed.
size_t decrypt(const Beacon& beacon, const uint8_t key[12], uint8_t out[24]);

enum class Op : uint8_t { None, Write, Subscribe, Read, Done, Failed };
struct Request {
  Op op = Op::None;
  uint16_t uuid = 0;
  uint8_t size = 0, data[12] = {};
};
// Transport-independent, one outstanding GATT operation. A notification can
// precede its write response; it is retained until that response succeeds.
class Authentication {
 public:
  Request begin(const uint8_t mac[6], uint16_t pid, const uint8_t token[12], bool login);
  Request complete(bool success, const uint8_t* data = nullptr, size_t size = 0);
  Request notification(const uint8_t* data, size_t size);
  Request abort();
  const uint8_t* key() const { return key_; }
  const uint8_t* token() const { return token_; }
 private:
  enum class State : uint8_t { Idle, Hello, Subscribe, Token, Proof, Finish,
                              LoginHello, Challenge, LoginReply, Ack, Key, Done, Failed };
  Request write(uint16_t uuid, const uint8_t* data, size_t size);
  Request consume_notification(const uint8_t* data, size_t size);
  State state_ = State::Idle;
  bool login_ = false;
  uint16_t pid_ = 0;
  uint8_t mac_[6] = {}, token_[12] = {}, session_[12] = {}, key_[12] = {};
  uint8_t early_[12] = {}, early_size_ = 0;
};
} }
