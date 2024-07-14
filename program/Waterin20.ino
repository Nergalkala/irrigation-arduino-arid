#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <RTClib.h>
#include <EEPROM.h>

LiquidCrystal_I2C lcd(0x27, 16, 2); // Dirección I2C 0x27, 16 columnas y 2 filas
RTC_DS3231 rtc;


const float MAX_TEMPERATURE = 36.0; // Maximum temperature threshold in Celsius
const int relayPin = 8;
const int sensor_pin = A0; // Soil Sensor input at Analog PIN A0
const int DHTPIN = 12;     // DHT sensor
const int TRIGGER_PIN = 11;
const int ECHO_PIN = 10;
const float BUCKET_HEIGHT = 25.5; // Height of the bucket in cm
const float BUCKET_CIRCUMFERENCE = 50.0; // Circumference of the bucket in cm
const float BUCKET_RADIUS = BUCKET_CIRCUMFERENCE / (2 * PI); // Calculated radius
const float BUCKET_VOLUME = PI * BUCKET_RADIUS * BUCKET_RADIUS * BUCKET_HEIGHT; // Calculated volume in cubic cm
const float SENSOR_OFFSET = 4.0; // Distance from sensor to top of bucket when full (adjust as needed)
int prev_output_value = -1;
float prev_temperature = -1;
float prev_waterLevel = -1;
int prev_pumpState = -1;
int prev_day = -1;
int prev_month = -1;
int prev_hour = -1;
int prev_minute = -1;

#define DHTTYPE DHT22   // DHT 22  (AM2302)
DHT dht(DHTPIN, DHTTYPE);

#define TIME_SET_FLAG_ADDRESS 0

int output_value;
float humidity, temperature;
long duration;
int distanceCm;
float calculateWaterLevel() {
  // Take multiple readings and average them
  long sum = 0;
  int readings = 5;
  for (int i = 0; i < readings; i++) {
    digitalWrite(TRIGGER_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIGGER_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIGGER_PIN, LOW);
    long duration = pulseIn(ECHO_PIN, HIGH);
    sum += duration * 0.034 / 2;
    delay(50); // Short delay between readings
  }
  float distanceCm = sum / readings;

  // Calculate water height
  float waterHeight = max(0, BUCKET_HEIGHT - (distanceCm - SENSOR_OFFSET));

  // Calculate water volume
  float waterVolume = PI * BUCKET_RADIUS * BUCKET_RADIUS * waterHeight;

  // Calculate water level percentage
  float waterLevelPercentage = (waterVolume / BUCKET_VOLUME) * 100;

  // Constrain to 0-100 range
  return constrain(waterLevelPercentage, 0, 100);
}
bool shouldActivatePump(int soilMoisture, float waterLevel, float temperature, int month, int hour) {
  if (soilMoisture < 20 && waterLevel > 20 && temperature < 36) {
    if (month >= 6 && month <= 9) {  // June to September
      return (hour < 11 || hour >= 20);
    } else {  // Other months
      return (hour < 12 || hour >= 18);
    }
  }
  return false;
}
bool checkValuesChanged(DateTime now, float waterLevel) {
  bool changed = false;
  
  if (output_value != prev_output_value) {
    prev_output_value = output_value;
    changed = true;
  }
  
  if (abs(temperature - prev_temperature) >= 0.5) {  // Check if temperature changed by 0.5°C or more
    prev_temperature = temperature;
    changed = true;
  }
  
  if (abs(waterLevel - prev_waterLevel) >= 1.0) {  // Check if water level changed by 1% or more
    prev_waterLevel = waterLevel;
    changed = true;
  }
  
  int currentPumpState = digitalRead(relayPin);
  if (currentPumpState != prev_pumpState) {
    prev_pumpState = currentPumpState;
    changed = true;
  }
  
  if (now.day() != prev_day || now.month() != prev_month) {
    prev_day = now.day();
    prev_month = now.month();
    changed = true;
  }
  
  if (now.hour() != prev_hour || now.minute() != prev_minute) {
    prev_hour = now.hour();
    prev_minute = now.minute();
    changed = true;
  }
  
  return changed;
}
void setup() {
  Serial.begin(9600);
  Wire.begin();  // Inicializa I2C
    Serial.println("I2C Scanner");
  byte error, address;
  int nDevices;
  nDevices = 0;
  for(address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address<16) {
        Serial.print("0");
      }
      Serial.println(address,HEX);
      nDevices++;
    }
  }
  if (nDevices == 0) {
    Serial.println("No I2C devices found\n");
  }
  else {
    Serial.println("done\n");
  }

  pinMode(relayPin, OUTPUT);
  pinMode(sensor_pin, INPUT);
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  dht.begin();

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  // Check if RTC time has been set before
  byte timeSetFlag = EEPROM.read(TIME_SET_FLAG_ADDRESS);

  if (timeSetFlag != 1) {
    // Time hasn't been set, so set it now
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    
    // Set the flag in EEPROM
    EEPROM.write(TIME_SET_FLAG_ADDRESS, 1);
    
    Serial.println("RTC time set to compilation time.");
  } else {
    Serial.println("RTC time already set.");
  }

  
  lcd.init();   // Starting LCD
  lcd.backlight();
  
  Serial.println("Reading From the Sensor ...");
  delay(2000);
}

void loop() {
  DateTime now = rtc.now();
  
  // Read moisture sensor
  output_value = analogRead(sensor_pin);
  output_value = map(output_value, 550, 10, 0, 100);
  
  // Read DHT sensor
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();

  // Read ultrasonic module
  float waterLevel = calculateWaterLevel();
  
  // Pump control
  bool shouldPump = shouldActivatePump(output_value, waterLevel, temperature, now.month(), now.hour());
  digitalWrite(relayPin, shouldPump ? LOW : HIGH);
  
  
  bool valuesChanged = checkValuesChanged(now, waterLevel);
  
  if (valuesChanged) {
    updateLCD(now, waterLevel);
  }
  printSerial(now, waterLevel);
  
  delay(3000);
}

void updateLCD(DateTime now, float waterLevel) {
  lcd.clear();
  
  // First row
  lcd.setCursor(0, 0);
  if (now.day() < 10) lcd.print('0');
  lcd.print(now.day());
  lcd.print('/');
  if (now.month() < 10) lcd.print('0');
  lcd.print(now.month());

  lcd.setCursor(6, 0);
  if (now.hour() < 10) lcd.print('0');
  lcd.print(now.hour(), DEC);
  lcd.print(':');
  if (now.minute() < 10) lcd.print('0');
  lcd.print(now.minute(), DEC);
  
  lcd.setCursor(12, 0);
  lcd.print("T:");
  int tempInt = (int)temperature;
  if (tempInt < 10) lcd.print('0');
  lcd.print(tempInt);
  lcd.print("C");
  
  // Second row
  lcd.setCursor(0, 1);
  lcd.print("S:");
  lcd.print(output_value);
  lcd.print("%");

  lcd.setCursor(6, 1);
  lcd.print("W:");
  if (waterLevel >= 100) {
    lcd.print("99");
  } else {
    if (waterLevel < 10) lcd.print("0");
    lcd.print(int(waterLevel)); // Cast to int for display
  }
  
  lcd.setCursor(12, 1);
  lcd.print("P:");
  lcd.print(digitalRead(relayPin) == LOW ? "ON" : "NO");
}

void printSerial(DateTime now, float waterLevel) {
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();

  Serial.print("Soil Moisture: ");
  Serial.print(output_value);
  Serial.println("%");
  
  Serial.print("Water Level: ");
  Serial.print(waterLevel, 1); // Print with 1 decimal place
  Serial.println("%");
  
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" C");

  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println("%");
  
  Serial.println(digitalRead(relayPin) == LOW ? "Pump ON" : "Pump OFF");
  Serial.println();
}
