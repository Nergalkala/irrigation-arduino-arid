#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <RTClib.h>
#include <EEPROM.h>


LiquidCrystal_I2C lcd(0x27, 16, 2); // Direcci�n I2C 0x27, 16 columnas y 2 filas
RTC_DS3231 rtc;



const float MAX_TEMPERATURE = 34.0; // Maximum temperature threshold in Celsius
const int relayPin = 8;
const int sensor_pin = A0; // Soil Sensor input at Analog PIN A0
const int DHTPIN = 12;     // DHT sensor
const int TRIGGER_PIN = 11;
const int ECHO_PIN = 10;


#define DHTTYPE DHT22   // DHT 22  (AM2302)
DHT dht(DHTPIN, DHTTYPE);


#define TIME_SET_FLAG_ADDRESS 0


int output_value;
float humidity, temperature;
long duration;
int distanceCm;


void setup() 
{
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


  
  lcd.init();   // Inicializa el LCD
  lcd.backlight();
  
  Serial.println("Reading From the Sensor ...");
  delay(2000);
}


void loop() {
  DateTime now = rtc.now();
  
  // Leer sensor de humedad del suelo
  output_value = analogRead(sensor_pin);
  output_value = map(output_value, 550, 10, 0, 100);
  
  // Leer DHT sensor
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();
  
  // Leer sensor ultras�nico
  digitalWrite(TRIGGER_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER_PIN, LOW);
  duration = pulseIn(ECHO_PIN, HIGH);
  distanceCm = duration * 0.034 / 2;
  
  // Control de la bomba
  bool shouldPump = false;
  
  if (output_value < 20) {  // Check if soil is dry
    if (temperature < 34.0) {  // Check temperature condition
      int currentMonth = now.month();
      int currentHour = now.hour();
      
      if (currentMonth >= 6 && currentMonth <= 9) {  // June to September
        if (currentHour < 11 || currentHour >= 20) {
          shouldPump = true;
        }
      } else {  // Other months
        if (currentHour < 12 || currentHour >= 18) {
          shouldPump = true;
        }
      }
    }
  }
  
  if (shouldPump) {
    digitalWrite(relayPin, LOW);  // Turn pump ON
  } else {
    digitalWrite(relayPin, HIGH);  // Turn pump OFF
  }
  
  
  updateLCD(now);
  printSerial(now);
  
  delay(3000);
}


void updateLCD(DateTime now) {
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
  int waterLevel = 100 - map(distanceCm, 0, 20, 0, 100); // Assuming 20cm is the max depth
  waterLevel = constrain(waterLevel, 0, 99); // Ensure it's between 0 and 99
  if (waterLevel < 10) lcd.print('0');
  lcd.print(waterLevel);
  lcd.print("%");
  
  lcd.setCursor(12, 1);
  lcd.print("P:");
  lcd.print(digitalRead(relayPin) == LOW ? "ON" : "NO");
}


void printSerial(DateTime now) {
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
  Serial.print(distanceCm);
  Serial.println(" cm");
  
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" C");


  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println("%");
  
  Serial.println(digitalRead(relayPin) == LOW ? "Pump ON" : "Pump OFF");
  Serial.println();
}
