#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <Wire.h>               // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h"        // legacy: #include "SSD1306.h"
#include "images.h"
#include "OLEDDisplayUi.h"
#include <TimeLib.h>
#include <StreamLib.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <BlynkSimpleEsp32.h>
#include <Average.h>
#include "DHT.h"
// setting the number of the connection pin
#define pinDHT 19

#define switch 16 // Switch connection if available

enum { PinA=5, PinB=17, IPINMODE=INPUT };

static  byte abOld;     // Initialize state
volatile int count = 200;     // current rotary count
         int old_count;     // old rotary count
         int halfcount;


DHT dht (pinDHT, DHT22);
float dhtTemp, dhtHum, temperatureC, absHum;
int encoder0Pos;
int tempOffset = 0.5;

Average<float> t1avg(30);
Average<float> t2avg(30);
Average<float> h1avg(30);

char auth[] = "xzOx2JrFeyFk7ea6zAZmHCzqBRC_ciuH"; //BLYNK

const char* ssid = "mikesnet";
const char* password = "springchicken";

const int oneWireBus = 33; 
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

AsyncWebServer server(80);

SSD1306Wire display(0x3c, SDA, SCL, GEOMETRY_64_48 );

int ThermistorPin = 36;
int Vo;
float R1 = 10000; // value of R1 on board
float logR2, R2, T;
float c1 = 0.001129148, c2 = 0.000234125, c3 = 0.0000000876741; 

void setup() {
  pinMode(PinA, IPINMODE);
  pinMode(PinB, IPINMODE);
  attachInterrupt(digitalPinToInterrupt(PinA), pinChangeISR, CHANGE); // Set up pin-change interrupts
  attachInterrupt(digitalPinToInterrupt(PinB), pinChangeISR, CHANGE);
  abOld = count = old_count = 0;

  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
  display.init();
    display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawStringMaxWidth(0, 0, 64, "Connectong to wifi...");
  display.display();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  display.clear();
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
        Blynk.config(auth, IPAddress(192, 168, 50, 197), 8080);
    Blynk.connect();
    sensors.begin();
  sensors.requestTemperatures(); 
  char buff[150];
 CStringBuilder sb(buff, sizeof(buff));

 sb.print(ssid);
 sb.print(", ");
 sb.print(WiFi.localIP());


  display.drawStringMaxWidth(0, 0, 64, buff);
  display.display();
  

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! I am ESP32 Gigatrooon!");
  });

  AsyncElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
  Serial.println("HTTP server sharted");
  delay(2000);
  display.clear();
  //analogReadResolution(11); // Default of 12 is not very linear. Recommended to use 10 or 11 depending on needed resolution.
//analogSetAttenuation(ADC_6db); 
dht.begin ();
}

unsigned long millisBlynk, millisTemp;

void loop() {

  Blynk.run();
    
  if  (millis() - millisTemp >= 1000)  //if it's been 30 seconds 
    {
      millisTemp = millis();
      dhtTemp = dht.readTemperature();
      dhtTemp += tempOffset;
      dhtHum = dht.readHumidity();
      sensors.requestTemperatures(); 
        temperatureC = sensors.getTempCByIndex(0);

      




      if ((dhtTemp > 0) && (dhtHum > 0)){
      t1avg.push(dhtTemp);
      h1avg.push(dhtHum);
      }
      t2avg.push(temperatureC);


    }

  if  (millis() - millisBlynk >= 30000)  //if it's been 30 seconds 
    {
        millisBlynk = millis();
        float tempAvgHolder, humAvgHolder;
        tempAvgHolder = t1avg.mean();
        humAvgHolder = h1avg.mean();
        absHum = (6.112 * pow(2.71828, ((17.67 * tempAvgHolder) / (tempAvgHolder + 243.5))) * humAvgHolder * 2.1674) / (273.15 + tempAvgHolder);
        Blynk.virtualWrite(V1, tempAvgHolder);
        Blynk.virtualWrite(V2, t2avg.mean());
        Blynk.virtualWrite(V3, humAvgHolder);
        Blynk.virtualWrite(V4, absHum);
    }

      char t1buff[150];
      CStringBuilder sbt1(t1buff, sizeof(t1buff));
      sbt1.print("T1: ");
      sbt1.print(dhtTemp);
      sbt1.print(" *C");

      char t2buff[150];
      CStringBuilder sbt2(t2buff, sizeof(t2buff));
      sbt2.print("T2: ");
      sbt2.print(temperatureC);
      sbt2.print(" *C");

      char h1buff[150];
      CStringBuilder sbh1(h1buff, sizeof(h1buff));
      sbh1.print("H1: ");
      sbh1.print(dhtHum);
      sbh1.print(" %");
          // write the buffer to the display
            display.clear();
      display.drawString(0, 0, t1buff);
      display.drawString(0, 10, t2buff);
      display.drawString(0, 20, h1buff);
      halfcount = count / 2;
      display.drawString(0, 30, String(halfcount));
      display.setBrightness(halfcount);
      display.display();
}

void pinChangeISR() {
  static int icount;
  enum { upMask = 0x66, downMask = 0x99 };
  byte abNew = (digitalRead(PinA) << 1) | digitalRead(PinB);
  byte criterion = abNew^abOld;
  if (criterion==1 || criterion==2) {
    if (upMask & (1 << (2*abOld + abNew/2)))
      count++;
    else count--;       // upMask = ~downMask
  }
  abOld = abNew;        // Save new state
}
