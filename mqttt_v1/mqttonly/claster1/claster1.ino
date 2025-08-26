#include <ESP8266WiFi.h>
#include <PubSubClient.h>

/*— Wi-Fi —*/
const char* ssid     = "MSI_GF65";
const char* password = "Dinethrad";

/*— HiveMQ Cloud —*/
const char* broker   = "0ef19ee9ef2a43f6b79d88f3fff74b2d.s1.eu.hivemq.cloud";
const uint16_t port  = 8883;
const char* mqttUser = "test1";
const char* mqttPass = "Test@123";

#define RELAY1_PIN 12
#define RELAY2_PIN 14

WiFiClientSecure secureClient;
PubSubClient     mqtt(secureClient);

String mac;
String topicRoot;     // e.g. "a4e57c1187f1"
String topicFilter;   // e.g. "a4e57c1187f1/+"

void pulse(uint8_t pin) {
  digitalWrite(pin, HIGH);
  delay(100);
  digitalWrite(pin, LOW);
}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  // topic = "<mac>/<deviceId>"
  String t = String(topic);
  int slash = t.indexOf('/');
  String deviceId = t.substring(slash+1);

  // payload = "1" or "2"
  String cmd;
  for (unsigned i = 0; i < len; i++) cmd += char(payload[i]);

  Serial.printf("Got from device %s → relay %s\n", deviceId.c_str(), cmd.c_str());

  if (cmd == "1")      pulse(RELAY1_PIN);
  else if (cmd == "2") pulse(RELAY2_PIN);
}

void connectWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
}

void connectMQTT() {
  while (!mqtt.connected()) {
    String clientId = "esp8266-" + mac;  
    secureClient.setInsecure();
    mqtt.setServer(broker, port);
    mqtt.setCallback(mqttCallback);
    if (mqtt.connect(clientId.c_str(), mqttUser, mqttPass)) {
      mqtt.subscribe(topicFilter.c_str());
      Serial.printf("Subscribed to %s\n", topicFilter.c_str());
    } else {
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);

  connectWiFi();

  mac = WiFi.macAddress();
  mac.replace(":", "");
  mac.toLowerCase();
  topicRoot   = mac;
  topicFilter = mac + "/+";

  connectMQTT();
}

void loop() {
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();
}
