// SPDX-License-Identifier: GPL-3.0-or-later
// Temporary SDK bring-up tracing. Observes allocation/configuration boundaries;
// never changes SDK state, credentials, return values or the invalid pointer.
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
extern "C" {
#include <FreeRTOS.h>
#include <task.h>
#include <hal_uart.h>
void* __real_wpa_supplicant_add_iface(void*, void*);
void* __real_os_zalloc(size_t);
void* __real_wpa_config_read(const char*);
void* __real_wpa_config_alloc_new_conf(void*);
size_t __real_os_strlcpy(char*, const char*, size_t);
}
namespace {
// Verified against the linked SDK's DWARF by supplicant_trace_test.py.
constexpr size_t station_size = 616;
constexpr size_t eapol_offset = 296;
TaskHandle_t owner;
void* station_context;
void report(const char* phase) {
  if (!station_context || owner != xTaskGetCurrentTaskHandle()) return;
  const uint32_t eapol = *reinterpret_cast<volatile uint32_t*>(
      static_cast<uint8_t*>(station_context) + eapol_offset);
  char line[112];
  snprintf(line,sizeof(line),"\r\nSDKTRACE %s station=%08lx eapol=%08lx\r\n",
      phase,static_cast<unsigned long>(reinterpret_cast<uintptr_t>(station_context)),
      static_cast<unsigned long>(eapol));
  for (const char* p=line; *p; ++p) hal_uart_put_char(HAL_UART_0,*p);
}
}
extern "C" {
void* __wrap_wpa_supplicant_add_iface(void* global, void* iface) {
  owner=xTaskGetCurrentTaskHandle();
  station_context=nullptr;
  void* result=__real_wpa_supplicant_add_iface(global,iface);
  // An unsuccessful creation may already have freed the context.
  if (result) report("created");
  station_context=nullptr;
  owner=nullptr;
  return result;
}
void* __wrap_os_zalloc(size_t size) {
  void* result=__real_os_zalloc(size);
  if (owner && owner==xTaskGetCurrentTaskHandle() && !station_context && size==station_size) {
    station_context=result;
    report("allocated");
  }
  return result;
}
void* __wrap_wpa_config_read(const char* name) {
  report("before-config");
  void* result=__real_wpa_config_read(name);
  report("after-config");
  return result;
}
void* __wrap_wpa_config_alloc_new_conf(void* previous) {
  report("before-copy-config");
  void* result=__real_wpa_config_alloc_new_conf(previous);
  report("after-copy-config");
  return result;
}
size_t __wrap_os_strlcpy(char* destination,const char* source,size_t size) {
  size_t result=__real_os_strlcpy(destination,source,size);
  report("after-string-copy");
  return result;
}
}
