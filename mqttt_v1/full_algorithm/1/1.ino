#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

/*— Wi-Fi credentials —*/
const char* ssid     = "MSI_GF65";
const char* password = "Dinethrad";

/*— HiveMQ Cloud cluster —*/
const char* broker   = "0ef19ee9ef2a43f6b79d88f3fff74b2d.s1.eu.hivemq.cloud";
const uint16_t port  = 8883;               // MQTT over TLS port

/*— HiveMQ Cloud tokens —*/
const char* mqttUser = "test1";
const char* mqttPass = "Test@123";

#define RELAY1_PIN 12  // you can pick any valid GPIO
#define RELAY2_PIN 14

WiFiClientSecure  secureClient;
PubSubClient      mqtt(secureClient);

String mac;
const char* ctrlTopic = "esp/control";

// Pulse a pin for 100 ms
void pulse(uint8_t pin) {
  digitalWrite(pin, HIGH);
  delay(100);
  digitalWrite(pin, LOW);
}

// Handle incoming messages: "<targetMac>:<relayNum>"
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String p;
  for (unsigned int i = 0; i < len; i++) p += (char)payload[i];
  int sep = p.indexOf(':');
  if (sep < 0) return;
  String targetMac = p.substring(0, sep);
  String relayNum  = p.substring(sep + 1);

  // only act on messages addressed to me
  if (targetMac != mac) return;

  Serial.printf("→ I (%s) got command: relay %s\n",
                mac.c_str(), relayNum.c_str());
  if      (relayNum == "1") pulse(RELAY1_PIN);
  else if (relayNum == "2") pulse(RELAY2_PIN);
}

void connectWiFi() {
  Serial.print("Wi-Fi → ");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.println(" connected");
}

void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("MQTT → ");
    String clientId = "esp32-" + mac;   // unique per board
    secureClient.setInsecure();         // skip cert check
    mqtt.setServer(broker, port);

    if (mqtt.connect(clientId.c_str(), mqttUser, mqttPass)) {
      Serial.println(" connected");
      mqtt.subscribe(ctrlTopic);
      Serial.printf("Subscribed to %s\n", ctrlTopic);
    } else {
      Serial.printf("failed (%d), retry in 5 s\n", mqtt.state());
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, LOW);
  digitalWrite(RELAY2_PIN, LOW);

  connectWiFi();

  // grab my MAC, strip colons, lowercase
  mac = WiFi.macAddress();
  mac.replace(":", "");
  mac.toLowerCase();
  Serial.printf("My MAC (topic) = %s\n", mac.c_str());

  mqtt.setCallback(mqttCallback);
  connectMQTT();
}

void loop() {
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();
  delay(10);  // <- this helps feed the watchdog
}
