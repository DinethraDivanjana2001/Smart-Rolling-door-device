#include <ESP8266WiFi.h>
#include <PubSubClient.h>

//
// —– Wi-Fi & MQTT broker settings —–
const char* ssid     = "MSI_GF65";
const char* password = "Dinethrad";
const char* broker   = "broker.hivemq.com";
const uint16_t port  = 1883;
// ——————————————————————————————

#define RELAY1_PIN 12  // D6
#define RELAY2_PIN 14  // D5

WiFiClient   wifi;
PubSubClient mqtt(wifi);

String mac;
String cmdTopic;

// pulse a pin for 100 ms
void pulse(uint8_t pin) {
  digitalWrite(pin, HIGH);
  delay(100);
  digitalWrite(pin, LOW);
}

// called on every incoming MQTT message
// payload format: "<deviceID>:<relayNum>"
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String p;
  for (unsigned int i = 0; i < len; i++) {
    p += (char)payload[i];
  }
  int sep = p.indexOf(':');
  String deviceID = p.substring(0, sep);
  String relayNum = p.substring(sep + 1);

  Serial.printf("From device %s → relay %s\n",
                deviceID.c_str(), relayNum.c_str());

  if (relayNum == "1")      pulse(RELAY1_PIN);
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
    // clientId = "esp8266-"+mac to keep it unique per ESP
    String clientId = "esp8266-" + mac;
    if (mqtt.connect(clientId.c_str())) {
      Serial.println(" connected");
      // subscribe to topic = mac
      mqtt.subscribe(cmdTopic.c_str());
      Serial.printf("Subscribed to %s\n", cmdTopic.c_str());
    } else {
      Serial.printf("failed (%d), retrying in 5 s\n", mqtt.state());
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

  // build topic = <mac> (no prefix)
  mac = WiFi.macAddress();
  mac.replace(":", "");
  mac.toLowerCase();
  cmdTopic = mac;

  mqtt.setServer(broker, port);
  mqtt.setCallback(mqttCallback);
  connectMQTT();
}

void loop() {
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();
}
