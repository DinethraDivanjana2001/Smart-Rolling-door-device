
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

const char* ssid = "YasiruDEX";
const char* password = "freewifi";
const char* mqtt_server = "broker.hivemq.com";  // Or your own broker

WiFiClient espClient;
PubSubClient client(espClient);

const int relay1Pin = 12; // D6
const int relay2Pin = 14; // D5

String macStr;

void callback(char* topic, byte* payload, unsigned int length) {
  String command;
  for (unsigned int i = 0; i < length; i++) {
    command += (char)payload[i];
  }

  Serial.println("Command received: " + command);

  if (command == "trigger1") {
    digitalWrite(relay1Pin, HIGH);
    delay(100);
    digitalWrite(relay1Pin, LOW);
  } else if (command == "trigger2") {
    digitalWrite(relay2Pin, HIGH);
    delay(100);
    digitalWrite(relay2Pin, LOW);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(macStr.c_str())) {
      String topic = "/relay/" + macStr;
      client.subscribe(topic.c_str());
      Serial.println("Subscribed to topic: " + topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(relay1Pin, OUTPUT);
  pinMode(relay2Pin, OUTPUT);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  macStr = WiFi.macAddress();
  macStr.replace(":", "");
  Serial.println("MAC Address: " + macStr);

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}

