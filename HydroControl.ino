#include "Particle.h"
#include "ThingSpeak.h"
#include "DS18B20.h"
#include "fonts.h"
#include "Adafruit_mfGFX.h"
#include "Adafruit_SSD1306.h"
#include "PietteTech_DHT.h"

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);

//Exhaust control
#define FANCONTROLPIN   D5 //Turns on/off the exhuast fan
bool turnExhuastFanON = false;
uint32_t prevFanTurnOnMillis = millis(); //prevents fan from turning on too frequently.


//DHT22 Sensor
#define DHTTYPE  DHT22              // Sensor type DHT11/21/22/AM2301/AM2302
#define DHTPIN   D3           	    // Digital pin for communications
#define DHT_SAMPLE_INTERVAL   2000  // Sample every two seconds
void dht_wrapper(); // must be declared before the lib initialization

PietteTech_DHT DHT(DHTPIN, DHTTYPE, dht_wrapper);
unsigned int prevSampleTime;	    // Next time we want to start sample
bool aquireDHTStarted;		    // flag to indicate we started acquisition
float dht22Temp = 0.0;
float dht22Humd = 0.0;
float prevDht22Temp = 0.0;
float prevDht22Humd = 0.0;
void dht_wrapper() {
  DHT.isrCallback();
}

//Temperature Sensors
uint32_t tempLastMillis = 0;
const int MAXRETRY = 3;
const int pinLED = D7;
const int nSENSORS = 2;
float tempSensor[nSENSORS] = {NAN, NAN};
const int pinOneWire = D2;

//ThingSpeak
uint32_t publishLastMillis = 0;
unsigned long myChannelNumber = 465928; //FermentorTemperature Channel
const char * myWriteAPIKey = "N3RADE7ZC8Z2X0UC";
float pinVoltage = 0;
uint8_t numCreds = 0;
uint8_t cellIsSet = 0;
WiFiAccessPoint ap[5];

retained uint8_t sensorAddresses[nSENSORS][8];
DS18B20 ds18b20(pinOneWire);
Adafruit_SSD1306 oled(200);       //Place holder does not perfrom any work.

TCPClient client;

void setCellCreds();
double getTemp(uint8_t addr[8]);

void setup() {
    Serial.begin(9600);

    pinMode(pinLED, OUTPUT);
    pinMode(FANCONTROLPIN, OUTPUT_PULLDOWN);

    oled.begin(SSD1306_SWITCHCAPVCC, 0x3C); //Start SSD1306 oled display
    oled.setTextColor(WHITE);
    oled.setRotation(0);     //set rotation 0=0, 1=90, 2=180, 3=270
    oled.setFont(COMICS_8);   //Arial Bold 16pt
    oled.setTextSize(1);
    oled.clearDisplay();
    oled.display();

    numCreds = WiFi.getCredentials(ap, 5);
    for(byte i = 0; i < numCreds; i++){
      String credList = ap[i].ssid;
      if (credList.equals("Home")){
        cellIsSet = 1;
      }
    }
    setCellCreds();
    Particle.connect();

    ThingSpeak.begin(client);

    ds18b20.resetsearch();                 // initialise for sensor search
    for (int i = 0; i < nSENSORS; i++) {   // try to read the sensor addresses
        ds18b20.search(sensorAddresses[i]); // and if available store
    }

}

void loop() {

  if (!Particle.connected()) Particle.connect();

    if (millis() - tempLastMillis >= 500) {
        for (int i = 0; i < nSENSORS; i++) {
            float temp = getTemp(sensorAddresses[i]);
            if (!isnan(temp)) tempSensor[i] = temp;
        }

        if (!aquireDHTStarted) {
	        DHT.acquire();
	        aquireDHTStarted = true;
	       }

        if (!DHT.acquiring()) {
            int result = DHT.getStatus();
            dht22Temp = DHT.getFahrenheit();
            dht22Humd = DHT.getHumidity();
            //Serial.print(" T: "); Serial.println(dht22Temp);
            //Serial.print(" H: "); Serial.print(dht22Humd);
            if (dht22Temp > 0) prevDht22Temp = dht22Temp; //prevent publishing negative error values
            else dht22Temp = prevDht22Temp;

            if (dht22Temp > 80) turnExhuastFanON = true;
            else turnExhuastFanON = false;

            if (dht22Humd > 0) prevDht22Humd = dht22Humd;
            else dht22Humd = prevDht22Humd;

            aquireDHTStarted = false;
        }

        oled.clearDisplay();
        oled.setCursor(0,0);
        oled.print("T1: "); oled.println(tempSensor[0],1);
        oled.print("T2: "); oled.println(tempSensor[1],1);
        oled.print("T/H: "); oled.print(dht22Temp,1); oled.print(":"); oled.println(dht22Humd,1);
        oled.display();

        tempLastMillis = millis();
    }

    if (turnExhuastFanON && (millis() - prevFanTurnOnMillis > 60000L)) {
      digitalWrite(FANCONTROLPIN, HIGH);
      prevFanTurnOnMillis = millis();
    } else digitalWrite(FANCONTROLPIN, LOW);

    if (millis() - publishLastMillis >= 180000L && Particle.connected()) {
        ThingSpeak.setField(1,tempSensor[0]);
        ThingSpeak.setField(2,tempSensor[1]);
        ThingSpeak.setField(3,dht22Temp);
        ThingSpeak.setField(4,dht22Humd);
        //ThingSpeak.setField(3,tempSensor[2]);
        ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

        publishLastMillis = millis();
    }
}

void setCellCreds() {
  if (cellIsSet == 0) {
    WiFi.on();
    delay(500);
    WiFi.setCredentials("Home", "we3sner9", WPA2, WLAN_CIPHER_AES);
    numCreds = WiFi.getCredentials(ap, 5);
    if(WiFi.listening()){
      WiFi.listen(false);
    }
    cellIsSet = 1;
  }
}

double getTemp(uint8_t addr[8]) {
  double _temp;
  int   i = 0;

  do {
    SINGLE_THREADED_BLOCK() {
      _temp = ds18b20.getTemperature(addr);
    }
  } while (!ds18b20.crcCheck() && MAXRETRY > i++);

  if (i < MAXRETRY) {
    _temp = ds18b20.convertToFahrenheit(_temp);
    //Serial.println(_temp);
  }
  else {
    _temp = NAN;
    Serial.println("Invalid reading");
  }

  return _temp;
}
