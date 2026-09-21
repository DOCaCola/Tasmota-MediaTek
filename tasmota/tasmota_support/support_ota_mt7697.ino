// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef TASMOTA_PLATFORM_MT7697N
#include <platform/ota_http.h>

// Function-local ownership avoids cross-translation-unit initialization order.
mt7697::OtaStager& NativeOtaStager() {
  static mt7697::OtaStager stager(mt7697::sdk_ota_flash());
  return stager;
}
mt7697::OtaDownload& NativeOtaDownload() {
  static mt7697::OtaDownload download(mt7697::sdk_ota_transport(), NativeOtaStager());
  return download;
}
bool native_ota_report_pending = false;

void NativeOtaCommand() {
  if (NativeOtaDownload().busy() || NativeOtaDownload().state() == mt7697::OtaDownloadState::Ready ||
      TasmotaGlobal.restart_flag) {
    ResponseCmndChar(PSTR("Update or restart already pending"));
  } else if (XdrvMailbox.data_len != 1 || XdrvMailbox.payload != 1) {
    ResponseCmndChar(PSTR("Set OtaUrl to an HTTP IPv4 MMM package, then Upgrade 1"));
  } else if (TasmotaGlobal.global_state.network_down) {
    ResponseCmndChar(PSTR("Network is offline"));
  } else if (!NativeOtaDownload().start(SettingsText(SET_OTAURL), millis())) {
    ResponseCmndChar(NativeOtaDownload().error());
  } else {
    native_ota_report_pending = true;
    ResponseCmndChar(PSTR("Downloading native OTA package"));
  }
}

void NativeOtaPoll() {
  auto& download = NativeOtaDownload();
  download.poll(millis(), !TasmotaGlobal.global_state.network_down);
  if (!native_ota_report_pending || download.busy()) return;
  native_ota_report_pending = false;
  if (download.state() == mt7697::OtaDownloadState::Ready) {
    const auto result = NativeOtaStager().activate();
    if (result == mt7697::OtaResult::Ok) {
      Response_P(PSTR("{\"Upgrade\":\"Verified and activated; restarting\"}"));
      TasmotaGlobal.restart_flag = 2;
    } else {
      Response_P(PSTR("{\"Upgrade\":\"Activation failed\",\"Code\":%u}"), unsigned(result));
    }
  } else {
    Response_P(PSTR("{\"Upgrade\":\"%s\",\"Code\":%u}"), download.error(), unsigned(download.staging_result()));
  }
  AddLog(LOG_LEVEL_INFO, PSTR("OTA: %s"), ResponseData());
  MqttPublishPrefixTopicRulesProcess_P(STAT, PSTR(D_CMND_UPGRADE));
}
#endif
