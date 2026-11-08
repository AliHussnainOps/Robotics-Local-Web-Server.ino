/*
  Robotics Machine Design - Assignment 1
  ESP32-S3 Local Web Server (Robot Status Control Panel)

  Student : Ali Hussnain
  Roll No : SU92-BSROM-S24-022
  Submit To: Sir Asif

  Routes:
    /                      -> dashboard
    /robot/status?state=1  -> OPERATIONAL (Green)
    /robot/status?state=0  -> SAFETY HALT (Red)
    /robot/status          -> current status
  Extra credit: http://robotics-server.local (mDNS)

  LED output:
    - External LEDs (Red GPIO 4, Green GPIO 5) with 220 ohm resistors
    - ESP32-S3 on-board RGB LED (no wiring needed, no extra library)
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// ---------------- WiFi credentials ----------------
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";   // <-- apna sahi password


// ---------------- LED pins ----------------
const int RED_LED_PIN   = 4;
const int GREEN_LED_PIN = 5;

// On-board RGB LED (DevKitC-1 clones: GPIO 48 or 38). RGB_BUILTIN is used if defined.
#ifndef RGB_BUILTIN
#define RGB_BUILTIN 48
#endif
const int ONBOARD_BRIGHTNESS = 40;   // 0-255

// ---------------- State ----------------
WebServer server(80);
bool serverStarted = false;
bool robotOperational = false;
unsigned long lastWifiCheck = 0;
unsigned long wifiStartTime = 0;

// ---------------- Dashboard ----------------
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Robot Control Room</title>
<style>
  * { box-sizing: border-box; }
  body { margin:0; min-height:100vh; display:flex; align-items:center; justify-content:center;
    font-family:"Segoe UI",Arial,sans-serif;
    background:linear-gradient(135deg,#0f2027,#203a43,#2c5364); color:#fff; }
  .card { width:92%; max-width:420px; padding:26px; background:rgba(255,255,255,0.08);
    border:1px solid rgba(255,255,255,0.18); border-radius:18px; text-align:center;
    box-shadow:0 10px 30px rgba(0,0,0,0.4); }
  h1 { margin:0 0 4px; font-size:24px; }
  .sub { font-size:13px; opacity:.75; margin-bottom:16px; }
  .info { background:rgba(0,0,0,0.25); border-radius:10px; padding:10px; font-size:13px;
    line-height:1.6; margin-bottom:18px; }
  .label { font-size:12px; letter-spacing:2px; opacity:.7; }
  .status { margin:8px 0 18px; padding:14px; border-radius:12px; font-size:22px;
    font-weight:700; transition:all .3s; }
  .halt { background:#b71c1c; box-shadow:0 0 18px #ff1744; }
  .ok   { background:#1b8f3a; box-shadow:0 0 18px #00e676; }
  button { width:100%; padding:15px; margin:8px 0; border:none; border-radius:10px;
    font-size:17px; font-weight:700; color:#fff; cursor:pointer; }
  button:active { transform:scale(.97); }
  .start { background:#2e7d32; }
  .stop  { background:#c62828; }
  .msg { font-size:13px; margin-top:10px; opacity:.85; min-height:18px; }
  .foot { font-size:11px; opacity:.55; margin-top:14px; }
</style>
</head>
<body>
<div class="card">
  <h1>Robot Control Room</h1>
  <div class="sub">Robotics Machine Design - Assignment 1</div>
  <div class="info">
    Student: Ali Hussnain<br>
    Roll No: SU92-BSROM-S24-022<br>
    Course: Robotics Machine Design
  </div>
  <div class="label">SYSTEM STATUS</div>
  <div id="status" class="status halt">SAFETY HALT</div>
  <button class="start" onclick="controlRobot(1)">START ROBOT</button>
  <button class="stop"  onclick="controlRobot(0)">SAFETY HALT</button>
  <div id="msg" class="msg"></div>
  <div class="foot">ESP32-S3 Local Web Server | Port 80</div>
</div>
<script>
  function render(text) {
    var el = document.getElementById("status");
    if (text.indexOf("OPERATIONAL") >= 0) { el.className="status ok"; el.innerText="OPERATIONAL - GREEN LED"; }
    else { el.className="status halt"; el.innerText="SAFETY HALT - RED LED"; }
  }
  function controlRobot(state) {
    fetch("/robot/status?state=" + state)
      .then(function(r){ return r.text(); })
      .then(function(t){ render(t); document.getElementById("msg").innerText="Command Successful"; })
      .catch(function(){ document.getElementById("msg").innerText="Connection Error"; });
  }
  fetch("/robot/status").then(function(r){ return r.text(); }).then(render);
</script>
</body>
</html>
)rawliteral";

// ---------------- LED helper ----------------
void setRobotState(bool operational) {
  robotOperational = operational;

  // external LEDs
  digitalWrite(GREEN_LED_PIN, operational ? HIGH : LOW);
  digitalWrite(RED_LED_PIN,   operational ? LOW  : HIGH);

  // on-board RGB LED: green = operational, red = halt
  if (operational) neopixelWrite(RGB_BUILTIN, 0, ONBOARD_BRIGHTNESS, 0);
  else             neopixelWrite(RGB_BUILTIN, ONBOARD_BRIGHTNESS, 0, 0);

  Serial.println(operational ? "ROBOT OPERATIONAL - GREEN LED" : "SAFETY HALT - RED LED");
}

// ---------------- Routes ----------------
void handleRoot() {
  server.send_P(200, "text/html", webpage);
}

void handleRobotStatus() {
  if (server.hasArg("state")) {
    String state = server.arg("state");
    if (state == "1")      setRobotState(true);
    else if (state == "0") setRobotState(false);
    else { server.send(400, "text/plain", "Invalid state"); return; }
  }
  server.send(200, "text/plain", robotOperational ? "OPERATIONAL" : "SAFETY HALT");
}

void handleNotFound() {
  server.send(404, "text/plain", "Page Not Found");
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);
  delay(1000);                       // sirf boot par, Serial Monitor ke liye
  Serial.println();
  Serial.println("Booting...");

  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  setRobotState(false);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  wifiStartTime = millis();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/robot/status", HTTP_GET, handleRobotStatus);
  server.onNotFound(handleNotFound);
}

// ---------------- Loop (non-blocking) ----------------
void loop() {
  if (!serverStarted) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println();
      Serial.println("WiFi Connected!");
      Serial.print("Local IP Address: ");
      Serial.println(WiFi.localIP());

      if (MDNS.begin("robotics-server")) {
        MDNS.addService("http", "tcp", 80);
        Serial.println("mDNS started: http://robotics-server.local");
      }

      server.begin();
      Serial.println("HTTP Server Started on Port 80");
      serverStarted = true;
    } else if (millis() - lastWifiCheck > 1000) {
      lastWifiCheck = millis();
      Serial.print(".");
      if (millis() - wifiStartTime > 20000) {   // 20 sec baad hint
        Serial.println();
        Serial.println("WiFi connect nahi hua: SSID/password check karein, WiFi 2.4GHz hona chahiye.");
        wifiStartTime = millis();
        WiFi.disconnect();
        WiFi.begin(ssid, password);
      }
    }
    return;
  }

  server.handleClient();

  if (millis() - lastWifiCheck > 5000) {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi lost - safety halt, reconnecting");
      setRobotState(false);
      WiFi.reconnect();
    }
  }
}
