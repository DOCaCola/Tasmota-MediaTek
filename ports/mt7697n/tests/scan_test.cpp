// SPDX-License-Identifier: GPL-3.0-or-later
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <string>
#define TASMOTA_PLATFORM_MT7697N
#define PSTR(x) x
#define LOG_LEVEL_ERROR 1
#define D_JSON_BUSY "Busy"
#define D_JSON_SCANNING "Scanning"
#define D_JSON_NOT_STARTED "Not started"
static uint32_t now;
static bool ready = true;
uint32_t millis() { return now; }
bool wifi_ready() { return ready; }
struct { struct { bool network_wifi = true; } flag4; } settings;
auto* Settings = &settings;
struct { unsigned data_len = 1; } XdrvMailbox;
std::string response;
void ResponseCmndChar(const char* text) { response = text; }
void ResponseCmndFailed() { response = "Failed"; }
void Response_P(const char* text) { response = text; }
void ResponseAppend_P(const char* format, ...) {
  char text[1024];
  va_list args;
  va_start(args, format);
  vsnprintf(text, sizeof(text), format, args);
  va_end(args);
  response += text;
}
static size_t response_limit = 2048;
size_t ResponseSize() { return response_limit; }
size_t ResponseLength() { return response.size(); }
std::string EscapeJSONString(const char* text) { return text; }
void AddLog(unsigned, const char*, ...) {}
#include "../../../tasmota/tasmota_support/support_scan_mt7697.ino"
static int fail_at = 0;
static int stop_calls = 0, deinit_calls = 0, unregister_calls = 0;
extern "C" {
int32_t wifi_connection_scan_init(wifi_scan_list_item_t* data, uint32_t count) {
  assert(data == NativeScan.entries && count == 16); return fail_at == 2 ? -1 : 0;
}
int32_t wifi_connection_scan_deinit() { ++deinit_calls; return fail_at == 4 ? -1 : 0; }
int32_t wifi_connection_start_scan(uint8_t*, uint8_t, uint8_t*, uint8_t, uint8_t) {
  return fail_at == 3 ? -1 : 0;
}
int32_t wifi_connection_stop_scan() { ++stop_calls; return fail_at == 5 ? -1 : 0; }
int32_t wifi_connection_register_event_handler(wifi_event_t, wifi_event_handler_t cb) {
  assert(cb == NativeScanEvent); return fail_at == 1 ? -1 : 0;
}
int32_t wifi_connection_unregister_event_handler(wifi_event_t, wifi_event_handler_t) {
  ++unregister_calls; return fail_at == 6 ? -1 : 0;
}
}
void reset() {
  NativeScan.running = NativeScan.valid = NativeScan.fault = false;
  NativeScan.attached = NativeScan.registered = false;
  NativeScan.complete.store(false);
  fail_at = 0; ready = true; now = 0;
  stop_calls = deinit_calls = unregister_calls = 0;
  XdrvMailbox.data_len = 1;
}
int main() {
  reset(); ready = false; CmndWifiScan();
  assert(!NativeScan.running && response == "Station radio is not ready");
  for (int failure = 1; failure <= 3; ++failure) {
    reset(); fail_at = failure; CmndWifiScan();
    assert(response == "Failed" && !NativeScan.running && !NativeScan.fault);
    assert(!NativeScan.attached && !NativeScan.registered);
  }
  reset(); CmndWifiScan(); assert(NativeScan.running && response == "Scanning");
  CmndWifiScan(); assert(response == "Busy");
  auto& entry = NativeScan.entries[0];
  entry.is_valid = 1; entry.ssid_length = 32; memset(entry.ssid, 'A', 32);
  entry.channel = 6; entry.rssi = -55;
  NativeScan.entries[1].ssid_length = 4; // Invalid entries must not be shown.
  NativeScanEvent(WIFI_EVENT_IOT_SCAN_COMPLETE, nullptr, 0);
  NativeScanPoll();
  assert(NativeScan.valid && !NativeScan.running && deinit_calls == 1 && unregister_calls == 1);
  XdrvMailbox.data_len = 0; CmndWifiScan();
  assert(response.find(std::string(32, 'A')) != std::string::npos);
  assert(response.find("\"Channel\":6") != std::string::npos);
  assert(response.find("\"Truncated\":false") != std::string::npos);
  response_limit = 360;
  CmndWifiScan();
  assert(response.find("\"Truncated\":true") != std::string::npos);
  assert(response.back() == '}');
  response_limit = 2048;
  reset(); now = UINT32_MAX - 1000; CmndWifiScan(); now += 14999; NativeScanPoll();
  assert(NativeScan.running);
  ++now; NativeScanPoll();
  assert(!NativeScan.running && !NativeScan.valid && !NativeScan.fault && stop_calls == 1);
  for (int failure : {4, 5, 6}) {
    reset(); CmndWifiScan(); fail_at = failure; now = 15000; NativeScanPoll();
    assert(NativeScan.fault && !NativeScan.valid);
    CmndWifiScan(); assert(response == "Scan cleanup failed; restart required");
    if (failure == 5) assert(NativeScan.attached && deinit_calls == 0);
  }
  puts("Scan tests passed: async lifecycle, fixed-length SSID, timeout wrap, failures and buffer ownership.");
}
