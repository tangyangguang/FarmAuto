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
};

extern FarmFeederApp FarmFeeder;
