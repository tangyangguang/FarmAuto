#pragma once

class FarmFeederApp {
 public:
  void begin();
  void handle();

 private:
  void configureStaticDefaults();
  void configureAppConfigPage();
  void configureBusinessShell();
  void handleScheduleTick();
  static void sendStatusJson();
  static void sendSchedulesJson();
  static void sendBucketsJson();
  static void sendBaseInfoJson();
  static void sendTargetsJson();
  static void sendRecordsJson();
  static void handleFeederTarget();
  static void handleFeederManualStart();
  static void handleFeederStart();
  static void handleFeederStop();
  static void handleFeederStopAll();
  static void handleScheduleCreate();
  static void handleScheduleUpdate();
  static void handleScheduleDelete();
  static void handleScheduleSkip();
  static void handleScheduleCancelSkip();
  static void handleBucketSetRemaining();
  static void handleBucketAddFeed();
  static void handleBucketMarkFull();
  static void handleBaseInfoChannel();
  static void handleMaintenanceClearToday();
  static void handleMaintenanceClearFault();
};

extern FarmFeederApp FarmFeeder;
