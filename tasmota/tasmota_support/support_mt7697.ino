// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef TASMOTA_PLATFORM_MT7697N
// These legacy-named common-core entry points are implemented by each platform.
uint32_t ESP_ResetInfoReason(void) {
  return static_cast<uint32_t>(mt7697::reset_reason());
}
String ESP_getResetReason(void) { return String(mt7697::reset_reason_text()); }
uint32_t ESP_getFreeHeap(void) { return mt7697::free_heap(); }
uint32_t ESP_getFreeHeap1024(void) { return mt7697::free_heap() / 1024; }
String GetDeviceHardware(void) { return String("MT7697N"); }
String GetDeviceHardwareRevision(void) { return String("YLXD01YL / MT7697N"); }
String GetCodeCores(void) { return String("-ARM"); }
void EspRestart(void) {
  mt7697::prepare_restart();
  Serial.flush();
  mt7697::restart();
}
#endif
