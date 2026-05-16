#pragma once

class FarmFeederApp {
 public:
  void begin();
  void handle();

 private:
  void configureStaticDefaults();
  void configureAppConfigPage();
  void configureBusinessShell();
  static void sendStatusJson();
  static void sendSchedulesJson();
  static void sendBucketsJson();
  static void sendBaseInfoJson();
  static void handleScheduleSkip();
  static void handleScheduleCancelSkip();
};

extern FarmFeederApp FarmFeeder;
