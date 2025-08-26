#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

const char* ssid = "WiFIH_host";
const char* password = "freewifi";

const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char* mqtt_topic = "fish-tank/sensors";
const char* clientID = "mqttx_d5ddf932";

#define SENSOR_PIN   17
#define TRIG_PIN     14
#define ECHO_PIN     12
#define PH_PIN       34
#define TANK_HEIGHT_CM 40.0

OneWire oneWire(SENSOR_PIN);
DallasTemperature DS18B20(&oneWire);

WiFiClient espClient;
PubSubClient client(espClient);

float tempC;
long duration;
float distanceCM;
float waterLevelPercent;
int phRaw;
float phVoltage, phValue;
float humidity = 50.0;

void setup_wifi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP Address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect(clientID)) {
      Serial.println("connected.");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  DS18B20.begin();
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(PH_PIN, INPUT);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  DS18B20.requestTemperatures();
  tempC = DS18B20.getTempCByIndex(0);

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  duration = pulseIn(ECHO_PIN, HIGH);
  distanceCM = duration * 0.0343 / 2;
  if (distanceCM > TANK_HEIGHT_CM) distanceCM = TANK_HEIGHT_CM;
  waterLevelPercent = ((TANK_HEIGHT_CM - distanceCM) / TANK_HEIGHT_CM) * 100.0;

  phRaw = analogRead(PH_PIN);
  phVoltage = phRaw * (3.3 / 4095.0);
  phValue = 3.3 * phVoltage;

  humidity += random(-5, 6) * 0.1;
  humidity = constrain(humidity, 40, 80);

  String payload = "{";
  payload += "\"water_level\": " + String(waterLevelPercent, 1) + ",";
  payload += "\"temperature\": " + String(tempC, 2) + ",";
  payload += "\"pH\": " + String(phValue, 2) + ",";
  payload += "\"humidity\": " + String(humidity, 1);
  payload += "}";

  Serial.println(payload);
  client.publish(mqtt_topic, (char*) payload.c_str());

  delay(1000);
}