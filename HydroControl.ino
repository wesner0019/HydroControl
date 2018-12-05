#include "application.h"
#include "ThingSpeak.h"
#include "DS18B20.h"
#include "fonts.h"
#include "Adafruit_mfGFX.h"
#include "Adafruit_SSD1306.h"

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);
//Temperature Sensors
uint32_t tempLastMillis = 0;
const int MAXRETRY = 3;
const int pinLED = D7;
const int nSENSORS = 3;
float tempSensor[nSENSORS] = {NAN, NAN, NAN};
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
    oled.begin(SSD1306_SWITCHCAPVCC, 0x3C); //Start SSD1306 oled display
    oled.setTextColor(WHITE);
    oled.setRotation(0);     //set rotation 0=0, 1=90, 2=180, 3=270
    oled.setFont(COMICS_8);   //Arial Bold 16pt
    oled.setTextSize(1);
    oled.clearDisplay();
    oled.display();
    pinMode(pinLED, OUTPUT);

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

    if (millis() - tempLastMillis >= 500) {
        for (int i = 0; i < nSENSORS; i++) {
            float temp = getTemp(sensorAddresses[i]);
            if (!isnan(temp)) tempSensor[i] = temp;
        }

        oled.clearDisplay();
        oled.setCursor(0,0);
        oled.print("T1:  "); oled.println(tempSensor[0],1);
        oled.print("T2:  "); oled.println(tempSensor[1],1);
        oled.print("T3:  "); oled.println(tempSensor[2],1);
        oled.display();

        tempLastMillis = millis();
    }

    if (millis() - publishLastMillis >= 180000L && Particle.connected()) {
        ThingSpeak.setField(1,tempSensor[0]);
        ThingSpeak.setField(2,tempSensor[1]);
        ThingSpeak.setField(3,tempSensor[2]);
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
