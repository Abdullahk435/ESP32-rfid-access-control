
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>

// ---------------- PIN CONFIGURATION ----------------

#define RFID_SS       5
#define RFID_RST      27

#define SERVO_PIN     13
#define GREEN_LED     26
#define RED_LED       25
#define BUZZER_PIN    14

#define OLED_SDA      21
#define OLED_SCL      22

// ---------------- SETTINGS ----------------

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

const int LOCKED_ANGLE = 0;
const int UNLOCKED_ANGLE = 90;

const unsigned long UNLOCK_DURATION = 3000;
const unsigned long LOCKOUT_DURATION = 15000;
const int MAX_FAILED_ATTEMPTS = 3;

// ---------------- OBJECTS ----------------

MFRC522 rfid(RFID_SS, RFID_RST);
Servo doorServo;
Adafruit_SSD1306 display(
  SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1
);
Preferences preferences;

// ---------------- VARIABLES ----------------

String authorizedCards;

int failedAttempts = 0;

unsigned long lockoutUntil = 0;
unsigned long unlockUntil = 0;

bool doorUnlocked = false;
bool oledReady = false;

// ---------------- DISPLAY ----------------

void showMessage(String line1, String line2 = "") {
  if (!oledReady) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 10);
  display.println(line1);

  display.setCursor(0, 30);
  display.println(line2);

  display.display();
}

// ---------------- AUDIT LOGGER ----------------

// Uptime timestamp: HH:MM:SS since ESP32 boot.
// This is not a real calendar timestamp.

String getTimestamp() {
  unsigned long totalSeconds = millis() / 1000;

  unsigned long hours = totalSeconds / 3600;
  unsigned long minutes = (totalSeconds / 60) % 60;
  unsigned long seconds = totalSeconds % 60;

  char timestamp[20];

  snprintf(
    timestamp, sizeof(timestamp),
    "%02lu:%02lu:%02lu",
    hours, minutes, seconds
  );

  return String(timestamp);
}

void auditLog(String event, String uid) {
  Serial.print("[");
  Serial.print(getTimestamp());
  Serial.print("] ");
  Serial.print(event);
  Serial.print(" | UID: ");
  Serial.println(uid);
}

// ---------------- UID HANDLING ----------------

// Converts 01:02:03:04 into 01020304.
// Also accepts spaces and hyphens.

String normalizeUID(String input) {
  input.trim();
  input.toUpperCase();

  String result = "";

  for (unsigned int i = 0; i < input.length(); i++) {
    char c = input.charAt(i);

    if (c == ':' || c == '-' || c == ' ') {
      continue;
    }

    if (!isHexadecimalDigit(c)) {
      return "";
    }

    result += c;
  }

  return result;
}

String readCardUID() {
  String uid = "";

  for (byte i = 0; i < rfid.uid.size; i++) {
    char hexByte[3];

    snprintf(
      hexByte, sizeof(hexByte),
      "%02X", rfid.uid.uidByte[i]
    );

    uid += hexByte;
  }

  return uid;
}

bool validUID(String uid) {
  // Common UID lengths: 4, 7 or 10 bytes.
  if (uid.length() != 8 &&
      uid.length() != 14 &&
      uid.length() != 20) {
    return false;
  }

  return true;
}

// ---------------- CREDENTIAL STORAGE ----------------

// Exact token matching avoids partial UID matches.

bool isAuthorized(String uid) {
  int start = 0;

  while (start < authorizedCards.length()) {
    int comma = authorizedCards.indexOf(',', start);

    String card;

    if (comma == -1) {
      card = authorizedCards.substring(start);
      start = authorizedCards.length();
    } else {
      card = authorizedCards.substring(start, comma);
      start = comma + 1;
    }

    card.trim();

    if (card == uid) {
      return true;
    }
  }

  return false;
}

void saveCredentials() {
  preferences.putString("cards", authorizedCards);
}

void addCard(String uid) {
  uid = normalizeUID(uid);

  if (!validUID(uid)) {
    Serial.println("Invalid UID format.");
    return;
  }

  if (isAuthorized(uid)) {
    Serial.println("Card already registered.");
    return;
  }

  if (authorizedCards.length() > 0) {
    authorizedCards += ",";
  }

  authorizedCards += uid;
  saveCredentials();

  auditLog("ADMIN_CARD_ADDED", uid);
  Serial.println("Card saved to ESP32 NVS.");
}

void removeCard(String uid) {
  uid = normalizeUID(uid);

  if (!validUID(uid)) {
    Serial.println("Invalid UID format.");
    return;
  }

  if (!isAuthorized(uid)) {
    Serial.println("Card not found.");
    return;
  }

  String updatedCards = "";
  int start = 0;

  while (start < authorizedCards.length()) {
    int comma = authorizedCards.indexOf(',', start);

    String card;

    if (comma == -1) {
      card = authorizedCards.substring(start);
      start = authorizedCards.length();
    } else {
      card = authorizedCards.substring(start, comma);
      start = comma + 1;
    }

    card.trim();

    if (card != uid) {
      if (updatedCards.length() > 0) {
        updatedCards += ",";
      }

      updatedCards += card;
    }
  }

  authorizedCards = updatedCards;
  saveCredentials();

  auditLog("ADMIN_CARD_REMOVED", uid);
  Serial.println("Card removed.");
}

void listCards() {
  Serial.println("Authorized UIDs:");

  if (authorizedCards.length() == 0) {
    Serial.println("(none)");
    return;
  }

  int start = 0;

  while (start < authorizedCards.length()) {
    int comma = authorizedCards.indexOf(',', start);

    if (comma == -1) {
      Serial.println(authorizedCards.substring(start));
      break;
    }

    Serial.println(authorizedCards.substring(start, comma));
    start = comma + 1;
  }
}

// ---------------- DOOR CONTROL ----------------

void lockDoor() {
  doorServo.write(LOCKED_ANGLE);
  doorUnlocked = false;
  unlockUntil = 0;

  digitalWrite(GREEN_LED, LOW);

  auditLog("DOOR_LOCKED", "-");
}

void unlockDoor(String uid) {
  doorServo.write(UNLOCKED_ANGLE);

  doorUnlocked = true;
  unlockUntil = millis() + UNLOCK_DURATION;

  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(RED_LED, LOW);

  tone(BUZZER_PIN, 2000, 150);

  showMessage("ACCESS GRANTED", uid);

  auditLog("ACCESS_GRANTED", uid);
}

void denyAccess(String uid) {
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);

  tone(BUZZER_PIN, 500, 500);

  showMessage("ACCESS DENIED", uid);

  auditLog("ACCESS_DENIED", uid);

  failedAttempts++;

  Serial.print("Failed attempts: ");
  Serial.println(failedAttempts);

  if (failedAttempts >= MAX_FAILED_ATTEMPTS) {
    lockoutUntil = millis() + LOCKOUT_DURATION;

    digitalWrite(RED_LED, HIGH);

    showMessage("SYSTEM LOCKED", "Wait 15 seconds");

    auditLog("LOCKOUT_STARTED", "-");

    Serial.println(
      "Too many failures. RFID access blocked for 15 seconds."
    );
  }
}

// ---------------- SERIAL ADMIN COMMANDS ----------------

// Commands in Serial Monitor:
// ADD 01020304
// REMOVE 01020304
// LIST
// HELP
//
// Simulation/demo only: USB serial commands are NOT
// authenticated and must not be used as production admin access.

void printHelp() {
  Serial.println();
  Serial.println("=== RFID ACCESS CONTROL ===");
  Serial.println("ADD 01020304    Register a card");
  Serial.println("REMOVE 01020304 Remove a card");
  Serial.println("LIST            List authorized UIDs");
  Serial.println("HELP            Show commands");
  Serial.println("===========================");
  Serial.println();
}

void handleSerialCommands() {
  if (!Serial.available()) return;

  String command = Serial.readStringUntil('\n');
  command.trim();

  String upperCommand = command;
  upperCommand.toUpperCase();

  if (upperCommand == "HELP") {
    printHelp();
    return;
  }

  if (upperCommand == "LIST") {
    listCards();
    return;
  }

  if (upperCommand.startsWith("ADD ")) {
    addCard(command.substring(4));
    return;
  }

  if (upperCommand.startsWith("REMOVE ")) {
    removeCard(command.substring(7));
    return;
  }

  Serial.println("Unknown command. Type HELP.");
}

// ---------------- SETUP ----------------

void setup() {
  Serial.begin(115200);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // OLED initialization
  Wire.begin(OLED_SDA, OLED_SCL);

  oledReady = display.begin(
    SSD1306_SWITCHCAPVCC, 0x3C
  );

  if (oledReady) {
    showMessage("RFID DOOR LOCK", "Initializing...");
  } else {
    Serial.println("OLED not detected; continuing.");
  }

  // Servo initialization
  doorServo.setPeriodHertz(50);
  doorServo.attach(SERVO_PIN, 500, 2400);
  doorServo.write(LOCKED_ANGLE);

  // RFID initialization
  SPI.begin(18, 19, 23, RFID_SS);

  rfid.PCD_Init();

  // Persistent authorized-card storage
  preferences.begin("rfidlock", false);

  authorizedCards = preferences.getString("cards", "");

  // Default demo card: 01:02:03:04
  // Initialize only if no saved credential list exists.
  if (!preferences.isKey("cards")) {
    authorizedCards = "01020304";
    saveCredentials();
  }

  Serial.println();
  Serial.println("ESP32 RFID Access Control Started.");
  Serial.println("Default demo UID: 01020304");
  Serial.println("Serial baud rate: 115200");

  printHelp();

  showMessage("SCAN YOUR CARD", "Door is locked");

  auditLog("SYSTEM_STARTED", "-");
}

// ---------------- MAIN LOOP ----------------

void loop() {
  handleSerialCommands();

  // Automatically relock after the unlock period.
  if (doorUnlocked &&
      (int32_t)(millis() - unlockUntil) >= 0) {
    lockDoor();
    showMessage("SCAN YOUR CARD", "Door is locked");
  }

  // End temporary lockout.
  if (lockoutUntil != 0 &&
      (int32_t)(millis() - lockoutUntil) >= 0) {
    lockoutUntil = 0;
    failedAttempts = 0;

    digitalWrite(RED_LED, LOW);

    auditLog("LOCKOUT_ENDED", "-");

    showMessage("SCAN YOUR CARD", "Ready");
  }

  // Check for a newly presented RFID card.
  if (!rfid.PICC_IsNewCardPresent()) {
    delay(5);
    return;
  }

  if (!rfid.PICC_ReadCardSerial()) {
    delay(5);
    return;
  }

  String uid = readCardUID();

  Serial.print("Card detected: ");
  Serial.println(uid);

  if (lockoutUntil != 0) {
    auditLog("ACCESS_BLOCKED_LOCKOUT", uid);

    showMessage("SYSTEM LOCKED", "Please wait");
  } else if (isAuthorized(uid)) {
    failedAttempts = 0;
    unlockDoor(uid);
  } else {
    denyAccess(uid);
  }

  // End the current RFID communication session.
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  delay(100);
}
