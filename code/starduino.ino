#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Servo.h>
#include <SPI.h>
#include <pitches.h>
#include <Wire.h>
#include <EEPROM.h>
#include <Crypto.h>
#include <SHA256.h>

// Create instances
LiquidCrystal_I2C lcd(0x27, 16, 2);
MFRC522 mfrc522(10, 9); // MFRC522 mfrc522(SS_PIN, RST_PIN)
Servo sg90;

// Initialize Pins for LEDs, servo, and buzzer
constexpr uint8_t greenLed = 7;
constexpr uint8_t redLed = 6;
constexpr uint8_t servoPin = 8;
constexpr uint8_t buzzerPin = 5;

String tagUID = "EE B6 08 17"; // String to store UID of tag
char password[4];              // Variable to store user's password input
boolean RFIDMode = true;       // Boolean to change modes
char key_pressed = 0;          // Variable to store incoming keys
uint8_t i = 0;                 // Counter variable

// Define how many rows and columns our keypad has
const byte rows = 4;
const byte columns = 4;

// Keypad pin map
char hexaKeys[rows][columns] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};

// Initialize pins for keypad
byte row_pins[rows] = {A0, A1, A2, A3};
byte column_pins[columns] = {4, 3, 2};

// Create instance for keypad
Keypad keypad_key = Keypad(makeKeymap(hexaKeys), row_pins, column_pins, rows, columns);

// SHA256 instance for hashing
SHA256 sha256;

// Melody for successful attempt (Star Wars)
int successMelody[] = {
  NOTE_AS4, NOTE_AS4, NOTE_AS4,
  NOTE_F5, NOTE_C6,
  NOTE_AS5, NOTE_A5, NOTE_G5, NOTE_F6, NOTE_C6,
  NOTE_AS5, NOTE_A5, NOTE_G5, NOTE_F6, NOTE_C6,
  NOTE_AS5, NOTE_A5, NOTE_AS5, NOTE_G5, NOTE_C5, NOTE_C5, NOTE_C5,
  NOTE_F5, NOTE_C6,
  NOTE_AS5, NOTE_A5, NOTE_G5, NOTE_F6, NOTE_C6,
  NOTE_AS5, NOTE_A5, NOTE_G5, NOTE_F6, NOTE_C6,
  NOTE_AS5, NOTE_A5, NOTE_AS5, NOTE_G5
};

// Note durations: 4 = quarter note, 8 = eighth note, etc.
int successNoteDurations[] = {
  8, 8, 8,
  2, 2,
  8, 8, 8, 2, 4,
  8, 8, 8, 2, 4,
  8, 8, 8, 2, 8, 8, 8,
  2, 2,
  8, 8, 8, 2, 4,
  8, 8, 8, 2, 4,
  8, 8, 8, 2
};

// Melody for failed attempt (Imperial March)
int failureMelody[] = {
  NOTE_A4, NOTE_A4, NOTE_A4, NOTE_F4, NOTE_C5, 
  NOTE_A4, NOTE_F4, NOTE_C5, NOTE_A4, 
  NOTE_E5, NOTE_E5, NOTE_E5, NOTE_F5, NOTE_C5, 
  NOTE_GS4, NOTE_F4, NOTE_C5, NOTE_A4,
  NOTE_A5, NOTE_A4, NOTE_A4, NOTE_A5, NOTE_GS5, NOTE_G5, 
  NOTE_FS5, NOTE_F5, NOTE_FS5, 0, NOTE_AS4, NOTE_DS5, NOTE_D5, NOTE_CS5, 
  NOTE_C5, NOTE_B4, NOTE_C5, 0, NOTE_F4, NOTE_GS4, NOTE_F4, NOTE_A4, 
  NOTE_C5, NOTE_A4, NOTE_C5, NOTE_E5
};

// Note durations: 4 = quarter note, 8 = eighth note, etc.
int failureNoteDurations[] = {
  4, 4, 4, 6, 16, 
  4, 6, 16, 2, 
  4, 4, 4, 6, 16, 
  4, 6, 16, 2,
  4, 6, 16, 4, 6, 16, 
  16, 16, 8, 8, 8, 4, 6, 16, 
  16, 16, 8, 8, 8, 4, 6, 16, 
  4, 6, 16, 2
};

void playSuccessMelody() {
  int melodyLength = sizeof(successMelody) / sizeof(successMelody[0]); // Calculate the length of the melody array
  for (int thisNote = 0; thisNote < melodyLength; thisNote++) {
    int noteDuration = 1000 / successNoteDurations[thisNote];
    tone(buzzerPin, successMelody[thisNote], noteDuration);
    delay(noteDuration * 1.1); // Add a small delay after each note
    noTone(buzzerPin);
    delay(50); // Add a short pause between notes
  }
}

void playFailureMelody() {
  int melodyLength = sizeof(failureMelody) / sizeof(failureMelody[0]); // Calculate the length of the melody array
  for (int thisNote = 0; thisNote < melodyLength; thisNote++) {
    int noteDuration = 1000 / failureNoteDurations[thisNote];
    tone(buzzerPin, failureMelody[thisNote], noteDuration);
    delay(noteDuration * 1.1); // Add a small delay after each note
    noTone(buzzerPin);
    delay(50); // Add a short pause between notes
  }
}

// Function to hash and store a password in EEPROM
void storePassword(const char *password) {
    byte hash[32]; // Buffer for SHA-256 hash

    sha256.reset();
    sha256.doUpdate((const byte *)password, strlen(password));
    sha256.doFinal(hash);

    // Write hash to EEPROM
    for (int i = 0; i < sizeof(hash); i++) {
        EEPROM.write(i, hash[i]);
    }
}

// Function to retrieve stored hash from EEPROM
void getStoredHash(byte *storedHash) {
    for (int i = 0; i < 32; i++) {
        storedHash[i] = EEPROM.read(i);
    }
}

// Function to authenticate user input by comparing hashes
bool authenticatePassword(const char *userInput) {
    byte inputHash[32];
    byte storedHash[32];

    getStoredHash(storedHash);

    sha256.reset();
    sha256.doUpdate((const byte *)userInput, strlen(userInput));
    sha256.doFinal(inputHash);

    return memcmp(inputHash, storedHash, sizeof(inputHash)) == 0;
}

// One-time function to set a new password via serial monitor
void setupPassword() {
    char newPassword[5]; // Buffer for a 4-digit password + null terminator

    Serial.println("Enter a new 4-digit password:");
    while (Serial.available() == 0) {
        // Wait for user input
    }

    int len = Serial.readBytesUntil('\n', newPassword, sizeof(newPassword) - 1);
    newPassword[len] = '\0'; // Null-terminate the string

    if (len == 4) { // Ensure it's exactly 4 digits
        storePassword(newPassword); // Hash and store it in EEPROM
        Serial.println("Password stored successfully!");
    } else {
        Serial.println("Invalid password length! Must be 4 digits.");
    }
}

void setup() {
    pinMode(buzzerPin, OUTPUT);
    pinMode(redLed, OUTPUT);
    pinMode(greenLed, OUTPUT);

    sg90.attach(servoPin); // Declare pin for servo
    sg90.write(0);         // Set initial position at locked state

    lcd.begin();   // LCD screen
    lcd.backlight();
    SPI.begin();   // Init SPI bus
    mfrc522.PCD_Init(); // Init RFID

    Serial.begin(9600); // Initialize serial communication

    lcd.clear();

    // Uncomment this line ONLY ONCE to set or reset the password:
    // setupPassword();
}

void loop() {
  if (RFIDMode == true) {
      lcd.setCursor(0, 0);
      lcd.print("Door Locked:");
      lcd.setCursor(0, 1);
      lcd.print("Scan Your Card");

      // Look for new cards
      if (!mfrc522.PICC_IsNewCardPresent()) {
          return;
      }

      // Select one of the cards
      if (!mfrc522.PICC_ReadCardSerial()) {
          return;
      }

      // Reading from the card
      String tag = "";
      for (byte j = 0; j < mfrc522.uid.size; j++) {
          tag.concat(String(mfrc522.uid.uidByte[j] < 0x10 ? " 0" : " "));
          tag.concat(String(mfrc522.uid.uidByte[j], HEX));
      }
      tag.toUpperCase();

      // Checking the card
      if (tag.substring(1) == tagUID) {
          lcd.clear();
          lcd.print("Tag Matched");
          digitalWrite(greenLed, HIGH);
          delay(3000);
          digitalWrite(greenLed, LOW);

          lcd.clear();
          lcd.print("Enter Password:");
          RFIDMode = false; // Switch to password entry mode
      } else {
          lcd.clear();
          lcd.print("Access Denied");
          playFailureMelody();
          digitalWrite(redLed, HIGH);
          delay(3000);
          digitalWrite(redLed, LOW);
          lcd.clear();
      }
  }

  if (RFIDMode == false) { 
      key_pressed = keypad_key.getKey(); 
      
      if (key_pressed) {
          password[i++] = key_pressed; 
          lcd.print("*"); 
      }

      if (i == 4) { 
          password[i] = '\0'; 

          if (authenticatePassword(password)) { 
                lcd.clear(); 
                lcd.setCursor(0, 0);
                lcd.print(" May The Force");
                lcd.setCursor(0, 1);
                lcd.print(" Be With You!");
                playSuccessMelody(); 
                sg90.write(90); 
                digitalWrite(greenLed, HIGH); 
                delay(3000); 
                digitalWrite(greenLed, LOW); 
                sg90.write(0); 
                RFIDMode = true; 
                i = 0; 
                Serial.println("Login Attempt: Success");
            } else { 
                lcd.clear(); 
                lcd.setCursor(0, 0);
                lcd.print("Failed You Have");
                lcd.setCursor(0, 1);
                lcd.print(" Access Denied");
                playFailureMelody(); 
                digitalWrite(redLed, HIGH); 
                delay(1500); 
                digitalWrite(redLed, LOW); 
                RFIDMode = true; 
                i = 0; 
                Serial.println("Login Attempt: Failure");
            }
        }
    }
}