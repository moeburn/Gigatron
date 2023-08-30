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
#include <FastLED.h>

#define LED_PIN 18 //d7
#define NUM_LEDS 1
CRGB leds[NUM_LEDS];

#define pinSSR 15
#define pinDHT 19
bool ssrState = false;
float hysteresis = 1.1;

#define pushbutton 16 // Switch connection if available
const int DEBOUNCE_DELAY = 50;   // the debounce time; increase if the output flickers
// Variables will change:
int lastSteadyState = LOW;       // the previous steady state from the input pin
int lastFlickerableState = LOW;  // the previous flickerable state from the input pin
int currentState;                // the current reading from the input pin
bool buttonstate = false;
// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled

enum { PinA=5, PinB=17, IPINMODE=INPUT };

static  byte abOld;     // Initialize state
volatile int count = 200;     // current rotary count
         int old_count;     // old rotary count
         int halfcount;


DHT dht (pinDHT, DHT22);
float dhtTemp, dhtHum, temperatureC, absHum;
float setTemp = 21.0;
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
int zebraR, zebraG, zebraB, sliderValue;

BLYNK_WRITE(V16)
{
     zebraR = param[0].asInt();
     zebraG = param[1].asInt();
     zebraB = param[2].asInt();
}

void setup() {
FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  pinMode(PinA, IPINMODE);
  pinMode(PinB, IPINMODE);
  pinMode(pushbutton, INPUT_PULLUP);
  pinMode(pinSSR, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(PinA), pinChangeISR, CHANGE); // Set up pin-change interrupts
  attachInterrupt(digitalPinToInterrupt(PinB), pinChangeISR, CHANGE);
  abOld = count = old_count = 0;

  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
  display.init();
    display.setFont(Monospaced_plain_8);
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
  display.setFont(Monospaced_bold_16);
  //analogReadResolution(11); // Default of 12 is not very linear. Recommended to use 10 or 11 depending on needed resolution.
//analogSetAttenuation(ADC_6db); 
dht.begin ();
      dhtTemp = dht.readTemperature();
     
      dhtHum = dht.readHumidity();
      sensors.requestTemperatures(); 
        temperatureC = sensors.getTempCByIndex(0);
}

unsigned long millisBlynk, millisTemp;

void doDisplay (){

      char t1buff[150];
      CStringBuilder sbt1(t1buff, sizeof(t1buff));
      sbt1.print(dhtTemp, 1);
      sbt1.print("°C");

      char t2buff[150];
      CStringBuilder sbt2(t2buff, sizeof(t2buff));
      sbt2.print(setTemp, 1);
      sbt2.print("°C");

      /*char h1buff[150];
      CStringBuilder sbh1(h1buff, sizeof(h1buff));
      sbh1.print("H1: ");
      sbh1.print(dhtHum);
      sbh1.print(" %");*/
          // write the buffer to the display
      display.clear();
      if (buttonstate) {
      display.drawCircle(60,3,3);
      } 
      //display.setTextAlignment(TEXT_ALIGN_LEFT);
      //display.drawString(0, 0, "T:");
     // display.drawString(0, 10, "Set: ");
     display.setFont(Monospaced_bold_16);
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.drawString(32, 0, t1buff);
      display.drawString(32, 24, t2buff);
      display.setFont(ArialMT_Plain_10);
      display.drawString(6, 38, "^");
      //display.drawString(0, 20, h1buff);
      
      //display.drawString(0, 30, String(halfcount));
      //display.setBrightness(halfcount);
      display.display();
}

void loop() {
//leds[0] = CRGB(zebraR, zebraG, zebraB);
  halfcount = count / 2;
      
  doDisplay();
  if (dhtTemp < setTemp) {ssrState = true;
  digitalWrite(pinSSR, HIGH);
  leds[0] = CRGB(100, 0, 0);
  }
  if (dhtTemp > (setTemp + hysteresis)) {ssrState = false;
  digitalWrite(pinSSR, LOW);
  leds[0] = CRGB(0, 0, 0);
  }



  FastLED.show();

  Blynk.run();

  currentState = digitalRead(pushbutton);
  // If the switch/button changed, due to noise or pressing:
  if (currentState != lastFlickerableState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
    // save the the last flickerable state
    lastFlickerableState = currentState;
  }
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (lastSteadyState == HIGH && currentState == LOW){
      count = 0;
      buttonstate = true;}
    else if (lastSteadyState == LOW && currentState == HIGH){
      buttonstate = false;}

    // save the the last steady state
    lastSteadyState = currentState;
  }
    
  if  (millis() - millisTemp >= 5000)  //if it's been 30 seconds 
    {
      millisTemp = millis();
      dhtTemp = dht.readTemperature();
     
      dhtHum = dht.readHumidity();
      sensors.requestTemperatures(); 
        temperatureC = sensors.getTempCByIndex(0);

    }

  if  (millis() - millisBlynk >= 30000)  //if it's been 30 seconds 
    {
        millisBlynk = millis();
      dhtTemp = dht.readTemperature();
      
      dhtHum = dht.readHumidity();
      sensors.requestTemperatures(); 
        temperatureC = sensors.getTempCByIndex(0);
        float tempAvgHolder, humAvgHolder;
        tempAvgHolder = dhtTemp;
        humAvgHolder = dhtHum;
        absHum = (6.112 * pow(2.71828, ((17.67 * tempAvgHolder) / (tempAvgHolder + 243.5))) * humAvgHolder * 2.1674) / (273.15 + tempAvgHolder);
              if ((dhtTemp > 0) && (dhtHum > 0)){
        Blynk.virtualWrite(V1, tempAvgHolder);
        
        Blynk.virtualWrite(V3, humAvgHolder);
        Blynk.virtualWrite(V4, absHum);
              }
              
              Blynk.virtualWrite(V2, temperatureC);
              Blynk.virtualWrite(V5, setTemp);
              Blynk.virtualWrite(V6, ssrState);
    }
      
}

void pinChangeISR() {
  static int icount;
  enum { upMask = 0x66, downMask = 0x99 };
  byte abNew = (digitalRead(PinA) << 1) | digitalRead(PinB);
  byte criterion = abNew^abOld;
  if (criterion==1 || criterion==2) {
    if (upMask & (1 << (2*abOld + abNew/2))){
      count++;
      setTemp += 0.05;}
    else {
      count--;       // upMask = ~downMask
      setTemp -= 0.05;}
  }
  abOld = abNew;        // Save new state
}
