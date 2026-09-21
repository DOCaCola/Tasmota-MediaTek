// SPDX-License-Identifier: GPL-3.0-or-later
#include "ota_http_fakes.h"
#include <stdarg.h>
#define TASMOTA_PLATFORM_MT7697N
#define PSTR(x) x
#define SET_OTAURL 0
#define LOG_LEVEL_INFO 2
#define STAT 1
#define D_CMND_UPGRADE "Upgrade"
static MemoryFlash flash;
static Transport transport;
namespace mt7697 {
OtaFlash& sdk_ota_flash() { return flash; }
OtaTransport& sdk_ota_transport() { return transport; }
}
static uint32_t now;
uint32_t millis() { return now; }
struct {
  struct { bool network_down = false; } global_state;
  unsigned restart_flag = 0;
} TasmotaGlobal;
struct { unsigned data_len = 1, payload = 1; } XdrvMailbox;
const char* SettingsText(unsigned) { return "http://10.9.8.2/update.bin"; }
static char output[256];
static unsigned published;
void ResponseCmndChar(const char* text) { snprintf(output,sizeof(output),"%s",text); }
void Response_P(const char* format, ...) {
  va_list args; va_start(args,format); vsnprintf(output,sizeof(output),format,args); va_end(args);
}
const char* ResponseData() { return output; }
void AddLog(unsigned,const char*,...) {}
void MqttPublishPrefixTopicRulesProcess_P(unsigned,const char*) { ++published; }
#include "../../../tasmota/tasmota_support/support_ota_mt7697.ino"

int main(int argc, char** argv) {
  assert(argc == 3);
  std::ifstream file(argv[1],std::ios::binary);
  std::vector<uint8_t> package((std::istreambuf_iterator<char>(file)),{});
  const unsigned mode = unsigned(argv[2][0]-'0');
  TasmotaGlobal.global_state.network_down = true;
  NativeOtaCommand(); assert(strstr(output,"offline") && !flash.writes);
  TasmotaGlobal.global_state.network_down = false;
  XdrvMailbox.payload = 0; NativeOtaCommand(); assert(strstr(output,"Upgrade 1"));
  XdrvMailbox.payload = 1;
  TasmotaGlobal.restart_flag = 2; NativeOtaCommand(); assert(strstr(output,"pending"));
  TasmotaGlobal.restart_flag = 0;
  flash.activation_ok = mode == 0;
  if (mode == 2) package.back() ^= 1;
  transport.response = response(package);
  if (mode == 3) transport.response.resize(transport.response.size()-1);
  NativeOtaCommand(); assert(native_ota_report_pending);
  NativeOtaCommand(); assert(strstr(output,"pending"));
  for (now=0; now < 20000 && native_ota_report_pending; ++now) NativeOtaPoll();
  assert(!native_ota_report_pending && published==1);
  NativeOtaPoll(); assert(published==1);
  if (mode == 0) {
    assert(TasmotaGlobal.restart_flag==2 && flash.activations==1 && strstr(output,"activated"));
  } else {
    assert(!TasmotaGlobal.restart_flag);
    assert(flash.activations==(mode==1 ? 1U : 0U));
  }
  puts("OTA command test passed: reporting and restart gated on verified activation");
}
