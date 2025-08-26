#include <ESP8266WiFi.h>
#include <PubSubClient.h>

/* ---------- user settings ---------- */
const char* ssid     = "MSI_GF65";
const char* password = "Dinethrad";
const char* broker   = "broker.hivemq.com";
const uint16_t port  = 1883;

#define RELAY1_PIN 12          // D6
#define RELAY2_PIN 14          // D5
/* ----------------------------------- */

WiFiClient   wifi;
PubSubClient mqtt(wifi);

String mac;             // lowercase, no colons
String topic1, topic2;  // esp8266/<mac>/relay1 , esp8266/<mac>/relay2

/* helper: 100 ms pulse */
void pulse(uint8_t pin) {
  digitalWrite(pin, HIGH);      // flip to LOW if your board is LOW-active
  delay(100);
  digitalWrite(pin, LOW);
}

void cb(char* topic, byte* payload, unsigned int len) {
  // Create a null-terminated string from payload
  char message[len + 1];
  memcpy(message, payload, len);
  message[len] = '\0';

  Serial.printf("MQTT [%s] %s\n", topic, message);

  if (strcmp(topic, topic1.c_str()) == 0)          pulse(RELAY1_PIN);
  else if (strcmp(topic, topic2.c_str()) == 0)     pulse(RELAY2_PIN);
}


/* connections ------------------------------------------------------------ */
void connectWiFi() {
  Serial.print("Wi-Fi → ");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { Serial.print('.'); delay(500); }
  Serial.println("connected");
}

void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("MQTT → ");
    if (mqtt.connect(("esp8266-" + mac).c_str())) {
      Serial.println("connected");
      mqtt.subscribe(topic1.c_str());
      mqtt.subscribe(topic2.c_str());
      Serial.printf("  ↳ %s\n  ↳ %s\n", topic1.c_str(), topic2.c_str());
    } else {
      Serial.printf("failed (%d) – retry 5 s\n", mqtt.state());
      delay(5000);
    }
  }
}

/* Arduino lifecycle ------------------------------------------------------ */
void setup() {
  Serial.begin(115200);

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, LOW);
  digitalWrite(RELAY2_PIN, LOW);

  connectWiFi();

  mac = WiFi.macAddress(); mac.replace(":", ""); mac.toLowerCase(); // a4e57c1187f1
  topic1 = "esp8266/" + mac + "/relay1";
  topic2 = "esp8266/" + mac + "/relay2";

  mqtt.setServer(broker, port);
  mqtt.setCallback(cb);
  connectMQTT();
}

void loop() {
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();
}