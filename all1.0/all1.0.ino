#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <EEPROM.h>

#define Solenoid 34
#define RX_PIN 16
#define TX_PIN 17
#define SDA_PIN 21
#define SCL_PIN 22
#define EEPROM_SIZE 512
#define RFID_START_ADDR 0

HardwareSerial mySerial(2);
Adafruit_Fingerprint finger(&mySerial);
Adafruit_PN532 rfid(SDA_PIN, SCL_PIN);

bool enrollMode = false;
int id;
int fingerprintCounter = 0; // Counter untuk mendeteksi jumlah sidik jari valid
int rfidCounter = 0; // Counter untuk mendeteksi jumlah RFID yang terdeteksi
int lastEEPROMAddr = RFID_START_ADDR; // Alamat terakhir yang digunakan untuk menyimpan UID RFID

uint8_t uid[7];
uint8_t uidLength;

void setup() {
  Serial.begin(115200);
  while (!Serial);
  mySerial.begin(57600, SERIAL_8N1, RX_PIN, TX_PIN);
  
  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor ready...");
  } else {
    Serial.println("Fingerprint sensor not detected!");
    while (true); // Berhenti jika sensor tidak terdeteksi
  }
  
  rfid.begin();
  rfid.SAMConfig();
  Serial.println("RFID PN532 ready...");
  
  pinMode(Solenoid, OUTPUT);
  digitalWrite(Solenoid, LOW);

  // Cari alamat terakhir yang digunakan di EEPROM untuk RFID
  lastEEPROMAddr = RFID_START_ADDR;
  while (lastEEPROMAddr < EEPROM_SIZE) {
    if (EEPROM.read(lastEEPROMAddr) == 0xFF) {
      lastEEPROMAddr++;
      break;
    }
    lastEEPROMAddr++;
  }
  Serial.print("Last EEPROM Address: ");
  Serial.println(lastEEPROMAddr);
}

void loop() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command == "enroll_fp") {
      Serial.println("Entering enroll mode for Fingerprint...");
      enrollMode = true;
    } else if (command == "enroll") {
      Serial.println("Entering enroll mode for RFID...");
      enrollMode = true;
    }
  }

  if (enrollMode) {
    // Enroll mode, check which type to process
    if (Serial.available()) {
      String command = Serial.readStringUntil('\n');
      command.trim();
      if (command == "enroll_fp") {
        enrollFingerprint();
      } else if (command == "enroll") {
        enrollRFID();
      }
    }
  } else {
    actionFingerprint();
    actionRFID();
  }
}

void enrollFingerprint() {
  Serial.println("Enter an ID for the fingerprint (1-127):");
  while (true) {
    if (Serial.available()) {
      id = Serial.parseInt();
      if (id > 0 && id <= 127) break;
      Serial.println("Invalid ID. Use an ID between 1 and 127:");
    }
  }

  Serial.print("Enrolling fingerprint with ID: ");
  Serial.println(id);

  // Proses pendaftaran sidik jari
  Serial.println("Place your finger on the sensor...");
  while (finger.getImage() != FINGERPRINT_OK); // Tunggu hingga sidik jari terdeteksi

  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    Serial.println("Error converting fingerprint image.");
  } else {
    Serial.println("Remove your finger...");
    delay(2000);

    while (finger.getImage() != FINGERPRINT_NOFINGER);
    Serial.println("Place the same finger again...");
    while (finger.getImage() != FINGERPRINT_OK);

    if (finger.image2Tz(2) != FINGERPRINT_OK) {
      Serial.println("Error converting fingerprint image.");
    } else if (finger.createModel() != FINGERPRINT_OK) {
      Serial.println("Error creating model.");
    } else if (finger.storeModel(id) == FINGERPRINT_OK) {
      Serial.println("Fingerprint enrolled successfully!");
    } else {
      Serial.println("Error saving fingerprint.");
    }
  }

  enrollMode = false;
  Serial.println("Returning to action mode...");
}

void enrollRFID() {
  Serial.println("Place your RFID card near the reader...");
  if (rfid.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
    Serial.print("New RFID detected: ");
    for (int i = 0; i < uidLength; i++) {
      Serial.print(uid[i], HEX);
      EEPROM.write(lastEEPROMAddr++, uid[i]); // Menyimpan UID ke EEPROM
    }
    EEPROM.write(lastEEPROMAddr++, 0xFF); // Tanda akhir UID
    EEPROM.commit(); // Menyimpan perubahan ke EEPROM
    Serial.println(" - Added to database.");
    Serial.println("Returning to action mode...");
  } else {
    Serial.println("No RFID detected. Try again.");
  }

  enrollMode = false;
  Serial.println("Returning to action mode...");
}

void actionFingerprint() {
  if (finger.getImage() == FINGERPRINT_OK) {
    if (finger.image2Tz() == FINGERPRINT_OK && finger.fingerSearch() == FINGERPRINT_OK) {
      Serial.print("Fingerprint Matched. ID: ");
      Serial.println(finger.fingerID);

      // Logika Counter
      fingerprintCounter++; // Tambahkan counter setiap sidik jari valid terdeteksi
      if (fingerprintCounter % 2 == 1) {
        Serial.println("Turning on LED (Odd Count)...");  // LED ON jika hitungan ganjil
        digitalWrite(Solenoid, HIGH);
      } else {
        Serial.println("Turning off LED (Even Count)..."); // LED OFF jika hitungan genap
        digitalWrite(Solenoid, LOW);
      }
    } else {
      Serial.println("Fingerprint not recognized.");
    }
  } else {
    Serial.println("No finger detected.");
  }

  delay(100);
}

void actionRFID() {
  if (rfid.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
    Serial.print("RFID Detected: ");
    for (int i = 0; i < uidLength; i++) {
      Serial.print(uid[i], HEX);
    }
    Serial.println();

    // Cek apakah UID terdaftar di EEPROM
    int addr = RFID_START_ADDR;
    bool matchFound = false;
    while (addr < lastEEPROMAddr) {
      bool match = true;
      for (int i = 0; i < uidLength; i++) {
        if (EEPROM.read(addr + i) != uid[i]) {
          match = false;
          break;
        }
      }
      addr += uidLength + 1;
      if (match) {
        matchFound = true;
        break;
      }
    }

    if (matchFound) {
      rfidCounter++; // Increment counter setiap kali RFID terdeteksi
      if (rfidCounter % 2 == 1) {
        Serial.println("RFID Match! Turning on LED...");
        digitalWrite(Solenoid, HIGH);
      } else {
        Serial.println("RFID Match! Turning off LED...");
        digitalWrite(Solenoid, LOW);
      }
    } else {
      Serial.println("RFID not recognized.");
    }
  }
  delay(100);
}
