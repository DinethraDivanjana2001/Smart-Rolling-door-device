#include <WiFi.h>
#include <WebServer.h>

// AP Credentials
const char *ssid = "ESP32_Pulse_AP";
const char *password = "12345678";

// Create Web Server on port 80
WebServer server(80);

// Pulse Pin (Change if needed)
#define PULSE_PIN 3  // GPIO 3 (Can be changed to safer pin like GPIO 4)

// Setup
void setup() {
  Serial.begin(115200);
  pinMode(PULSE_PIN, OUTPUT);
  digitalWrite(PULSE_PIN, LOW);  // Initial state LOW

  // Start Access Point
  WiFi.softAP(ssid, password);

  Serial.println("Access Point Started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Define Web Routes
  server.on("/", handleRoot);
  server.on("/pulse", handlePulse);

  server.begin();
}

// Main Loop
void loop() {
  server.handleClient();
}

// Serve HTML Page
void handleRoot() {
  String html = "<html>\
  <head><title>ESP32 Pulse Control</title></head>\
  <body>\
  <h2>ESP32 Pulse Generator</h2>\
  <p><a href='/pulse'><button style='font-size:20px;'>Generate 100ms Pulse</button></a></p>\
  </body>\
  </html>";
  server.send(200, "text/html", html);
}

// Handle Pulse Trigger
void handlePulse() {
  digitalWrite(PULSE_PIN, HIGH);
  delay(100);   // 100 ms pulse
  digitalWrite(PULSE_PIN, LOW);
  
  server.sendHeader("Location", "/", true);  // Redirect back to main page
  server.send(302, "text/plain", "");        // Redirect response
}
