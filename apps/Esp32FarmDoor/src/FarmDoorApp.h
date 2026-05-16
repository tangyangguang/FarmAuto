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
  static void sendStatusJson();
  static void sendDiagnosticsJson();
  static void handleDoorOpen();
  static void handleDoorClose();
  static void handleDoorStop();
  static void handleSetPosition();
  static void handleSetTravel();
  static void handleAdjustTravel();
  static void handleClearFault();
};

extern FarmDoorApp FarmDoor;
