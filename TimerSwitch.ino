#include "RTClib.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Timer.h>

RTC_DS3231 rtc;
int relayPin = 12;

struct Duration {
  String from;
  String to;
};
static const struct Duration EmptyDuration;

/* Put your SSID & Password */
const char * ssid = "Timer Switch WiFi";
const char * password = "trialnetwork";
const int MAX_SIZE = 10;
Duration configs[MAX_SIZE];

ESP8266WebServer server(80);
Timer timer;

void setup () {

#ifndef ESP8266
  while (!Serial); // for Leonardo/Micro/Zero
#endif

  Serial.begin(9600);
  pinMode(relayPin, OUTPUT);
  delay(3000); // wait for console opening

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  server.on("/", sendConfigurationPage);
  server.on("/configure-time", onAdjustTimeForRTC);
  server.on("/delete", onConfigDelete);
  server.on("/add-config", HTTP_POST, onAddConfig);
  server.onNotFound(provide404Error);

  server.begin();
  Serial.println("HTTP server started");
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  timer.every(20000, checkAndTriggerRelay);
  digitalWrite(relayPin, HIGH);
}

void loop () {
  server.handleClient();
  timer.update();
}

void checkAndTriggerRelay() {
  bool canTurnOn = false;
  bool canTurnOff = false;
  DateTime now = rtc.now();
  char buf1[] = "hh:mm";
  String currentTime = now.toString(buf1);
  for (int index = 0; index < availableConfigsCount(); index++) {
    canTurnOn = canTurnOn || configs[index].from == currentTime;
    canTurnOff = canTurnOff || configs[index].to == currentTime;
  }
  
  if (canTurnOn) {
    Serial.println("Turning On");
    digitalWrite(relayPin, LOW);
  } else if (canTurnOff) {
    Serial.println("Turning Off");
    digitalWrite(relayPin, HIGH);
  }
}

void onAdjustTimeForRTC() {
  if (!server.hasArg("date") || !server.hasArg("time")) {
    server.send(400, "text/plain", "400: Invalid Request");
    return;
  }
  char* date = new char [server.arg("date").length()+1];
  strcpy (date, server.arg("date").c_str());
  
  char* timeValue = new char [server.arg("time").length()+1];
  strcpy (timeValue, server.arg("time").c_str());
  rtc.adjust(DateTime(date, timeValue));
  server.sendHeader("Location", "/",true); //Redirect to our html web page 
  server.send(302, "text/plane",""); 
}

void onConfigDelete() {
  if (!server.hasArg("index") || server.arg("index").toInt() >= MAX_SIZE) {
    server.send(400, "text/plain", "400: Invalid Request");
    return;
  }

  int index = server.arg("index").toInt();
  while (configs[index].from != NULL) {
    configs[index].from = configs[index + 1].from;
    configs[index].to = configs[index + 1].to;
    index++;
  }
  configs[index - 1] = EmptyDuration;
  sendConfigurationPage();
}

void onAddConfig() {
  if (!server.hasArg("start") || !server.hasArg("end") ||
    server.arg("start") == NULL || server.arg("end") == NULL) {
    server.send(400, "text/plain", "400: Invalid Request");
    return;
  }
  int indexToBeAdded = availableConfigsCount();
  configs[indexToBeAdded].from = server.arg("start");
  configs[indexToBeAdded].to = server.arg("end");
  sendConfigurationPage();
}

void provide404Error() {
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1, user-scalable=no'><title>Timer Switch Configuration</title>";
  html += "<style>body{margin:0px;height:100vh;width:100vw;display:flex;align-items:center;justify-content:center}.notfound{display:flex;flex-direction:column;max-width:70%;font-family:sans-serif;font-size:18px;font-weight:700}";
  html += "a{text-decoration:none;text-align:center;color:#fff;padding:13px 23px;background:#ff6300}a:hover{color:#ff6300;background:#211b19}</style></head>";
  html += "<body><div class='notfound'><h2>404 - Page not found</h2> <a href='/#'>Home</a></div></body></html>";
  server.send(404, "text/html", html);
}

void sendConfigurationPage() {
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1.0, user-scalable=no'><title>Timer Switch Configuration</title>";
  html += "<style>html{font-family:Helvetica;display:inline-block;margin:0px auto;text-align:center}body{margin-top:50px}h1{color:#444;margin:50px auto 30px}h3{color:#444}";
  html += ".button{display:block;width:150px;border:none;color:white;padding:13px 30px;text-decoration:none;font-size:18px;margin:0px auto 35px;cursor:pointer;border-radius:4px;background-color:#1abc9c}";
  html += ".button:active{background-color:#16a085}p{font-size:14px;color:#888;margin-bottom:10px}";
  html += ".delete-icon{margin-left:20px;display:inline-block;width:24px;height:24px;";
  html += "background:url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAABmJLR0QA/wD/AP+gvaeTAAABKUlEQVRYhe2Uuw4BQRSGPwqJW6KgU4jeA0jEC2h4Ku8hXkFJiCdYURJR0Ot0FM5kx2TZHTu7QvZPTjYzszP/";
  html += "d85cIFMme42AM3Az4gwM0wAIMldxSgNAmUXtf6tciJFLBXrlHZs410dltZn/9Qr8HIBZ0rC2cwDnygD+EmDvcjHbU78Dmlr7Qswn3RZAmavKdgUiNQCAPrAFWhqEqQYwBzpJAMyk76BBmOae/DNPAqAELKT/CLQN842MeUDdFiCqSjyy0yH0zCOZxwEAKANL/O2wylzpJJN6MSBW+IlsTPOwh2gi37W2SFhcgYHMKwI1bb0qULHJoACM8SthY17nec9VJV7dDqcKOnDvbodz81cHLhUIdf08gTFVwd+OZRIAHYEIMtchpkDrDvRKkk0dBvEBAAAAAElFTkSuQmCC') 50% 50% no-repeat;background-size:100%;border:none;}";
  html += "div#currentConfig{padding:0;margin:0;}div#currentConfig p{border:1px solid #ddd;margin-top:-1px;background-color:#f6f6f6;padding:12px;text-decoration:none;font-size:18px;color:black;display:block;position:relative;}";
  html += "div#currentConfig p:hover{background-color:#eee;}</style></head>";
  html += "<body><h1>Timer Switch Configurator</h1><h3>Current Available Configuration</h3><div id='currentConfig'>";
  int i;
  for (i = 0; i < availableConfigsCount(); i++) {
    html += "<p>Starts By " + configs[i].from + " & Ends By " + configs[i].to + " <a class='delete-icon' href='/delete?index=" + i + "'></a></p>";
  }
  if (i == 0) {
    html += "<p>No Configuration available</p>\n";
  }
  html += "</div>";
  if (i < MAX_SIZE) {
    html += "<h3 style='margin-top:25px;'>Add New Configuration</h3>";
    html += "<form action='/add-config' method='POST'><div style='display:flex;justify-content:space-evenly;margin-bottom:10px;'>";
    html += "<p>Start By</p><input type='time' name='start' required></input><p>End By</p><input type='time' name='end' required></input></div><input type='submit' class='button' value='Add Config'/>";
    html += "</form>";
  }

  html += "<a class='button' onclick='window.location.href = \"configure-time?date=\" + new Date().toDateString().substring(new Date().toDateString().indexOf(\" \") + 1) + \"&time=\" + new Date().toTimeString().split(\" \", 1)[0];'>Configure Time</a></body></html>\n";
  
  server.send(200, "text/html", html);
}

int availableConfigsCount() {
  int count = 0;
  while (configs[count].from != NULL) {
    count++;
  }
  return count;
}
