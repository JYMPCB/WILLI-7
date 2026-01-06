#pragma once
#include <Arduino.h>
#include <Preferences.h>

struct ServiceState {
  uint32_t last_ack_epoch = 0; // último aceptar
  bool     pending = false;    // hay popup pendiente (vencido o postergado)
  bool     postponed = false;  // el usuario presionó "Postergar"
};

class ServiceMgr {
public:
  void begin();
  bool evaluate_on_boot(time_t now_epoch); // si venció → pending=true (postponed=false)
  void onPostpone();                       // pending=true, postponed=true
  void onAccept(time_t now_epoch);         // pending=false, postponed=false, last_ack=now

  bool pending() const { return st.pending; }

  void formatConfigLabel(char* out, size_t n, time_t now_epoch, bool time_ok) const;
  void formatPopupText(char* out, size_t n, time_t now_epoch) const;

private:
  Preferences prefs;
  ServiceState st;

  static constexpr uint32_t SERVICE_DAYS = 45;
  static constexpr uint32_t SERVICE_SEC  = SERVICE_DAYS * 86400UL;

  int32_t days_left(time_t now_epoch) const;
  void save();
};