/* ─────────────────────────────────────────────────────────────
   Dual-Mode Configuration Portal (always 60-s Soft-AP)
   tested with Arduino-ESP8266 core 3.x
   ───────────────────────────────────────────────────────────── */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Ticker.h>

/* ================= persistent layout ================= */
#define EEPROM_SIZE   96          // 1 flag + 32 SSID + 32 PW
#define FLAG_ADDR     0           // 0 = none, 1 = HOTSPOT, 2 = STA
#define SSID_ADDR     1
#define PW_ADDR       33

/* ================= globals ================= */
ESP8266WebServer server(80);
Ticker            finaliseTimer;

/* ================= EEPROM helpers ================= */
void saveFlag(uint8_t v) { EEPROM.write(FLAG_ADDR, v); EEPROM.commit(); }

void write32(int addr, const String& s) {
  for (uint8_t i = 0; i < 32; i++) EEPROM.write(addr + i, i < s.length() ? s[i] : 0);
}
String read32(int addr) {
  char buf[33] = {0};
  for (uint8_t i = 0; i < 32; i++) buf[i] = EEPROM.read(addr + i);
  return String(buf);
}

void rebootSoon() { delay(400); ESP.restart(); }

void cors() { server.sendHeader("Access-Control-Allow-Origin", "*"); }

/* ================= /config1  → keep hotspot ================= */
void cfgHotspot() {
  Serial.println(F("[API] /config1  → keep-hotspot requested"));
  saveFlag(1);                                // flag = HOTSPOT
  cors(); server.send(200, "text/plain", "Hot-spot mode saved");
  rebootSoon();
}

/* ================= /config2  → save STA creds ================= */
void cfgSta() {
  String ss = server.arg("ssid");
  String pw = server.arg("pw");
  if (!ss.length()) { cors(); server.send(400, "text/plain", "ssid missing"); return; }

  Serial.printf("[API] /config2  → new STA creds: \"%s\"\n", ss.c_str());
  write32(SSID_ADDR, ss); write32(PW_ADDR, pw); saveFlag(2);      // flag = STA
  cors(); server.send(200, "text/plain", "Wi-Fi saved – rebooting…");
  rebootSoon();
}

/* ================= 60-second Soft-AP portal ================= */
void startPortal() {
  String ap = "ESP-SETUP-" + String(ESP.getChipId(), HEX);
  WiFi.mode(WIFI_AP);                         // AP only for now
  WiFi.softAP(ap.c_str(), "12345678");
  Serial.printf("[AP ] Soft-AP \"%s\" (PW:12345678)  IP=%s\n",
                ap.c_str(), WiFi.softAPIP().toString().c_str());

  server.on("/config1", []() { cors(); cfgHotspot(); });
  server.on("/config2", []() { cors(); cfgSta(); });
  server.onNotFound([]() { cors(); server.send(404, "text/plain", ""); });
  server.begin();
}

/* ================= what happens after 60 s ================= */
void finalisePortal() {
  uint8_t flag = EEPROM.read(FLAG_ADDR);
  String  ss   = read32(SSID_ADDR);
  Serial.printf("[AP ] Finalising… flag=%u  ssid=\"%s\"\n", flag, ss.c_str());

  if (flag == 1) {                           // stay AP permanently
    Serial.println("[AP ] Staying in Hot-spot mode");
    return;
  }

  if (flag == 2 && ss.length()) {            // switch to STA
    Serial.println("[AP ] Switching to STA (joining Wi-Fi) …");
    String pw = read32(PW_ADDR);
    WiFi.softAPdisconnect(true);             // turn AP off
    WiFi.mode(WIFI_STA);
    WiFi.begin(ss.c_str(), pw.c_str());

    WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP &e) {
      Serial.print("[EVENT] Got IP "); Serial.println(e.ip);
    });

  } else {                                   // no valid creds → fallback AP
    Serial.println("[AP ] No valid Wi-Fi creds, keeping Hot-spot");
    saveFlag(1);                             // force hotspot so next boot knows
  }
}

/* ================= Arduino setup ================= */
void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);

  /* ❶ always open Soft-AP portal first */
  startPortal();

  /* ❷ schedule closure in 60 s */
  finaliseTimer.once(60, finalisePortal);
}

/* ================= main loop ================= */
void loop() {
  server.handleClient();
  /* when STA is connected you can run your normal application here */
}
