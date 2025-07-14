#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
//================================================================PIN DEFINE=====================================================
#define SS_PIN            10
#define SCK_PIN           11
#define MOSI_PIN          12
#define MISO_PIN          13
#define RST_PIN           9
#define DOOR_PIN          17
          
#define SDA_PIN           8
#define SCL_PIN           3
#define DHT_SENSOR_PIN    18
#define DHT_SENSOR_TYPE   DHT11

#define RAIN_SENSOR_PIN   4
#define WINDOW_PIN        15
#define CPD_PIN           16

#define FLAME_SENSOR_PIN  2
#define BUZZER_PIN        5
#define FLAME_DETECTED    LOW 

#define LED_PIN            6   
#define LIGHT_SENSOR_PIN   35  
//================================================================CLASS DEFINITIONS=============================================
// RFID Door Control Class
  class RFIDDoorControl {
  private:
      MFRC522 rfid;
      Servo door;
      byte authorizedUID1[4] = {0x83, 0x0A, 0x44, 0x29};
      byte authorizedUID2[4] = {0xC3, 0xA7, 0x66, 0x14};
      int angle = 0;

  public:
      RFIDDoorControl() : rfid(SS_PIN, RST_PIN) {}

      void setup() {
          SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
          rfid.PCD_Init();
          door.attach(DOOR_PIN);
          door.write(angle);
          Serial.println("Tap RFID/NFC Tag on reader");
      }

      void process() {
          if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
              if (checkUID(authorizedUID1) || checkUID(authorizedUID2)) {
                  Serial.println("Authorized Tag");
                  angle = (angle == 0) ? 90 : 0;
                  door.write(angle);
                  Serial.printf("Rotate Servo Motor to %d°\n", angle);
              } else {
                  Serial.println("Unauthorized Tag");
              }
              rfid.PICC_HaltA();
              rfid.PCD_StopCrypto1();
          }
      }

  private:
      bool checkUID(const byte *uid) {
          return memcmp(rfid.uid.uidByte, uid, 4) == 0;
      }
  };

// DHT LCD Display Class
    class DHTLCD {
      private:
            DHT dht_sensor;
            LiquidCrystal_I2C lcd;

    public:
          DHTLCD() : dht_sensor(DHT_SENSOR_PIN, DHT_SENSOR_TYPE), lcd(0x27, 16, 2) {}

          void setup() {
              Wire.begin(SDA_PIN, SCL_PIN);
              dht_sensor.begin();
              lcd.init();
              lcd.backlight();
          }

          void process() {
                float humi = dht_sensor.readHumidity();
                float tempC = dht_sensor.readTemperature();
                if (isnan(tempC) || isnan(humi)) {
                    lcd.setCursor(0, 0);
                    lcd.print("Failed");
                } else {
                    lcd.setCursor(0, 0);
                    lcd.printf("Temp: %.1fC ", tempC);
                    lcd.setCursor(0, 1);
                    lcd.printf("Humi: %.1f%%", humi);
                }
                delay(2000);
            }

      };

// Rain Sensor and Servo Control Class
    class RainSensorControl {
    private:
        Servo window_servo;
        Servo CPD_servo2;
        int prev_rain_state = HIGH;
        int rain_state = HIGH;

    public:
        void setup() {
            pinMode(RAIN_SENSOR_PIN, INPUT);
            window_servo.attach(WINDOW_PIN);
            CPD_servo2.attach(CPD_PIN);
            window_servo.write(0);
            CPD_servo2.write(0);
            rain_state = digitalRead(RAIN_SENSOR_PIN);
        }

        void process() {
            prev_rain_state = rain_state;
            rain_state = digitalRead(RAIN_SENSOR_PIN);
            if (rain_state == LOW && prev_rain_state == HIGH) {
                Serial.println("Rain detected!");
                window_servo.write(90);
                CPD_servo2.write(90);
            } else if (rain_state == HIGH && prev_rain_state == LOW) {
                Serial.println("Rain stopped!");
                window_servo.write(0);
                CPD_servo2.write(0);
            }
        }
    };

// Flame and Buzzer Alarm Class
 class FlameAlarm {
    private:
        bool flameDetected = false;

    public:
        void setup() {
            pinMode(FLAME_SENSOR_PIN, INPUT); // Set flame sensor pin as input
            pinMode(BUZZER_PIN, OUTPUT); // Set buzzer pin as output
        }

        void process() {
            int flameValue = digitalRead(FLAME_SENSOR_PIN); // Read digital state of the flame sensor
            Serial.printf("Flame state: %d\n", flameValue);
            if (flameValue == FLAME_DETECTED) { // Check if flame is detected (LOW)
                if (!flameDetected) {
                    flameDetected = true;
                    Serial.println("Warning: Flame detected! Activating alarm.");
                }
                playFireAlarm();
            } else {
                if (flameDetected) {
                    flameDetected = false;
                    Serial.println("Stopping alarm.");
                }
                noTone(BUZZER_PIN); // Stop buzzer
            }
            delay(500); // Small delay to avoid flooding serial output
        }

    private:
        void playFireAlarm() {
            const int frequencyMin = 500;
            const int frequencyMax = 2000;
            const int stepDelay = 5;
            for (int freq = frequencyMin; freq <= frequencyMax; freq += 10) {
                tone(BUZZER_PIN, freq);
                delay(stepDelay);
            }
            for (int freq = frequencyMax; freq >= frequencyMin; freq -= 10) {
                tone(BUZZER_PIN, freq);
                delay(stepDelay);
            }
        }
  };
//Light LED:
  class LightController {
    private:
      int ledPin;
      int lightSensorPin;
      
      int lightStateCurrent;
      int lightStatePrevious;

    public:
      // Constructor
      LightController(int ledPin, int lightSensorPin) 
        : ledPin(ledPin), lightSensorPin(lightSensorPin), 
          lightStateCurrent(LOW), lightStatePrevious(LOW) {}

      // Initialize pins
      void setup() {
        pinMode(ledPin, OUTPUT);        // Set LED pin as output
        pinMode(lightSensorPin, INPUT); // Set light sensor pin as input

      }

      // Update function to handle motion detection and LED control
      void process() {
        lightStatePrevious = lightStateCurrent;             // Store old state
        lightStateCurrent = digitalRead(lightSensorPin);         // Read the light sensor state

        // Light detected 
        if (lightStatePrevious == LOW && lightStateCurrent == HIGH)  { 
          Serial.println("Motion detected!, turns LED ON");
          digitalWrite(ledPin, HIGH); // Turn LED ON
        } 
        // Light stopped
        else if (lightStatePrevious == HIGH && lightStateCurrent == LOW) { 
          Serial.println("Motion stopped!, turns LED OFF");
          digitalWrite(ledPin, LOW); // Turn LED OFF
        }
      }
    };

//================================================================MAIN PROGRAM===================================================
RFIDDoorControl rfidDoor;
DHTLCD dhtLCD;
RainSensorControl rainSensor;
FlameAlarm flameAlarm;
LightController LightController(LED_PIN, LIGHT_SENSOR_PIN);

void setup() {
    Serial.begin(115200);
    rfidDoor.setup();
    dhtLCD.setup();
    rainSensor.setup();  
    flameAlarm.setup();
    LightController.setup();

}
unsigned long previousMillis = 0;
const long interval = 500; // 500ms
void loop() {
        rfidDoor.process();
        dhtLCD.process();
        rainSensor.process();
        flameAlarm.process();
        LightController.process();
    }
    

      