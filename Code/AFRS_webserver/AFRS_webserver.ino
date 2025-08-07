#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// Wi-Fi credentials
const char* ssid = "";
const char* password = "";

// Web server on port 80
WebServer server(80);

// TCP client for talking to motor ESP
WiFiClient motorClient;
IPAddress motorIP(192, 168, 1, 18); // Motor ESP IP (must be static or fixed by DHCP)

bool starterSet = false;
bool endSet = false;

// =================== COMMUNICATION ===================

bool ensureConnected() {
  if (!motorClient.connected()) {
    return motorClient.connect(motorIP, 3333);  // Port must match motor ESP
  }
  return true;
}

String sendMotorCommand(const String& cmd) {
  if (!ensureConnected()) return "Motor Not Connected";

  motorClient.println(cmd);
  motorClient.flush();

  String response = "";
  unsigned long start = millis();
  while (millis() - start < 300) {
    while (motorClient.available()) {
      char c = motorClient.read();
      if (c == '\n') return response;
      response += c;
    }
  }
  return response;
}

// =================== SETUP ===================

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi. IP: " + WiFi.localIP().toString());

  if (!LittleFS.begin(true)) {
    Serial.println("Failed to mount LittleFS");
    return;
  }

  // Routes
  server.on("/", handleRoot);
  server.on("/cmd", handleCommand);
  server.on("/move", handleMove);
  server.on("/set", handleSet);
  server.on("/status", handleStatus);

  server.begin();
}

void loop() {
  server.handleClient();
}

// =================== WEB HANDLERS ===================

void handleRoot() {
  File file = LittleFS.open("/index.html", "r");
  if (!file) {
    server.send(500, "text/plain", "Failed to load HTML file");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}

void handleCommand() {
  String command = server.arg("code");

  String reply = sendMotorCommand(command);
  server.send(200, "text/plain", reply.length() > 0 ? reply : "OK");
}

void handleSet() {
  if (!server.hasArg("type") || !server.hasArg("val")) {
    server.send(400, "text/plain", "Missing arguments");
    return;
  }

  String type = server.arg("type");
  float value = server.arg("val").toFloat();

  if (type == "starter-position-input") {
    starterSet = true;
    String cmd = "starter-position-input " + String(value, 2);
    sendMotorCommand(cmd);
  } else if (type == "end-position-input") {
    endSet = true;
    String cmd = "end-position-input " + String(value, 2);
    sendMotorCommand(cmd);
  } else {
    server.send(400, "text/plain", "Invalid type");
    return;
  }

  server.send(200, "text/plain", "OK");
}

void handleStatus() {
  String json = sendMotorCommand("status");
  if (json.startsWith("{")) {
    server.send(200, "application/json", json);
  } else {
    server.send(500, "text/plain", "Invalid status");
  }
}

void handleMove() {
  float position = server.arg("pos").toFloat();
  String cmd = "move " + String(position, 2);
  sendMotorCommand(cmd);

  // MUST send a response to complete the request
  server.send(200, "text/plain", "OK");
}
