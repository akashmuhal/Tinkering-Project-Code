#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "Firebase_ESP_Client.h"
#include <HX711_ADC.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define FIREBASE_API_KEY "AIzaSyByrM6puhU-bUW5fFyqWD8kS5bjrgx70H4"
#define FIREBASE_DATABASE_URL "https://tl-2024mcb1318-default-rtdb.firebaseio.com"
#define USER_EMAIL "muhalaakash2@gmail.com"
#define USER_PASSWORD "Chomu@suck1"
#define FIREBASE_SEAT_PATH "/seat/1"
#define WIFI_SSID "M32"
#define WIFI_PASSWORD "77777777"

const char* secretToken = "2024MCB1318";

#define PIR_PIN 36
#define HX711_DT_PIN 35
#define HX711_SCK_PIN 25
#define LED_PIN_OCCUPIED 12
#define LED_PIN_EMPTY 32
#define LED_PIN_RESERVED 33

const float CALIBRATION_FACTOR = 645.8;
const float OBJECT_THRESHOLD_GRAMS = 100.0;
HX711_ADC LoadCell(HX711_DT_PIN, HX711_SCK_PIN);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool firebaseReady = false;

WebServer server(80);

enum SeatState { STATE_EMPTY, STATE_OCCUPIED, STATE_RESERVED };
SeatState currentSeatState = STATE_EMPTY;
SeatState previousSeatState = STATE_EMPTY;
String loggedInUser = "none";
bool timerRunning = false;
bool isPersonPresent = false;
bool isObjectPresent = false;
volatile bool nfcTapPending = false;
String nfcUserTapped = "";
unsigned long lastTapInTime = 0;
unsigned long lastStateChangeTime = 0;
const long tapGracePeriod = 10000;
const long stateDebounceTime = 3000;
bool loadCellReady = false;
unsigned long lastStatusPrint = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n\n=================================");
  Serial.println("Smart Library Seat System v3.0");
  Serial.println("=================================\n");
  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN_OCCUPIED, OUTPUT);
  pinMode(LED_PIN_EMPTY, OUTPUT);
  pinMode(LED_PIN_RESERVED, OUTPUT);
  Serial.println("🔍 Testing LEDs...");
  testLEDs();
  connectToWiFi();
  initializeFirebase();
  setupLoadCell();
  setupServer();
  updateLedStatus();
  Serial.println("\n✅ Setup Complete!");
  Serial.println("==================================\n");
}

void loop() {
  server.handleClient();
  if (nfcTapPending) {
    processNfcTap();
  }
  readSensors();
  evaluateSeatState();
  if (currentSeatState != previousSeatState) {
    Serial.println("\n🔄 State Change: " + stateToString(previousSeatState) +
                   " → " + stateToString(currentSeatState));
    previousSeatState = currentSeatState;
    if (firebaseReady) {
      updateFirebase();
    } else {
      Serial.println("⚠️  Firebase not ready, skipping update");
    }
  }
  updateLedStatus();
  if (millis() - lastStatusPrint > 10000) {
    printSystemStatus();
    lastStatusPrint = millis();
  }
  delay(500);
}

void initializeFirebase() {
  Serial.println("🔥 Initializing Firebase...");
  config.api_key = FIREBASE_API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = FIREBASE_DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;
  fbdo.setBSSLBufferSize(4096, 1024);
  fbdo.setResponseSize(2048);
  Serial.println("   Calling Firebase.begin()...");
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Serial.println("   Waiting for token...");
  unsigned long startWait = millis();
  while (!Firebase.ready() && (millis() - startWait < 30000)) {
    Serial.print(".");
    delay(500);
  }
  if (Firebase.ready()) {
    firebaseReady = true;
    Serial.println("\n✅ Firebase Ready!");
    Serial.println("📤 Sending initial state...");
    updateFirebase();
  } else {
    firebaseReady = false;
    Serial.println("\n❌ Firebase authentication timeout!");
    Serial.println("   Will continue without Firebase");
  }
}

void updateFirebase() {
  if (!firebaseReady) {
    Serial.println("⚠️  Firebase not ready");
    return;
  }
  FirebaseJson json;
  json.set("status", stateToString(currentSeatState));
  json.set("user", loggedInUser);
  json.set("timerRunning", timerRunning);
  Serial.print("📤 Firebase update... ");
  if (Firebase.RTDB.setJSON(&fbdo, FIREBASE_SEAT_PATH, &json)) {
    Serial.println("✅ Success");
  } else {
    Serial.println("❌ Failed");
    Serial.println("   " + fbdo.errorReason());
    if (fbdo.errorReason().indexOf("auth") >= 0) {
      firebaseReady = false;
      Serial.println("   Will retry authentication...");
    }
  }
}

void evaluateSeatState() {
  if (currentSeatState == STATE_EMPTY) {
    return;
  }
  if (millis() - lastTapInTime < tapGracePeriod) {
    return;
  }
  if (isObjectPresent && isPersonPresent) {
    if (currentSeatState != STATE_OCCUPIED) {
      if (millis() - lastStateChangeTime > stateDebounceTime) {
        currentSeatState = STATE_OCCUPIED;
        timerRunning = false;
        lastStateChangeTime = millis();
        Serial.println("👤 User returned");
      }
    }
  } else if (isObjectPresent && !isPersonPresent) {
    if (currentSeatState != STATE_RESERVED) {
      if (millis() - lastStateChangeTime > stateDebounceTime) {
        currentSeatState = STATE_RESERVED;
        timerRunning = true;
        lastStateChangeTime = millis();
        Serial.println("⏰ User away, seat reserved");
      }
    }
  }
}

void testLEDs() {
  Serial.println("  RED (Occupied)...");
  digitalWrite(LED_PIN_OCCUPIED, HIGH);
  digitalWrite(LED_PIN_EMPTY, LOW);
  digitalWrite(LED_PIN_RESERVED, LOW);
  delay(1000);
  Serial.println("  GREEN (Empty)...");
  digitalWrite(LED_PIN_EMPTY, HIGH);
  digitalWrite(LED_PIN_OCCUPIED, LOW);
  digitalWrite(LED_PIN_RESERVED, LOW);
  delay(1000);
  Serial.println("  YELLOW (Reserved)...");
  digitalWrite(LED_PIN_RESERVED, HIGH);
  digitalWrite(LED_PIN_OCCUPIED, LOW);
  digitalWrite(LED_PIN_EMPTY, LOW);
  delay(1000);
  Serial.println("✅ LED test complete\n");
}

void updateLedStatus() {
  switch (currentSeatState) {
    case STATE_EMPTY:
      digitalWrite(LED_PIN_EMPTY, HIGH);
      digitalWrite(LED_PIN_OCCUPIED, LOW);
      digitalWrite(LED_PIN_RESERVED, LOW);
      break;
    case STATE_OCCUPIED:
      digitalWrite(LED_PIN_EMPTY, LOW);
      digitalWrite(LED_PIN_OCCUPIED, HIGH);
      digitalWrite(LED_PIN_RESERVED, LOW);
      break;
    case STATE_RESERVED:
      digitalWrite(LED_PIN_EMPTY, LOW);
      digitalWrite(LED_PIN_OCCUPIED, LOW);
      digitalWrite(LED_PIN_RESERVED, HIGH);
      break;
  }
}

void setupServer() {
  server.on("/scan", HTTP_POST, []() {
    if (server.method() != HTTP_POST) {
      server.send(405, "text/plain", "Use POST");
      return;
    }
    String body = server.arg("plain");
    Serial.println("\n📩 NFC POST: " + body);
    StaticJsonDocument<200> doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
      Serial.println("❌ JSON parse failed");
      server.send(400, "text/plain", "Invalid JSON");
      return;
    }
    String tag = doc["tag"];
    String token = doc["token"];
    if (token != secretToken) {
      Serial.println("🚫 Unauthorized");
      server.send(401, "text/plain", "Unauthorized");
      return;
    }
    Serial.println("✅ Valid tag: " + tag);
    handleNfcTap(tag);
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  });
  server.begin();
  Serial.println("🌐 Server started at http://" + WiFi.localIP().toString() + "/scan\n");
}

void handleNfcTap(String tappedUser) {
  nfcUserTapped = tappedUser;
  nfcTapPending = true;
}

void processNfcTap() {
  Serial.println("\n🏷️  Processing tap: " + nfcUserTapped);
  if (currentSeatState == STATE_EMPTY) {
    currentSeatState = STATE_OCCUPIED;
    loggedInUser = nfcUserTapped;
    timerRunning = false;
    lastTapInTime = millis();
    lastStateChangeTime = millis();
    Serial.println("✅ TAP IN - " + loggedInUser);
  } else if (nfcUserTapped == loggedInUser) {
    currentSeatState = STATE_EMPTY;
    loggedInUser = "none";
    timerRunning = false;
    lastStateChangeTime = millis();
    Serial.println("✅ TAP OUT");
  } else {
    Serial.println("⚠️  Wrong user! Occupied by: " + loggedInUser);
  }
  nfcTapPending = false;
  nfcUserTapped = "";
}

void readSensors() {
  isPersonPresent = (digitalRead(PIR_PIN) == HIGH);
  if (loadCellReady && LoadCell.update()) {
    float weight = LoadCell.getData();
    isObjectPresent = (weight > OBJECT_THRESHOLD_GRAMS);
  }
}

void setupLoadCell() {
  Serial.print("⚖️  Load Cell init... ");
  LoadCell.begin();
  LoadCell.start(2000, true);
  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("❌ Timeout");
    loadCellReady = false;
  } else {
    LoadCell.setCalFactor(CALIBRATION_FACTOR);
    Serial.println("✅ Ready");
    loadCellReady = true;
  }
}

void connectToWiFi() {
  Serial.print("📡 WiFi: " + String(WIFI_SSID) + " ");
  WiFi.persistent(false);
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setSleep(false);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start < 20000)) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Connected: " + WiFi.localIP().toString() + "\n");
  } else {
    Serial.println("\n❌ Failed! Restarting...");
    delay(1000);
    ESP.restart();
  }
}

String stateToString(SeatState state) {
  switch (state) {
    case STATE_OCCUPIED: return "OCCUPIED";
    case STATE_RESERVED: return "RESERVED";
    default: return "EMPTY";
  }
}

void printSystemStatus() {
  Serial.println("\n--- Status ---");
  Serial.println("State: " + stateToString(currentSeatState));
  Serial.println("User: " + loggedInUser);
  Serial.println("Person: " + String(isPersonPresent ? "YES" : "NO"));
  Serial.println("Object: " + String(isObjectPresent ? "YES" : "NO"));
  Serial.println("Firebase: " + String(firebaseReady ? "Ready" : "Not Ready"));
  Serial.println("WiFi: " + String(WiFi.status() == WL_CONNECTED ? "OK" : "FAIL"));
  if (loadCellReady && LoadCell.update()) {
    Serial.println("Weight: " + String(LoadCell.getData(), 1) + "g");
  }
  Serial.println("-------------\n");
}