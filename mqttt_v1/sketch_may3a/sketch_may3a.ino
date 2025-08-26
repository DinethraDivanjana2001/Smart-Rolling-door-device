/* ------------ Wi‑Fi ------------ */
const char* ssid     = "MSI_GF65";
const char* password = "Dinethrad";

/* ---------- HiveMQ Cloud -------- */
const char* broker   = "0ef19ee9ef2a43f6b79d88f3fff74b2d.s1.eu.hivemq.cloud";
const uint16_t port  = 8883;                 // TLS port (not Web‑Sockets)
const char*   mqttUser = "test1";
const char*   mqttPass = "Test@123";

/* ---------- GPIO pins ---------- */
#define RELAY1_PIN 12      // D6
#define RELAY2_PIN 14      // D5
const uint16_t PULSE_MS = 100;

/* ---------- libraries ---------- */
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

/* ---------- globals ---------- */
WiFiClientSecure net;
PubSubClient     mqtt(net);
String macTopic;            // topic = MAC w/o colons

/* ---------- helper: pulse pin ---------- */
void pulse(uint8_t pin) {
  digitalWrite(pin, HIGH);
  delay(PULSE_MS);
  digitalWrite(pin, LOW);
}

/* ---------- MQTT callback ---------- */
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String msg; while (len--) msg += (char)*payload++;
  Serial.printf("[ESP] %s : %s\n", topic, msg.c_str());

  if (msg == "relay1") { pulse(RELAY1_PIN); Serial.println("→ relay1 fired"); }
  else if (msg == "relay2") { pulse(RELAY2_PIN); Serial.println("→ relay2 fired"); }
}

/* ---------- ensure MQTT connected ---------- */
void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("MQTT connecting… ");
    if (mqtt.connect("esp_relay", mqttUser, mqttPass)) {
      Serial.println("OK");
      mqtt.subscribe(macTopic.c_str());
      Serial.print("Subscribed to topic: "); Serial.println(macTopic);
    } else {
      Serial.printf("failed (%d). retry 2 s\n", mqtt.state());
      delay(2000);
    }
  }
}

void setup() {
  pinMode(RELAY1_PIN, OUTPUT); digitalWrite(RELAY1_PIN, LOW);
  pinMode(RELAY2_PIN, OUTPUT); digitalWrite(RELAY2_PIN, LOW);
  Serial.begin(115200);

  /* Wi‑Fi */
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(400); Serial.print('.'); }
  Serial.print("\nIP: "); Serial.println(WiFi.localIP());

  /* topic = MAC */
  uint8_t mac[6]; WiFi.macAddress(mac);
  char buf[13]; sprintf(buf, "%02X%02X%02X%02X%02X%02X",
                        mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  macTopic = buf;
  Serial.print("MQTT topic (ESP MAC): "); Serial.println(macTopic);

  /* MQTT (TLS) */
  net.setInsecure();               // skip CA check for demo
  mqtt.setServer(broker, port);
  mqtt.setCallback(mqttCallback);
  connectMQTT();
}

void loop() {
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();
}
