// SPDX-License-Identifier: GPL-3.0-or-later
#include "../arduino/IPAddress.h"
#include "../arduino/Print.h"
#include <cassert>
#include <cstdio>
#include <string>

class Output : public Print {
 public:
  std::string bytes;
  size_t write(uint8_t byte) override {
    bytes.push_back(static_cast<char>(byte));
    return 1;
  }
};

int main() {
  IPAddress ip(10, 9, 8, 95);
  for (const char* invalid : {"", "1", "1.2.3", "1.2.3.4.5", "256.2.3.4",
                               "-1.2.3.4", " 1.2.3.4", "1.2.3.4x", "1..3.4"}) {
    assert(!ip.fromString(invalid));
    assert(ip == IPAddress(10, 9, 8, 95));
  }
  assert(ip.fromString("0.255.100.1"));
  assert(ip == IPAddress(0, 255, 100, 1));
  Output output;
  assert(output.printf("%s:%d %.2f", "lamp", 42, 1.25) == 12);
  assert(output.bytes == "lamp:42 1.25");
  assert(output.printf("%s", "") == 0);
  std::string long_text(4096, 'x');
  assert(output.printf("%s", long_text.c_str()) == long_text.size());
  assert(output.bytes.substr(12) == long_text);
  String name("lamp");
  assert(name.concat("-mt7697-trailing", 7));
  assert(name == "lamp-mt7697");
  assert(name.concat(name.c_str(), name.length()));
  assert(name == "lamp-mt7697lamp-mt7697");
  const char raw[] = {'a', 'b', 'c'};
  assert(name.concat(raw, sizeof(raw)));
  assert(name == "lamp-mt7697lamp-mt7697abc");
  puts("Arduino tests passed: strict IPv4 parsing, failed-parse preservation, formatted output, bounded String append.");
}
