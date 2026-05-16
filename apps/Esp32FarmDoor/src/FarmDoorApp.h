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
  static void sendHomePage();
  static void sendRecordsPage();
  static void sendCalibrationPage();
  static void sendDiagnosticsPage();
  static void sendStatusJson();
  static void sendDiagnosticsJson();
  static void sendRecentEventsJson();
  static void sendRecordsJson();
  static void handleDoorOpen();
  static void handleDoorClose();
  static void handleDoorStop();
  static void handleSetPosition();
  static void handleSetTravel();
  static void handleAdjustTravel();
  static void handleClearFault();
};

extern FarmDoorApp FarmDoor;
