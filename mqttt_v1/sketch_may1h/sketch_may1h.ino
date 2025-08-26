/*****************************************************************
 * 3-MODE · 2-RELAY CONTROLLER  (GPIO-12 / GPIO-14, 100 ms pulse)
 *  flag 1 : Hot-spot  |  flag 2 : LAN Web  |  flag 3 : LAN + MQTT
 *  Soft-AP every reboot for 60 s: “ESP-SETUP-<chipID>” (pw 12345678)
 *****************************************************************/
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Ticker.h>
#include <PubSubClient.h>

/* -------- pins -------- */
#define RELAY1_PIN  12      // D6
#define RELAY2_PIN  14      // D5

/* -------- EEPROM layout -------- */
#define EEPROM_SIZE   192
#define FLAG_ADDR       0          // 0 none | 1 HS | 2 LAN | 3 MQTT
#define SSID_ADDR       1          // 32
#define PW_ADDR        33          // 32
#define BROKER_ADDR    65          // 32
#define TOPIC_ADDR     97          // 32

enum { FLAG_NONE=0, FLAG_HS=1, FLAG_LAN=2, FLAG_MQTT=3 };

/* -------- globals -------- */
ESP8266WebServer server(80);
WiFiClient       net;
PubSubClient     mqtt(net);
Ticker           tPortal;

uint8_t  g_flag;
String   g_topicBase;
String   g_mac;

/* ===== EEPROM helpers ===== */
void wr32(int a,const String&s){for(uint8_t i=0;i<32;i++)EEPROM.write(a+i,i<s.length()?s[i]:0);}
String rd32(int a){char b[33]={0};for(uint8_t i=0;i<32;i++)b[i]=EEPROM.read(a+i);return String(b);}
void saveFlag(uint8_t f){EEPROM.write(FLAG_ADDR,f);EEPROM.commit();g_flag=f;}

/* ===== relay pulse ===== */
void pulseRelay(uint8_t ch){
  uint8_t pin=(ch==1)?RELAY1_PIN:RELAY2_PIN;
  digitalWrite(pin,HIGH); delay(100); digitalWrite(pin,LOW);
  if(g_flag==FLAG_MQTT && mqtt.connected()){
      String t=g_topicBase+"/"+g_mac+"/relay"+String(ch);
      mqtt.publish(t.c_str(),"PULSE",false);
  }
}

/* ===== HTTP helpers ===== */
void cors(){server.sendHeader("Access-Control-Allow-Origin","*");}

/* ---------------- /relay?ch=1|2 ---------------- */
void apiRelay(){
  int ch=server.arg("ch").toInt();
  if(ch!=1&&ch!=2){cors();server.send(400,"text/plain","bad arg");return;}
  pulseRelay(ch); cors(); server.send(200,"text/plain","PULSED");
}

/* ------------- config endpoints ------------- */
void cfg1(){saveFlag(FLAG_HS );cors();server.send(200,"text/plain","flag1");}
void cfg2(){
  String ss=server.arg("ssid"),pw=server.arg("pw");
  if(!ss.length()){cors();server.send(400,"text/plain","ssid?");return;}
  wr32(SSID_ADDR,ss); wr32(PW_ADDR,pw); saveFlag(FLAG_LAN);
  cors(); server.send(200,"text/plain","flag2");
}
void cfg3(){
  String ss=server.arg("ssid"),pw=server.arg("pw");
  String br=server.arg("broker"),tp=server.arg("topic");
  if(!ss.length()||!br.length()||!tp.length()){cors();server.send(400,"text/plain","args?");return;}
  wr32(SSID_ADDR,ss); wr32(PW_ADDR,pw); wr32(BROKER_ADDR,br); wr32(TOPIC_ADDR,tp);
  saveFlag(FLAG_MQTT); cors(); server.send(200,"text/plain","flag3");
}

/* ------------- 60-s Soft-AP ------------- */
void startPortal(){
  String ap="ESP-SETUP-"+String(ESP.getChipId(),HEX);
  WiFi.mode(WIFI_AP); WiFi.softAP(ap.c_str(),"12345678");
  Serial.printf("[AP ] %s  IP=%s\n",ap.c_str(),WiFi.softAPIP().toString().c_str());

  server.on("/config1",cfg1);
  server.on("/config2",cfg2);
  server.on("/config3",cfg3);
  server.on("/relay",  apiRelay);
  server.onNotFound([](){cors();server.send(404,"text/plain","");});
  server.begin();
}

/* ------------- after 60 s decide mode ------------- */
void finishPortal(){
  String ss=rd32(SSID_ADDR), pw=rd32(PW_ADDR);
  if(g_flag==FLAG_HS||g_flag==FLAG_NONE){Serial.println("[MODE] Hot-spot");return;}

  WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA); WiFi.begin(ss.c_str(),pw.c_str());
  if(WiFi.waitForConnectResult(15000)!=WL_CONNECTED){
      Serial.println("[ERR] Wi-Fi fail → HS next boot");saveFlag(FLAG_HS);ESP.restart();}
  server.begin();
  Serial.printf("[LAN] IP %s\n",WiFi.localIP().toString().c_str());

  if(g_flag==FLAG_MQTT){
      String br=rd32(BROKER_ADDR); g_topicBase=rd32(TOPIC_ADDR);
      mqtt.setServer(br.c_str(),1883);
      mqtt.setCallback([](char* t, byte* p, unsigned len){
          String topic(t);
          String pay; pay.reserve(len);
          for(unsigned i=0;i<len;i++) pay+=(char)p[i];
          if(topic.endsWith("/relay1")) pulseRelay(1);
          if(topic.endsWith("/relay2")) pulseRelay(2);
      });
      mqtt.connect(("esp-"+g_mac).c_str());
      mqtt.subscribe((g_topicBase+"/"+g_mac+"/relay1").c_str());
      mqtt.subscribe((g_topicBase+"/"+g_mac+"/relay2").c_str());
      Serial.printf("[MQTT] %s  base %s\n",br.c_str(),g_topicBase.c_str());
  }
}

/* ------------- setup ------------- */
void setup(){
  Serial.begin(115200);
  pinMode(RELAY1_PIN,OUTPUT); pinMode(RELAY2_PIN,OUTPUT);
  digitalWrite(RELAY1_PIN,LOW); digitalWrite(RELAY2_PIN,LOW);

  EEPROM.begin(EEPROM_SIZE);
  g_flag=EEPROM.read(FLAG_ADDR);
  g_mac=WiFi.macAddress(); g_mac.toLowerCase(); g_mac.replace(":","");

  startPortal(); tPortal.once(60,finishPortal);
}

/* ------------- loop ------------- */
void loop(){
  server.handleClient();
  if(g_flag==FLAG_MQTT) mqtt.loop();
}
