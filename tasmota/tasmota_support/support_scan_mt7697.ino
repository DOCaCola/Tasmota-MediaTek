// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef TASMOTA_PLATFORM_MT7697N
#include <atomic>
extern "C" {
#include <wifi_api.h>
}

// The driver owns this static buffer until scan_deinit succeeds. Never expose
// partially filled entries or free storage that an in-flight callback can use.
struct NativeScanState {
  wifi_scan_list_item_t entries[16];
  std::atomic<bool> complete{false};
  uint32_t started = 0;
  bool registered = false;
  bool attached = false;
  bool running = false;
  bool valid = false;
  bool fault = false;
} NativeScan;

int32_t NativeScanEvent(wifi_event_t event, uint8_t*, uint32_t) {
  if (event == WIFI_EVENT_IOT_SCAN_COMPLETE) {
    NativeScan.complete.store(true, std::memory_order_release);
  }
  return 0;
}

bool NativeScanRelease(void) {
  bool ok = true;
  if (NativeScan.attached) {
    if (wifi_connection_scan_deinit() < 0) { ok = false; }
    else { NativeScan.attached = false; }
  }
  if (NativeScan.registered) {
    if (wifi_connection_unregister_event_handler(WIFI_EVENT_IOT_SCAN_COMPLETE, NativeScanEvent) < 0) {
      ok = false;
    } else { NativeScan.registered = false; }
  }
  NativeScan.fault = !ok;
  return ok;
}

void NativeScanPoll(void) {
  if (!NativeScan.running) { return; }
  const bool done = NativeScan.complete.load(std::memory_order_acquire);
  if (!done && uint32_t(millis() - NativeScan.started) < 15000) { return; }
  if (!done && wifi_connection_stop_scan() < 0) {
    NativeScan.running = false;
    NativeScan.fault = true;
    AddLog(LOG_LEVEL_ERROR, PSTR("WIF: Scan stop failed; buffer retained until reboot"));
    return;
  }
  NativeScan.running = false;
  NativeScan.valid = NativeScanRelease() && done;
  if (!NativeScan.valid) {
    AddLog(LOG_LEVEL_ERROR, PSTR("WIF: Scan timed out or cleanup failed"));
  }
}

void CmndWifiScan(void) {
  NativeScanPoll();
  if (NativeScan.fault) {
    ResponseCmndChar(PSTR("Scan cleanup failed; restart required"));
    return;
  }
  if (NativeScan.running) {
    ResponseCmndChar(D_JSON_BUSY);
    return;
  }
  if (XdrvMailbox.data_len) {
    // Initial radio initialization remains owned by the station lifecycle.
    if (!wifi_ready() || !Settings->flag4.network_wifi) {
      ResponseCmndChar(PSTR("Station radio is not ready"));
      return;
    }
    NativeScan.valid = false;
    NativeScan.complete.store(false, std::memory_order_release);
    memset(NativeScan.entries, 0, sizeof(NativeScan.entries));
    if (wifi_connection_register_event_handler(WIFI_EVENT_IOT_SCAN_COMPLETE, NativeScanEvent) < 0) {
      ResponseCmndFailed();
      return;
    }
    NativeScan.registered = true;
    if (wifi_connection_scan_init(NativeScan.entries, 16) < 0) {
      NativeScanRelease();
      ResponseCmndFailed();
      return;
    }
    NativeScan.attached = true;
    if (wifi_connection_start_scan(nullptr, 0, nullptr, 0, 0) < 0) {
      NativeScanRelease();
      ResponseCmndFailed();
      return;
    }
    NativeScan.started = millis();
    NativeScan.running = true;
    ResponseCmndChar(D_JSON_SCANNING);
    return;
  }
  if (!NativeScan.valid) {
    ResponseCmndChar(D_JSON_NOT_STARTED);
    return;
  }
  Response_P(PSTR("{\"WifiScan\":["));
  bool comma = false;
  bool truncated = false;
  for (const auto& entry : NativeScan.entries) {
    if (!entry.is_valid || entry.ssid_length > 32) { continue; }
    char ssid[33];
    memcpy(ssid, entry.ssid, entry.ssid_length);
    ssid[entry.ssid_length] = '\0';
    // The SDK already orders by descending RSSI. Leave room for the closing
    // JSON, even with the worst-case escaped SSID.
    if (ResponseSize() < ResponseLength() + 360) { truncated = true; break; }
    ResponseAppend_P(PSTR("%s{\"SSID\":\"%s\",\"BSSID\":\"%02X:%02X:%02X:%02X:%02X:%02X\","
                          "\"Channel\":%u,\"RSSI\":%d,\"Authentication\":%u,\"Encryption\":%u}"),
      comma ? "," : "", EscapeJSONString(ssid).c_str(),
      entry.bssid[0], entry.bssid[1], entry.bssid[2], entry.bssid[3], entry.bssid[4], entry.bssid[5],
      entry.channel, entry.rssi, entry.auth_mode, entry.encrypt_type);
    comma = true;
  }
  ResponseAppend_P(PSTR("],\"Truncated\":%s,\"ScanCapacity\":16}"), truncated ? "true" : "false");
}
#endif
