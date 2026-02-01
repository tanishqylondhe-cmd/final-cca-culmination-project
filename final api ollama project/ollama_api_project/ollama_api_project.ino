#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

/* ========= CHANGE THESE ========= */
const char* WIFI_SSID = "Airtel_yoge_7996";
const char* WIFI_PASSWORD = "8754472667";
const char* PC_IP = "192.168.1.6";   // PC running Ollama
/* ================================ */

WebServer server(80);

/* ---------- CORS Preflight ---------- */
void handleOptions() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(204);
}

/* ---------- /ask Endpoint ---------- */
void handleAsk() {
  server.sendHeader("Access-Control-Allow-Origin", "*");

  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"reply\":\"No request body\"}");
    return;
  }

  // Parse browser request
  StaticJsonDocument<512> inputDoc;
  DeserializationError err = deserializeJson(inputDoc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"reply\":\"Bad JSON\"}");
    return;
  }

  const char* question = inputDoc["question"];
  if (!question) {
    server.send(400, "application/json", "{\"reply\":\"Missing question\"}");
    return;
  }

  Serial.println("Question received:");
  Serial.println(question);

  // Build Ollama request
  StaticJsonDocument<1024> ollamaReq;
  ollamaReq["model"] = "llama3.2:3b";
  ollamaReq["prompt"] = question;
  ollamaReq["stream"] = false;
  ollamaReq["options"]["num_predict"] = 100;   // LIMIT RESPONSE SIZE

  String reqBody;
  serializeJson(ollamaReq, reqBody);

  String ollamaURL = "http://" + String(PC_IP) + ":11434/api/generate";

  HTTPClient http;
  http.setTimeout(30000);   // 🔥 30 seconds timeout (CRITICAL)
  http.begin(ollamaURL);
  http.addHeader("Content-Type", "application/json");

  Serial.println("Sending to Ollama...");
  int httpCode = http.POST(reqBody);

  Serial.print("HTTP Code: ");
  Serial.println(httpCode);

  if (httpCode <= 0) {
    http.end();
    server.send(500, "application/json", "{\"reply\":\"PC unreachable\"}");
    return;
  }

  String payload = http.getString();
  http.end();

  // Parse Ollama response
  StaticJsonDocument<2048> ollamaResp;
  err = deserializeJson(ollamaResp, payload);
  if (err) {
    server.send(500, "application/json", "{\"reply\":\"Ollama JSON error\"}");
    return;
  }

  const char* reply = ollamaResp["response"];
  if (!reply) reply = "No response";

  StaticJsonDocument<1024> out;
  out["reply"] = reply;

  String response;
  serializeJson(out, response);

  server.send(200, "application/json", response);
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nESP32 READY");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  server.on("/ask", HTTP_OPTIONS, handleOptions);
  server.on("/ask", HTTP_POST, handleAsk);
  server.begin();
}

void loop() {
  server.handleClient();
}
