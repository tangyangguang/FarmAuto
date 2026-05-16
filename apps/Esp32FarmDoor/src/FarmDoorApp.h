#pragma once

class FarmDoorApp {
 public:
  void begin();
  void handle();

 private:
  void configureStaticDefaults();
  void configureHardwareInputs();
  void configureAppConfigPage();
  void configureBusinessShell();
};

extern FarmDoorApp FarmDoor;
