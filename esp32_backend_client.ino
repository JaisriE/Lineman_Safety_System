// ADD to the existing harness sketch (nothing existing is removed).
// 1) At the top, with the other includes:
#include <WiFi.h>
#include <HTTPClient.h>

const char* WIFI_SSID = "YOUR_WIFI";
const char* WIFI_PASS = "YOUR_PASSWORD";
// IP of the computer running app.py (same Wi-Fi network)
const char* SERVER_URL = "http://192.168.1.100:5000/api/ingest";
const char* API_KEY    = "change-me";      // must match app.py
unsigned long lastPost = 0;

// 2) New function:
void postToBackend() {
  if (WiFi.status() != WL_CONNECTED) { WiFi.reconnect(); return; }
  String j = "{";
  j += "\"hr\":" + String(heartRate, 0);
  j += ",\"temp\":" + (isnan(bodyTemp) ? String("null") : String(bodyTemp, 1));
  j += ",\"fsr\":" + String(fsrValue);
  j += ",\"accel\":" + String(accelG, 2);
  j += ",\"fall\":" + String(fallDetected ? "true" : "false");
  j += ",\"pressure\":" + String(pressureAbnormal ? "true" : "false");
  j += ",\"alarm\":" + String(alarmActive ? "true" : "false");
  j += ",\"sms\":" + String(smsSentForEvent ? "true" : "false");
  j += ",\"gpsFix\":" + String(gpsFix ? "true" : "false");
  if (gpsFix) j += ",\"lat\":" + String(gps.location.lat(), 6) + ",\"lng\":" + String(gps.location.lng(), 6);
  j += "}";
  HTTPClient http;
  http.setTimeout(1500);
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-Key", API_KEY);
  http.POST(j);
  http.end();
}

// 3) In setup(), after the sensors are initialised:
//    WiFi.begin(WIFI_SSID, WIFI_PASS);
//    (do not block waiting; postToBackend() reconnects if needed)

// 4) In loop(), just before updateLCD():
//    if (millis() - lastPost >= 2000) { lastPost = millis(); postToBackend(); }
