#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "Yasiru's S25 Ultra";
const char* password = "freewifi";

const int PIN_OPEN = 5;
const int PIN_STOP = 18;
const int PIN_CLOSE = 19;

WebServer server(80);

void activateRelay(int pin) {
  digitalWrite(pin, HIGH);
  delay(100);
  digitalWrite(pin, LOW);
}

void handleOpen() {
  Serial.println("Received: OPEN");
  activateRelay(PIN_OPEN);
  server.send(200, "text/plain", "OPEN command received");
}

void handleStop() {
  Serial.println("Received: STOP");
  activateRelay(PIN_STOP);
  server.send(200, "text/plain", "STOP command received");
}

void handleClose() {
  Serial.println("Received: CLOSE");
  activateRelay(PIN_CLOSE);
  server.send(200, "text/plain", "CLOSE command received");
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_OPEN, OUTPUT);
  pinMode(PIN_STOP, OUTPUT);
  pinMode(PIN_CLOSE, OUTPUT);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi!");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  server.on("/open", handleOpen);
  server.on("/stop", handleStop);
  server.on("/close", handleClose);

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}
