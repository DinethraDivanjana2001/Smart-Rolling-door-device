#include <EEPROM.h>

void setup() {
  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0xFF);  // Use 0x00 if you want to zero it instead
  }
}

void loop() {
  // Nothing to do here
}
