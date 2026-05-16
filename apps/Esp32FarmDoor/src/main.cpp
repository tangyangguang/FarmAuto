#include <Arduino.h>

#include "FarmDoorApp.h"

void setup() {
  FarmDoor.begin();
}

void loop() {
  FarmDoor.handle();
}
