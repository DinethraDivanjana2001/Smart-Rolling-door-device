#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid     = "MSI_GF65";
const char* password = "Dinethrad";
const char* broker   = "broker.hivemq.com";
const int   port     = 1883;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

#define RELAY1_PIN 18
String mac;

void pulse(uint8_t pin) {
  digitalWrite(pin, HIGH);
  delay(100);
  digitalWrite(pin, LOW);
}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String p;
  for (unsigned int i = 0; i < len; i++) p += (char)payload[i];
  if (p == mac + ":1") pulse(RELAY1_PIN);
}

void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("MQTT connecting...");
    String clientId = "esp32c3-" + mac;
    if (mqtt.connect(clientId.c_str())) {
      Serial.println(" connected");
      mqtt.subscribe("esp/control");
    } else {
      Serial.println(" failed, retrying in 5s");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY1_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, LOW);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("WiFi connected");

  mac = WiFi.macAddress();
  mac.replace(":", "");
  mac.toLowerCase();
  Serial.println("MAC: " + mac);

  mqtt.setServer(broker, port);
  mqtt.setCallback(mqttCallback);
  connectMQTT();
}

void loop() {
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();
  delay(10);
}
