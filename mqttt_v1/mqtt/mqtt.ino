/*****************************************************************
 *  ESP8266  ·  dual-relay pulse controller  ·  3 modes
 *  -------------------------------------------------------------
 *  flag 1 : permanent Hot-spot  (HTTP /relay?ch=1|2)
 *  flag 2 : Wi-Fi STA + same HTTP page
 *  flag 3 : Wi-Fi STA + MQTT on broker.hivemq.com
 *           topics  esp8266/<mac>/relay1  and  relay2
 *           (mac = hw MAC without colons, lowercase,
 *           or user-supplied override saved in config3)
 *
 *  A Soft-AP “ESP-SETUP-<chipID>” (pw 12345678) appears
 *  for 60 s on **every** reboot so the user can re-configure.
 *
 *  Relays on GPIO-12 (D6) & GPIO-14 (D5) – 100 ms HIGH pulse.
 *****************************************************************/
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Ticker.h>
#include <PubSubClient.h>

/* ---------------- hardware ---------------- */
#define R1_PIN 12          // D6
#define R2_PIN 14          // D5
#define PULSE_MS 100

/* ---------------- EEPROM layout ---------------- */
#define EE_SIZE     224
#define FLAG        0      // 1 HS | 2 LAN | 3 MQTT
#define SSID        1      // 32
#define PASS       33      // 32
#define TOPIC_BASE 65      // 32 – normally "esp8266"
#define MAC_OVR    97      // 32 – empty = use hw mac
/* we leave the rest unused */

/* ---------------- modes ---------------- */
enum { FL_NONE=0, FL_HS=1, FL_LAN=2, FL_MQTT=3 };

/* ---------------- globals ---------------- */
ESP8266WebServer web(80);
WiFiClient       net;
PubSubClient     mqtt(net);
Ticker           toPortal;

uint8_t modeFlag;
String  topicBase;
String  macUsed;

/* ========== tiny helpers ========== */
void wr32(int a,const String&s){for(uint8_t i=0;i<32;i++)EEPROM.write(a+i,i<s.length()?s[i]:0);}
String rd32(int a){char b[33]={0};for(int i=0;i<32;i++)b[i]=EEPROM.read(a+i);return String(b);}
void saveFlag(uint8_t f){EEPROM.write(FLAG,f);EEPROM.commit();modeFlag=f;}
void cors(){web.sendHeader("Access-Control-Allow-Origin","*");}

void pulse(uint8_t ch){
  uint8_t pin=(ch==1)?R1_PIN:R2_PIN;
  digitalWrite(pin,HIGH); delay(PULSE_MS); digitalWrite(pin,LOW);
}

/* ---------- /relay?ch=1|2 ---------- */
void apiRelay(){
  int ch=web.arg("ch").toInt();
  if(ch!=1&&ch!=2){cors();web.send(400,"text/plain","bad arg");return;}
  pulse(ch); cors(); web.send(200,"text/plain","PULSE");
}

/* ---------- config endpoints ---------- */
void cfg1(){saveFlag(FL_HS );cors();web.send(200,"text/plain","flag1");}
void cfg2(){
  String ss=web.arg("ssid"),pw=web.arg("pw");
  if(!ss.length()){cors();web.send(400,"text/plain","ssid?");return;}
  wr32(SSID,ss); wr32(PASS,pw); saveFlag(FL_LAN);
  cors();web.send(200,"text/plain","flag2");
}
void cfg3(){
  String ss = web.arg("ssid"), pw = web.arg("pw");
  String mac = web.arg("mac"), tp = "esp8266";   // fixed topic base
  if(!ss.length()){cors();web.send(400,"text/plain","ssid?");return;}
  wr32(SSID,ss); wr32(PASS,pw); wr32(TOPIC_BASE,tp); wr32(MAC_OVR,mac);
  saveFlag(FL_MQTT); cors(); web.send(200,"text/plain","flag3");
}

/* ---------- 60-s Soft-AP ---------- */
void startPortal(){
  String ap="ESP-SETUP-"+String(ESP.getChipId(),HEX);
  WiFi.mode(WIFI_AP); WiFi.softAP(ap.c_str(),"12345678");
  Serial.printf("[AP ] %s  IP=%s\n",ap.c_str(),WiFi.softAPIP().toString().c_str());

  web.on("/config1",cfg1);
  web.on("/config2",cfg2);
  web.on("/config3",cfg3);
  web.on("/relay",  apiRelay);
  web.onNotFound([](){cors();web.send(404,"text/plain","");});
  web.begin();
}

/* ---------- after 60 s choose mode ---------- */
void finishPortal(){
  String ss=rd32(SSID), pw=rd32(PASS);

  if(modeFlag==FL_HS||modeFlag==FL_NONE){Serial.println("[MODE] HS");return;}

  WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA);
  WiFi.begin(ss.c_str(),pw.c_str());
  if(WiFi.waitForConnectResult(15000)!=WL_CONNECTED){
      Serial.println("[ERR] Wi-Fi fail → HS next boot"); saveFlag(FL_HS); ESP.restart();}
  web.begin();
  Serial.printf("[LAN] IP %s\n",WiFi.localIP().toString().c_str());

  if(modeFlag==FL_MQTT){
      topicBase = rd32(TOPIC_BASE); if(!topicBase.length()) topicBase="esp8266";
      macUsed   = rd32(MAC_OVR);
      if(!macUsed.length()){
          macUsed = WiFi.macAddress(); macUsed.replace(":",""); macUsed.toLowerCase();
      }
      String t1=topicBase+"/"+macUsed+"/relay1";
      String t2=topicBase+"/"+macUsed+"/relay2";

      mqtt.setServer("broker.hivemq.com",1883);
      mqtt.setCallback([&](char* t, byte* p, unsigned len){
          String topic(t);
          if(topic==t1) pulse(1);
          if(topic==t2) pulse(2);
      });
      mqtt.connect(("esp-"+macUsed).c_str());
      mqtt.subscribe(t1.c_str()); mqtt.subscribe(t2.c_str());

      Serial.printf("[MQTT] broker.hivemq.com  topics %s  &  %s\n",t1.c_str(),t2.c_str());
  }
}

/* ---------- setup ---------- */
void setup(){
  Serial.begin(115200);
  pinMode(R1_PIN,OUTPUT); pinMode(R2_PIN,OUTPUT);
  digitalWrite(R1_PIN,LOW); digitalWrite(R2_PIN,LOW);

  EEPROM.begin(EE_SIZE);
  modeFlag=EEPROM.read(FLAG);

  startPortal(); toPortal.once(60,finishPortal);
}

/* ---------- loop ---------- */
void loop(){
  web.handleClient(); 
  if(modeFlag==FL_MQTT) mqtt.loop();
}
