#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <MAX30105.h>
#include "heartRate.h"
#include <TinyGPSPlus.h>
#include <math.h>

// ============================================================
// USER SETTINGS
// ============================================================

const char PHONE_NUMBER[] = "+917010458095";  //7010458095

// Calibrate using actual FSR readings from Serial Monitor.
const int FSR_THRESHOLD = 3000;

// Set true if high FSR reading means abnormal pressure.
// Set false if low FSR reading means abnormal pressure.
const bool FSR_HIGH_IS_ABNORMAL = true;

// Initial prototype fall-detection thresholds.
const float FALL_IMPACT_G = 2.4;
const float FREE_FALL_G = 0.45;

// Cancellation countdown
const unsigned long ALARM_TIME = 5000;

// ============================================================
// PIN CONFIGURATION
// ============================================================

#define FSR_PIN       34
#define DS18B20_PIN    4

#define GPS_RX_PIN    16
#define GPS_TX_PIN    17

#define GSM_RX_PIN    26
#define GSM_TX_PIN    27

#define BUTTON_PIN    25
#define BUZZER_PIN    18
#define LED_PIN       19

// ============================================================
// OBJECTS
// ============================================================

LiquidCrystal_I2C lcd(0x27, 16, 2);

OneWire oneWire(DS18B20_PIN);
DallasTemperature tempSensor(&oneWire);

Adafruit_MPU6050 mpu;
MAX30105 particleSensor;
TinyGPSPlus gps;

HardwareSerial GPSSerial(1);
HardwareSerial GSMSerial(2);

// ============================================================
// VARIABLES
// ============================================================

int fsrValue = 0;

float bodyTemp = NAN;
float heartRate = 0;
float accelG = 1.0;
float previousAccelG = 1.0;

bool mpuOK = false;
bool maxOK = false;
bool gpsFix = false;

bool fallDetected = false;
bool pressureAbnormal = false;

bool alarmActive = false;
bool smsSentForEvent = false;

unsigned long alarmStart = 0;
unsigned long lastFallTime = 0;
unsigned long lastTempRequest = 0;
unsigned long lastLcdUpdate = 0;
unsigned long lastPageChange = 0;
unsigned long lastHeartBeat = 0;

uint16_t heartRateSamples[4] = {0, 0, 0, 0};
uint8_t heartRateIndex = 0;
uint8_t validHeartRateSamples = 0;

int displayPage = 0;
int lastButtonState = HIGH;

bool cancelMessagePending = false;

// ============================================================
// LCD HELPER
// ============================================================

void lcdLine(byte row, String message) {
  if (message.length() > 16) {
    message = message.substring(0, 16);
  }

  while (message.length() < 16) {
    message += ' ';
  }

  lcd.setCursor(0, row);
  lcd.print(message);
}

// ============================================================
// GSM AT COMMAND
// ============================================================

bool sendAT(const char *command,
            const char *expected,
            unsigned long timeout) {

  while (GSMSerial.available()) {
    GSMSerial.read();
  }

  GSMSerial.println(command);

  String response = "";
  unsigned long start = millis();

  while (millis() - start < timeout) {

    while (GSMSerial.available()) {
      char c = GSMSerial.read();
      response += c;

      if (response.indexOf(expected) >= 0) {
        return true;
      }

      if (response.indexOf("ERROR") >= 0) {
        return false;
      }
    }

    delay(2);
  }

  Serial.print("AT timeout: ");
  Serial.println(response);

  return false;
}

// ============================================================
// SEND EMERGENCY SMS
// ============================================================

bool sendEmergencySMS() {

  Serial.println("Preparing emergency SMS");

  // Check GSM response.
  if (!sendAT("AT", "OK", 3000)) {
    Serial.println("GSM not responding");
    return false;
  }

  sendAT("ATE0", "OK", 1000);

  if (!sendAT("AT+CMGF=1", "OK", 3000)) {
    Serial.println("SMS text mode failed");
    return false;
  }

  // Request SMS destination.
  while (GSMSerial.available()) {
    GSMSerial.read();
  }

  GSMSerial.print("AT+CMGS=\"");
  GSMSerial.print(PHONE_NUMBER);
  GSMSerial.println("\"");

  String response = "";
  unsigned long start = millis();

  while (millis() - start < 7000) {

    while (GSMSerial.available()) {
      char c = GSMSerial.read();
      response += c;
    }

    if (response.indexOf('>') >= 0) {
      break;
    }

    if (response.indexOf("ERROR") >= 0) {
      Serial.println("SMS destination error");
      return false;
    }

    delay(2);
  }

  if (response.indexOf('>') < 0) {
    Serial.println("SMS prompt not received");
    return false;
  }

  // Construct message.
  String message = "INDUSTRIAL SAFETY HARNESS ALERT\n";

  if (fallDetected) {
    message += "Event: FALL DETECTED\n";
  }

  if (pressureAbnormal) {
    message += "Event: ABNORMAL PRESSURE\n";
  }

  message += "Heart rate: ";

  if (maxOK && heartRate >= 30 && heartRate <= 220) {
    message += String(heartRate, 0) + " BPM\n";
  } else {
    message += "Unavailable\n";
  }

  message += "Temperature: ";

  if (!isnan(bodyTemp)) {
    message += String(bodyTemp, 1) + " C\n";
  } else {
    message += "Unavailable\n";
  }

  // Use only a recent valid GPS fix.
  if (gps.location.isValid() &&
      gps.location.age() < 10000) {

    double latitude = gps.location.lat();
    double longitude = gps.location.lng();

    message += "Latitude: ";
    message += String(latitude, 6);
    message += "\nLongitude: ";
    message += String(longitude, 6);

    message += "\nMap: https://maps.google.com/?q=";
    message += String(latitude, 6);
    message += ",";
    message += String(longitude, 6);

  } else {
    message += "GPS location unavailable\n";
  }

  GSMSerial.print(message);

  // Ctrl+Z sends the SMS.
  GSMSerial.write(26);

  // Wait for modem submission confirmation.
  start = millis();
  response = "";

  while (millis() - start < 25000) {

    while (GSMSerial.available()) {
      char c = GSMSerial.read();
      response += c;
    }

    if (response.indexOf("+CMGS:") >= 0) {
      Serial.println("SMS submitted successfully");
      return true;
    }

    if (response.indexOf("ERROR") >= 0) {
      Serial.println("SMS submission error");
      return false;
    }

    delay(2);
  }

  Serial.println("SMS confirmation timeout");
  return false;
}

// ============================================================
// READ FSR
// ============================================================

void readFSR() {

  fsrValue = analogRead(FSR_PIN);

  if (FSR_HIGH_IS_ABNORMAL) {
    pressureAbnormal = (fsrValue > FSR_THRESHOLD);
  } else {
    pressureAbnormal = (fsrValue < FSR_THRESHOLD);
  }

  Serial.print("FSR ADC: ");
  Serial.print(fsrValue);
  Serial.print("  Pressure: ");
  Serial.println(pressureAbnormal ? "ABNORMAL" : "NORMAL");
}

// ============================================================
// READ MPU6050 AND DETECT POSSIBLE FALL
// ============================================================

void readMPU6050() {

  if (!mpuOK) {
    return;
  }

  sensors_event_t acceleration;
  sensors_event_t gyro;
  sensors_event_t temperature;

  mpu.getEvent(&acceleration, &gyro, &temperature);

  float ax = acceleration.acceleration.x;
  float ay = acceleration.acceleration.y;
  float az = acceleration.acceleration.z;

  accelG = sqrt(ax * ax + ay * ay + az * az) / 9.80665;

  bool impact = (accelG >= FALL_IMPACT_G);
  bool freeFall = (accelG <= FREE_FALL_G);

  bool suddenChange =
      fabs(accelG - previousAccelG) >= 1.2;

  previousAccelG = accelG;

  if ((impact || freeFall) &&
      suddenChange &&
      millis() - lastFallTime >= 3000) {

    fallDetected = true;
    lastFallTime = millis();

    Serial.println("POSSIBLE FALL DETECTED");
  }
}

// ============================================================
// READ MAX30102 HEART RATE
// ============================================================

void readHeartRate() {

  if (!maxOK) {
    return;
  }

  long irValue = particleSensor.getIR();

  // No sufficient optical contact.
  if (irValue < 50000) {
    return;
  }

  if (checkForBeat(irValue)) {

    unsigned long currentTime = millis();

    if (lastHeartBeat > 0) {

      unsigned long interval =
          currentTime - lastHeartBeat;

      if (interval > 0) {

        float bpm = 60000.0 / interval;

        if (bpm >= 30 && bpm <= 220) {

          heartRateSamples[heartRateIndex] =
              (uint16_t)bpm;

          heartRateIndex =
              (heartRateIndex + 1) % 4;

          if (validHeartRateSamples < 4) {
            validHeartRateSamples++;
          }

          uint32_t total = 0;

          for (int i = 0;
               i < validHeartRateSamples;
               i++) {
            total += heartRateSamples[i];
          }

          heartRate =
              (float)total / validHeartRateSamples;
        }
      }
    }

    lastHeartBeat = currentTime;
  }
}

// ============================================================
// READ GPS
// ============================================================

void readGPS() {

  while (GPSSerial.available()) {
    gps.encode(GPSSerial.read());
  }

  gpsFix = gps.location.isValid() &&
           gps.location.age() < 10000;
}

// ============================================================
// READ DS18B20
// ============================================================

void readTemperature() {

  // Conversion is handled asynchronously.
  if (millis() - lastTempRequest >= 1000) {

    float reading = tempSensor.getTempCByIndex(0);

    if (reading != DEVICE_DISCONNECTED_C &&
        reading > -50 &&
        reading < 100) {

      bodyTemp = reading;
    }

    tempSensor.requestTemperatures();
    lastTempRequest = millis();
  }
}

// ============================================================
// START EMERGENCY ALARM
// ============================================================

void startAlarm() {

  if (alarmActive || smsSentForEvent) {
    return;
  }

  alarmActive = true;
  alarmStart = millis();

  digitalWrite(BUZZER_PIN, HIGH);
  digitalWrite(LED_PIN, HIGH);

  Serial.println("EMERGENCY ALARM STARTED");
}

// ============================================================
// LCD: EXACTLY TWO PAGES
// ============================================================

void updateLCD() {

  if (millis() - lastLcdUpdate < 250) {
    return;
  }

  lastLcdUpdate = millis();

  // Emergency countdown.
  if (alarmActive) {

    unsigned long elapsed = millis() - alarmStart;

    int remaining =
        (elapsed >= ALARM_TIME)
        ? 0
        : (ALARM_TIME - elapsed + 999) / 1000;

    lcdLine(0, "!!! ABNORMAL !!!");
    lcdLine(1, "Cancel: " + String(remaining) + " sec");

    return;
  }

  // Emergency event remains latched after SMS.
  if (smsSentForEvent) {

    lcdLine(0, "EMERGENCY ALERT");
    lcdLine(1, "SMS SENT/ACK BTN");

    return;
  }

  // Change between two pages every 2 seconds.
  if (millis() - lastPageChange >= 2000) {

    lastPageChange = millis();
    displayPage = (displayPage + 1) % 2;
  }

  // ---------------- PAGE 1 ----------------
  if (displayPage == 0) {

    String hr = (maxOK && heartRate > 0)
                ? String(heartRate, 0)
                : "--";

    String temp = isnan(bodyTemp)
                  ? "--.-"
                  : String(bodyTemp, 1);

    // Example: HR:78 T:36.5C
    lcdLine(0, "HR:" + hr + " T:" + temp + "C");

    // Example: FSR:1200 NORMAL
    String pressureText =
        pressureAbnormal ? "ABNORM" : "NORMAL";

    lcdLine(1, "FSR:" + String(fsrValue) +
               " " + pressureText);
  }

  // ---------------- PAGE 2 ----------------
  else {

    // Example: A:1.01g Fall:NO
    lcdLine(0, "A:" + String(accelG, 2) +
               "g Fall:" +
               (fallDetected ? "YES" : "NO"));

    String gpsStatus = gpsFix ? "FIX" : "NO FIX";

    String systemStatus =
        (fallDetected || pressureAbnormal)
        ? "ST:BAD"
        : "ST:OK";

    // Example: GPS:FIX ST:OK
    lcdLine(1, "GPS:" + gpsStatus + " " + systemStatus);
  }
}

// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(115200);

  pinMode(FSR_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  analogReadResolution(12);

  // Shared I2C bus.
  Wire.begin(21, 22);

  lcd.begin();
  lcd.backlight();

  lcdLine(0, "INDUSTRIAL");
  lcdLine(1, "SAFETY HARNESS");
  delay(1500);

  // DS18B20
  tempSensor.begin();
  tempSensor.setWaitForConversion(false);
  tempSensor.requestTemperatures();
  lastTempRequest = millis();

  // MPU6050
  if (mpu.begin()) {

    mpuOK = true;

    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    Serial.println("MPU6050 READY");

  } else {
    Serial.println("MPU6050 NOT FOUND");
  }

  // MAX30102
  if (particleSensor.begin(Wire, I2C_SPEED_FAST)) {

    maxOK = true;

    particleSensor.setup();
    particleSensor.setPulseAmplitudeRed(0x1F);
    particleSensor.setPulseAmplitudeIR(0x1F);

    Serial.println("MAX30102 READY");

  } else {
    Serial.println("MAX30102 NOT FOUND");
  }

  // GPS UART
  GPSSerial.begin(
      9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN
  );

  // GSM UART
  GSMSerial.begin(
      9600, SERIAL_8N1, GSM_RX_PIN, GSM_TX_PIN
  );

  lcdLine(0, "Initializing...");
  lcdLine(1, "Please Wait");
  delay(1500);

  displayPage = 0;
  lastPageChange = millis();

  Serial.println("SAFETY HARNESS READY");

  lcd.clear();
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop() {

  // Read sensors.
  readGPS();
  readFSR();
  readMPU6050();
  readHeartRate();
  readTemperature();

  // Cancel button uses INPUT_PULLUP.
  // Pressed = LOW.
  int buttonState = digitalRead(BUTTON_PIN);

  bool buttonPressed =
      (buttonState == LOW &&
       lastButtonState == HIGH);

  // ---------------- CANCEL BEFORE SMS ----------------

  if (alarmActive && buttonPressed) {

    alarmActive = false;
    fallDetected = false;
    pressureAbnormal = false;

    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);

    Serial.println("ALERT CANCELLED");

    lcdLine(0, "ALERT CANCELLED");
    lcdLine(1, "System Normal");

    // Avoid an immediate retrigger while the same
    // abnormal physical condition is still present.
    delay(300);
  }

  // ---------------- ACKNOWLEDGE AFTER SMS ----------------

  else if (smsSentForEvent && buttonPressed) {

    smsSentForEvent = false;
    fallDetected = false;
    pressureAbnormal = false;

    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);

    Serial.println("Emergency acknowledged");

    lcdLine(0, "ALERT ACKNOWLEDGED");
    lcdLine(1, "System Reset");

    delay(500);
  }

  lastButtonState = buttonState;

  // ---------------- TRIGGER ALARM ----------------

  if (!alarmActive &&
      !smsSentForEvent &&
      (fallDetected || pressureAbnormal)) {

    startAlarm();
  }

  // ---------------- 5-SECOND COUNTDOWN ----------------

  if (alarmActive &&
      millis() - alarmStart >= ALARM_TIME) {

    // Attempt SMS. Keep alarm outputs active until
    // the event is acknowledged by the user.
    lcdLine(0, "EMERGENCY ALERT");
    lcdLine(1, "Sending SMS...");

    bool sent = sendEmergencySMS();

    alarmActive = false;

    if (sent) {

      smsSentForEvent = true;

      Serial.println("EMERGENCY SMS SENT");

    } else {

      // Keep the event latched and outputs active.
      // Pressing the button acknowledges/resets it.
      smsSentForEvent = true;

      Serial.println("SMS FAILED - CHECK GSM");

      lcdLine(0, "SMS FAILED");
      lcdLine(1, "Check GSM/Network");
    }
  }

  // Keep buzzer and LED active during the countdown
  // and after an emergency SMS until acknowledgement.
  if (alarmActive || smsSentForEvent) {

    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);

  } else {

    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
  }

  updateLCD();

  // Debug information
  static unsigned long lastDebug = 0;

  if (millis() - lastDebug >= 1000) {

    lastDebug = millis();

    Serial.print("HR: ");
    Serial.print(heartRate);

    Serial.print(" | Temp: ");
    Serial.print(bodyTemp);

    Serial.print(" | FSR: ");
    Serial.print(fsrValue);

    Serial.print(" | Accel: ");
    Serial.print(accelG);

    Serial.print(" | GPS: ");
    Serial.print(gpsFix ? "FIX" : "NO FIX");

    Serial.print(" | Status: ");

    Serial.println(
      (fallDetected || pressureAbnormal)
      ? "ABNORMAL"
      : "NORMAL"
    );
  }
}