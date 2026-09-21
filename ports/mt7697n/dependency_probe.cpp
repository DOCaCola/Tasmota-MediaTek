// Compile/link and future runtime checks; this is not the Tasmota application.
#include <Arduino.h>
#include <LWiFi.h>
#include <PubSubClient.h>
#include <JsonParser.h>
#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>
#include <tuple>
#include <vector>
#include <sys/reent.h>

static_assert(sizeof(struct _reent) == 240, "SDK newlib ABI required");
static_assert(offsetof(struct _reent, __cleanup) == 40, "SDK newlib ABI required");
static_assert(sizeof(__FILE) == 104, "SDK stdio ABI required");
static_assert(sizeof(void*) == 4, "32-bit target required");

template <typename T> constexpr T twice(T value) {
  if constexpr (sizeof(T) == 4) {
    return value + value;
  } else {
    return value * 2;
  }
}
static_assert(twice(21) == 42);

bool check_tasmota_dependencies() {
  // Exercise allocation and destruction across the C++/SDK heap boundary.
  std::vector<int> values{3, 1, 2};
  std::sort(values.begin(), values.end());
  std::function<int(int)> transform = [](int value) { return twice(value); };
  std::string text("MT7697");
  text += "N";
  auto [number, valid] = std::make_tuple(transform(values[2]), text == "MT7697N");

  WiFiClient transport;
  PubSubClient mqtt(transport);
  mqtt.setServer("invalid.invalid", 1883);
  // No connection or credentials: validate allocation/configuration locally.
  bool allocated = mqtt.setBufferSize(1536);
  char payload[] = "{\"Build\":7697,\"Ready\":true}";
  JsonParser parser(payload);
  JsonParserObject root = parser.getRootObject();
  bool parsed = root["Build"].getUInt() == 7697 && root["Ready"].getBool();
  return number == 6 && valid && allocated && mqtt.getBufferSize() == 1536 && parsed;
}
