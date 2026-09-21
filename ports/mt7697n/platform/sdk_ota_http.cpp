// SPDX-License-Identifier: GPL-3.0-or-later
#include "ota_http.h"
extern "C" {
#include <lwip/sockets.h>
}
#include <errno.h>
#include <string.h>

namespace mt7697 {
namespace {
class SocketTransport final : public OtaTransport {
 public:
  bool connect(const OtaUrl& url) override {
    socket_ = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_ < 0) return false;
    unsigned long nonblocking = 1;
    if (lwip_ioctl(socket_, FIONBIO, &nonblocking) < 0) { close(); return false; }
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = lwip_htons(url.port);
    memcpy(&address.sin_addr.s_addr, url.ip, 4);
    if (lwip_connect(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) return true;
    if (errno == EINPROGRESS || errno == EWOULDBLOCK) return true;
    close(); return false;
  }
  int connected() override {
    fd_set write_set, error_set;
    FD_ZERO(&write_set); FD_ZERO(&error_set);
    FD_SET(socket_, &write_set); FD_SET(socket_, &error_set);
    timeval wait = {};
    const int ready = lwip_select(socket_ + 1, nullptr, &write_set, &error_set, &wait);
    if (ready <= 0) return ready;
    int error = 0;
    socklen_t length = sizeof(error);
    if (lwip_getsockopt(socket_, SOL_SOCKET, SO_ERROR, &error, &length) < 0 || error ||
        FD_ISSET(socket_, &error_set)) return -1;
    return FD_ISSET(socket_, &write_set) ? 1 : 0;
  }
  int send(const uint8_t* bytes, size_t size) override {
    const int n = lwip_send(socket_, bytes, size, 0);
    return n < 0 && (errno == EWOULDBLOCK || errno == EAGAIN) ? 0 : n;
  }
  int receive(uint8_t* bytes, size_t size) override {
    const int n = lwip_recv(socket_, bytes, size, MSG_DONTWAIT);
    if (n == 0) return -1;  // EOF before Content-Length is complete.
    return n < 0 && (errno == EWOULDBLOCK || errno == EAGAIN) ? 0 : n;
  }
  void close() override {
    if (socket_ >= 0) lwip_close(socket_);
    socket_ = -1;
  }
 private:
  int socket_ = -1;
};
SocketTransport transport;
}
OtaTransport& sdk_ota_transport() { return transport; }
}  // namespace mt7697
