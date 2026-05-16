#include <Arduino.h>

#include "FarmFeederApp.h"

void setup() {
  FarmFeeder.begin();
}

void loop() {
  FarmFeeder.handle();
}
