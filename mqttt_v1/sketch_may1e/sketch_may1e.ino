/*  ──────────────────────────────────────────────
    Dual-mode setup (Soft-AP portal ➊ or STA ➋)
    © 2025 – Tiny, self-contained, no SPIFFS needed
    Tested with Arduino-ESP8266 core 3.x
    ────────────────────────────────────────────── */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Ticker.h>

/* ========= persistent layout ========= */
#define EEPROM_SIZE  96            //  1 flag + 32 SSID + 32 PW + spare
#define FLAG_ADDR    0             // 0 = none, 1 = HOTSPOT, 2 = STA
#define SSID_ADDR    1
#define PW_ADDR      33

/* ========= globals ========= */
ESP8266WebServer server(80);
Ticker portalTimeout;

/* ========= helpers (EEPROM & reboot) ========= */
void saveFlag(uint8_t v){ EEPROM.write(FLAG_ADDR, v); EEPROM.commit(); }

void write32(int addr, const String& src) {
  for (uint8_t i = 0; i < 32; i++) EEPROM.write(addr + i, i < src.length() ? src[i] : 0);
}
String read32(int addr) {
  char buf[33] = {0};
  for (uint8_t i = 0; i < 32; i++) buf[i] = EEPROM.read(addr + i);
  return String(buf);
}
void rebootSoon() { delay(400); ESP.restart(); }

/* ========= CORS (lets local HTML call the API) ========= */
void beginResponse() { server.sendHeader("Access-Control-Allow-Origin", "*"); }

/* ========= /config1  (keep hotspot) ========= */
void cfgHotspot() {
  Serial.println(F("[API] /config1  → keep-hotspot requested"));
  saveFlag(1);
  beginResponse();
  server.send(200, "text/plain", "Hot-spot mode saved");
  rebootSoon();
}

/* ========= /config2  (save STA creds) ========= */
void cfgSta() {
  String ss = server.arg("ssid");
  String pw = server.arg("pw");
  if (!ss.length()) { beginResponse(); server.send(400, "text/plain", "ssid missing"); return; }

  Serial.printf("[API] /config2  → new STA creds: \"%s\"\n", ss.c_str());
  write32(SSID_ADDR, ss);
  write32(PW_ADDR, pw);
  saveFlag(2);

  beginResponse();
  server.send(200, "text/plain", "Wi-Fi saved – rebooting…");
  rebootSoon();
}

/* ========= soft-AP portal ========= */
void startPortal() {
  String apName = "ESP-SETUP-" + String(ESP.getChipId(), HEX);
  WiFi.softAP(apName.c_str(), "12345678");
  IPAddress ip = WiFi.softAPIP();
  Serial.printf("[AP ] Soft-AP \"%s\" (PW:12345678)  IP=%s\n",
                apName.c_str(), ip.toString().c_str());

  server.on("/config1", [](){ beginResponse(); cfgHotspot(); });
  server.on("/config2", [](){ beginResponse(); cfgSta(); });
  server.onNotFound([](){ beginResponse(); server.send(404,"text/plain",""); });
  server.begin();

  /* auto-close portal after 60 s if nobody used it */
  portalTimeout.once(60, [](){
      Serial.println("[AP ] Portal timeout reached");
      uint8_t f = EEPROM.read(FLAG_ADDR);
      String ss = read32(SSID_ADDR);
      if (f == 1) { Serial.println("[AP ] Staying in hotspot per user choice"); return; }
      if (ss.length()) rebootSoon();   // try STA on next boot
  });
}

/* ========= setup ========= */
void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);

  uint8_t flag   = EEPROM.read(FLAG_ADDR);
  String ssSaved = read32(SSID_ADDR);
  String pwSaved = read32(PW_ADDR);
  Serial.printf("[BOOT] flag=%u  ssid=\"%s\"\n", flag, ssSaved.c_str());

  /* try STA if it was the last valid mode */
  if (flag == 2 && ssSaved.length()) {
    Serial.println("[BOOT] attempting STA connection…");
    WiFi.begin(ssSaved.c_str(), pwSaved.c_str());

    WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& e){
        Serial.print("[EVENT] Got IP "); Serial.println(e.ip);
    });

    if (WiFi.waitForConnectResult(7000) == WL_CONNECTED) {
      Serial.print("[BOOT] STA connected, IP = ");
      Serial.println(WiFi.localIP());
      return;                           // >>> enter normal loop
    }
    Serial.println("[BOOT] STA failed, falling back to portal");
  }

  /* otherwise open Soft-AP portal for 60 s */
  startPortal();
}

/* ========= main loop ========= */
void loop() {
  server.handleClient();         // does nothing in STA-only mode
  /* your normal application code could run here when connected */
}
