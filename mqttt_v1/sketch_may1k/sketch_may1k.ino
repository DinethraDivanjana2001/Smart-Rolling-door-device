#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Ticker.h>
#include <PubSubClient.h>

/* hardware */
#define RELAY1_PIN 12
#define RELAY2_PIN 14
#define PULSE_MS   100

/* EEPROM layout */
#define EE_SIZE     224
#define FLG_ADDR     0   // 1=Hotspot,2=LAN,3=MQTT
#define SSID_ADDR    1   // 32 bytes
#define PW_ADDR     33   // 32 bytes
#define MACO_ADDR   97   // 32 bytes override MAC

enum Modes { NONE=0, HS=1, LAN=2, MQTT=3 };

ESP8266WebServer web(80);
WiFiClient       wc;
PubSubClient     mqtt(wc);
Ticker           portalTimer;

uint8_t modeFlag;
bool    staConnecting = false;
unsigned staStartMs   = 0;
String  macUsed;

// Helpers to read/write 32-byte strings in EEPROM
void wr32(int a, const String &s) {
  for (uint8_t i = 0; i < 32; i++) EEPROM.write(a + i, i < s.length() ? s[i] : 0);
}
String rd32(int a) {
  char buf[33] = {0};
  for (uint8_t i = 0; i < 32; i++) buf[i] = EEPROM.read(a + i);
  return String(buf);
}
void saveFlag(uint8_t f) {
  EEPROM.write(FLG_ADDR, f);
  EEPROM.commit();
  modeFlag = f;
}
void cors() {
  web.sendHeader("Access-Control-Allow-Origin", "*");
}

// Pulse relay channel `ch` (1 or 2) for 100 ms
void pulseRelay(int ch) {
  uint8_t pin = (ch == 1) ? RELAY1_PIN : RELAY2_PIN;
  digitalWrite(pin, HIGH);
  delay(PULSE_MS);
  digitalWrite(pin, LOW);
}

// ─── HTTP: /relay?ch=1|2 ───
void handleRelay() {
  int ch = web.arg("ch").toInt();
  if (ch < 1 || ch > 2) {
    cors();
    web.send(400, "text/plain", "bad arg");
    return;
  }
  pulseRelay(ch);
  cors();
  web.send(200, "text/plain", "PULSED");
}

// ─── HTTP: /config1 ───
void handleCfg1() {
  saveFlag(HS);
  cors();
  web.send(200, "text/plain", "flag1");
}

// ─── HTTP: /config2?ssid=…&pw=… ───
void handleCfg2() {
  String ss = web.arg("ssid"), pw = web.arg("pw");
  if (!ss.length()) {
    cors();
    web.send(400, "text/plain", "ssid?");
    return;
  }
  wr32(SSID_ADDR, ss);
  wr32(PW_ADDR, pw);
  saveFlag(LAN);
  cors();
  web.send(200, "text/plain", "flag2");
}

// ─── HTTP: /config3?ssid=…&pw=…&mac=… ───
void handleCfg3() {
  String ss = web.arg("ssid"), pw = web.arg("pw"), mo = web.arg("mac");
  if (!ss.length()) {
    cors();
    web.send(400, "text/plain", "ssid?");
    return;
  }
  wr32(SSID_ADDR, ss);
  wr32(PW_ADDR, pw);
  wr32(MACO_ADDR, mo);
  saveFlag(MQTT);
  cors();
  web.send(200, "text/plain", "flag3");
}

// Start the 60-second Soft-AP portal
void startPortal() {
  String ap = "ESP-SETUP-" + String(ESP.getChipId(), HEX);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap.c_str(), "12345678");
  Serial.printf("[AP ] %s  IP=%s\n", ap.c_str(),
                WiFi.softAPIP().toString().c_str());

  web.on("/relay", handleRelay);
  web.on("/config1", handleCfg1);
  web.on("/config2", handleCfg2);
  web.on("/config3", handleCfg3);
  web.onNotFound([]() {
    cors();
    web.send(404, "text/plain", "");
  });
  web.begin();
}

// After 60 s, start connecting as STA (non-blocking)
void finishPortal() {
  String ss = rd32(SSID_ADDR), pw = rd32(PW_ADDR);
  if (modeFlag == HS || modeFlag == NONE) {
    Serial.println("[MODE] Hot-spot");
    return;
  }
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ss.c_str(), pw.c_str());
  staConnecting = true;
  staStartMs = millis();
  Serial.printf("[STA] Connecting to %s …\n", ss.c_str());
}

// Called once when STA actually connects
void onStaUp() {
  staConnecting = false;
  Serial.printf("[STA] IP=%s\n", WiFi.localIP().toString().c_str());
  web.begin();  // keep the HTTP `/relay` on LAN

  if (modeFlag == MQTT) {
    macUsed = rd32(MACO_ADDR);
    if (!macUsed.length()) {
      macUsed = WiFi.macAddress();
      macUsed.replace(":", "");
      macUsed.toLowerCase();
    }
    String t1 = "esp8266/" + macUsed + "/relay1";
    String t2 = "esp8266/" + macUsed + "/relay2";

    mqtt.setServer("broker.hivemq.com", 1883);
    mqtt.setCallback([&](char* t, byte* p, unsigned l) {
      String top(t);
      if (top == t1) pulseRelay(1);
      if (top == t2) pulseRelay(2);
    });
    String cid = "ESP-" + macUsed;
    if (mqtt.connect(cid.c_str())) {
      mqtt.subscribe(t1.c_str());
      mqtt.subscribe(t2.c_str());
      Serial.printf("[MQTT] Subscribed %s & %s\n", t1.c_str(), t2.c_str());
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, LOW);
  digitalWrite(RELAY2_PIN, LOW);

  EEPROM.begin(EE_SIZE);
  modeFlag = EEPROM.read(FLG_ADDR);

  startPortal();
  portalTimer.once(60, finishPortal);
}

void loop() {
  web.handleClient();

  if (staConnecting) {
    if (WiFi.status() == WL_CONNECTED) {
      onStaUp();
    } else if (millis() - staStartMs > 15000) {  // 15 s timeout
      Serial.println("[ERR] Wi-Fi timeout → Hot-spot");
      saveFlag(HS);
      ESP.restart();
    }
  }

  if (modeFlag == MQTT && !staConnecting) {
    mqtt.loop();
  }
}
