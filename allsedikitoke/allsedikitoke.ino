//define Library
#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <EEPROM.h>
#define Solenoid 15
#define RX_PIN 16
#define TX_PIN 17
#define SDA_PIN 21
#define SCL_PIN 22
#define EEPROM_SIZE 512
#define RFID_START_ADDR 0
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger(&mySerial);
Adafruit_PN532 rfid(SDA_PIN, SCL_PIN);
TaskHandle_t taskRFIDHandle = NULL;
TaskHandle_t taskFingerprintHandle = NULL;
bool enrollMode = false;
int id;
int fingerprintCounter = 0;
int lastEEPROMAddr = RFID_START_ADDR;
int rfidCounter = 0;
uint8_t uid[7];
uint8_t uidLength;

void setup() {
  Serial.begin(115200);
  while (!Serial);
  mySerial.begin(57600, SERIAL_8N1, RX_PIN, TX_PIN);
  rfid.begin();
  rfid.SAMConfig();
  EEPROM.begin(EEPROM_SIZE);
  pinMode(Solenoid, OUTPUT);
  digitalWrite(Solenoid, LOW);
  lastEEPROMAddr = RFID_START_ADDR;
  while (lastEEPROMAddr < EEPROM_SIZE) {
    if (EEPROM.read(lastEEPROMAddr) == 0xFF) {
      lastEEPROMAddr++;
      break;
    }
    lastEEPROMAddr++;
  }
  xTaskCreate(taskSensorRFID, "taskRFID", 2048, NULL, 1, &taskRFIDHandle);
  xTaskCreate(taskSensorFingerprint, "taskFingerprint", 2048, NULL, 1, &taskFingerprintHandle);
}


void loop() {
  if (Serial.available()) {
    String input = Serial.readString();
    input.trim();
    if (input == "enroll_rfid") {
      enrollMode = true;
      Serial.println("Entering enroll RFID mode...");
      vTaskSuspend(taskRFIDHandle); vTaskSuspend(taskFingerprintHandle);
      enrollRFID();
      enrollMode = false;
      vTaskResume(taskFingerprintHandle); vTaskResume(taskRFIDHandle);
    }
    else if (input == "enroll_fingerprint") {
      enrollMode = true;
      Serial.println("Entering enroll Fingerprint mode...");
      vTaskSuspend(taskRFIDHandle);
      vTaskSuspend(taskFingerprintHandle);
      enrollFingerprint();
      enrollMode = false;
      vTaskResume(taskRFIDHandle);
      vTaskResume(taskFingerprintHandle);
      }
  }
  if (!enrollMode) {
    yield();    
    vTaskResume(taskRFIDHandle);
    vTaskResume(taskFingerprintHandle);
  }
  
}

void taskSensorRFID(void *pvParameters) {
  rfid.begin();
  rfid.SAMConfig();
  EEPROM.begin(EEPROM_SIZE);
    if (rfid.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
      Serial.print("RFID Detected: ");
      for (int i = 0; i < uidLength; i++) {
        Serial.print(uid[i], HEX);
      }
      Serial.println();
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
          Serial.println("RFID Match! DOOR UNLOCK...");
          digitalWrite(Solenoid, HIGH);
        } else {
          Serial.println("RFID Match! DOOR LOCK...");
          digitalWrite(Solenoid, LOW);
        }
      } else {
        Serial.println("RFID not recognized.");
      }
    }
  }

void taskSensorFingerprint(void *pvParameters) {
  while (true) {
    if (finger.getImage() == FINGERPRINT_OK) {
      if (finger.image2Tz() == FINGERPRINT_OK && finger.fingerSearch() == FINGERPRINT_OK) {
        Serial.print("Fingerprint Matched. ID: ");
        Serial.println(finger.fingerID);

        fingerprintCounter++; // Tambahkan counter setiap sidik jari valid terdeteksi
        if (fingerprintCounter % 2 == 1) {
          Serial.println("Turning on LED (Odd Count)..."); 
          digitalWrite(Solenoid, HIGH);
        } else {
          Serial.println("Turning off LED (Even Count)..."); 
          digitalWrite(Solenoid, LOW);
        }
      } else {
        Serial.println("Fingerprint not recognized.");
      }
    } else {
      Serial.println("No finger detected.");
    }
    vTaskDelay(100 / portTICK_PERIOD_MS); // Delay untuk memberikan waktu bagi task lain
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
  while (finger.getImage() != FINGERPRINT_OK);

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
}

void enrollRFID() {
  Serial.println("Place your RFID card near the reader...");
  if (rfid.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
    Serial.print("New RFID detected: ");
    for (int i = 0; i < uidLength; i++) {
      Serial.print(uid[i], HEX);
      EEPROM.write(lastEEPROMAddr++, uid[i]);
    }
    EEPROM.write(lastEEPROMAddr++, 0xFF);
    EEPROM.commit();
    Serial.println(" - Added to database."); Serial.println("Returning to action mode...");
  } else {
    Serial.println("No RFID detected. Try again.");
  }  
  rfid.begin();  rfid.SAMConfig();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  Serial.println("Exciting enroll RFID Mode");
}
