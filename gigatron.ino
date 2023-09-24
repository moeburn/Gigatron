#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <Wire.h>         // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h"  // legacy: #include "SSD1306.h"
#include "images.h"
#include "OLEDDisplayUi.h"
#include <TimeLib.h>
#include <StreamLib.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <BlynkSimpleEsp32.h>
#include <Average.h>
//#include "DHT.h"
#include <FastLED.h>
#include "Adafruit_SHT31.h"
#include "time.h"

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -14400;  //Replace with your GMT offset (secs)
const int daylightOffset_sec = 0;   //Replace with your daylight offset (secs)
int hours, mins, secs;

int ledValue;
bool haschanged = false;
bool haschanged2 = false;



Adafruit_SHT31 sht31 = Adafruit_SHT31();

#define LED_PIN 18  //d7
#define NUM_LEDS 1
CRGB leds[NUM_LEDS];

#define pinSSR 15
//#define pinDHT 19
bool ssrState = false;
float hysteresis = 1.1;
bool partymode = false;
#define pushbutton 16           // Switch connection if available
const int DEBOUNCE_DELAY = 50;  // the debounce time; increase if the output flickers
// Variables will change:
int lastSteadyState = LOW;       // the previous steady state from the input pin
int lastFlickerableState = LOW;  // the previous flickerable state from the input pin
int currentState;                // the current reading from the input pin
bool buttonstate = false;
// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled

enum { PinA = 5,
       PinB = 17,
       IPINMODE = INPUT };

static byte abOld;         // Initialize state
volatile int count = 200;  // current rotary count
int old_count;             // old rotary count
int halfcount;


//DHT dht (pinDHT, DHT22);
float dhtTemp, dhtHum, shtTemp, shtHum, temperatureC, absHum;
float setTemp = 20.0;
int encoder0Pos;
float tempOffset = -1.5;

Average<float> t1avg(30);
Average<float> t2avg(30);
Average<float> h1avg(30);

char auth[] = "xzOx2JrFeyFk7ea6zAZmHCzqBRC_ciuH";  //BLYNK

const char* ssid = "mikesnet";
const char* password = "springchicken";

const int oneWireBus = 33;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

AsyncWebServer server(80);

SSD1306Wire display(0x3c, SDA, SCL, GEOMETRY_64_48);

int ThermistorPin = 36;
int Vo;
float R1 = 10000;  // value of R1 on board
float logR2, R2, T;
float c1 = 0.001129148, c2 = 0.000234125, c3 = 0.0000000876741;

uint8_t startHue = 0;
uint8_t deltaHue = 0;

void rainbow2() {
  for (int i = 0; i <= 255; i++) {
    fill_rainbow(leds, NUM_LEDS, startHue, deltaHue);
    FastLED.show();
    delay(10);
    startHue++;
    deltaHue++;
  }
  leds[0] = CRGB(0, 0, 0);
  FastLED.show();
}

void rainbow_wave(uint8_t thisSpeed, uint8_t deltaHue) {  // The fill_rainbow call doesn't support brightness levels.

  // uint8_t thisHue = beatsin8(thisSpeed,0,255);                // A simple rainbow wave.
  uint8_t thisHue = beat8(thisSpeed, 255);  // A simple rainbow march.

  fill_rainbow(leds, NUM_LEDS, thisHue, deltaHue);  // Use FastLED's fill_rainbow routine.

}  // rainbow_wave()

int zebraR, zebraG, zebraB, sliderValue;

WidgetTerminal terminal(V0);
//WidgetLED led1(V41);

BLYNK_WRITE(V0) {
  if (String("help") == param.asStr()) {
    terminal.println("==List of available commands:==");
    terminal.println("wifi");
    terminal.println("blink");
    terminal.println("temp");
    terminal.println("==End of list.==");
  }
  if (String("wifi") == param.asStr()) {
    terminal.print("Connected to: ");
    terminal.println(ssid);
    terminal.print("IP address:");
    terminal.println(WiFi.localIP());
    terminal.print("Signal strength: ");
    terminal.println(WiFi.RSSI());
    printLocalTime();
  }

  if (String("blink") == param.asStr()) {
    terminal.println("Blinking...");
    partymode = true;
    rainbow2();
  }

  if (String("temp") == param.asStr()) {
    shtTemp = sht31.readTemperature();
    shtHum = sht31.readHumidity();
    shtTemp += tempOffset;
    sensors.requestTemperatures();
    temperatureC = sensors.getTempCByIndex(0);
    terminal.print("SHTTemp: ");
    terminal.print(shtTemp);
    terminal.print(", SHTHum: ");
    terminal.print(shtHum);
    terminal.print("ProbeTemp: ");
    terminal.print(temperatureC);
  }




  terminal.flush();
}

BLYNK_WRITE(V40) {
  float pinValue = param.asFloat();  // assigning incoming value from pin V1 to a variable


  setTemp = pinValue;
  Blynk.virtualWrite(V5, setTemp);
  Blynk.virtualWrite(V41, ledValue);
haschanged2 = true;
}





BLYNK_WRITE(V16) {
  zebraR = param[0].asInt();
  zebraG = param[1].asInt();
  zebraB = param[2].asInt();
}

void printLocalTime() {
  time_t rawtime;
  struct tm* timeinfo;
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  terminal.println(asctime(timeinfo));
  terminal.flush();
}

void setup() {
  setCpuFrequencyMhz(80);
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  sht31.begin(0x44);
  pinMode(PinA, IPINMODE);
  pinMode(PinB, IPINMODE);
  pinMode(pushbutton, INPUT_PULLUP);
  pinMode(pinSSR, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(PinA), pinChangeISR, CHANGE);  // Set up pin-change interrupts
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


  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/plain", "Hi! I am ESP32 Gigatrooon!");
  });

  AsyncElegantOTA.begin(&server);  // Start ElegantOTA
  server.begin();
  Serial.println("HTTP server sharted");
  delay(2000);
  display.clear();

  //analogReadResolution(11); // Default of 12 is not very linear. Recommended to use 10 or 11 depending on needed resolution.
  //analogSetAttenuation(ADC_6db);
  //dht.begin ();
  //dhtTemp = dht.readTemperature();

  // dhtHum = dht.readHumidity();
  sensors.requestTemperatures();
  temperatureC = sensors.getTempCByIndex(0);
  shtTemp = sht31.readTemperature();
  shtHum = sht31.readHumidity();
  shtTemp += tempOffset;
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  hours = timeinfo.tm_hour;
  mins = timeinfo.tm_min;
  secs = timeinfo.tm_sec;
  terminal.println("**********Gigatron/Goliath/");
  terminal.println("Smart Thermostat v1.0***********");

  terminal.print("Connected to ");
  terminal.println(ssid);
  terminal.print("IP address: ");
  terminal.println(WiFi.localIP());

  printLocalTime();
  terminal.flush();
  Blynk.virtualWrite(V40, setTemp);
}

unsigned long millisBlynk, millisTemp;
int buttoncounter;

void doDisplay() {

  char t1buff[150];
  CStringBuilder sbt1(t1buff, sizeof(t1buff));
  sbt1.print(shtTemp, 1);
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
    display.drawCircle(60, 3, 3);
    millisTemp = millis();
    buttoncounter++;
  }
  //display.setTextAlignment(TEXT_ALIGN_LEFT);
  //display.drawString(0, 0, "T:");
  // display.drawString(0, 10, "Set: ");
  display.setFont(SansSerif_bold_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(32, 0, t1buff);
  display.drawString(32, 24, t2buff);
  display.setFont(ArialMT_Plain_10);
  display.drawString(6, 38, "^");
  //display.drawString(0, 20, h1buff);
  
  //display.drawRect(0, 0, 64, 48);
  //display.drawString(0, 30, String(halfcount));
  //display.setBrightness(halfcount);
  display.display();
}



void loop() {
  //leds[0] = CRGB(zebraR, zebraG, zebraB);
  halfcount = count / 2;

  doDisplay();
  if (buttoncounter >= 9) {
    partymode = true;
    rainbow2();
  }

  //###############################################################################################
  //###############################################################################################
  //##############################  MAIN THERMOSTAT CODE IS HERE  #################################
  if ((shtTemp < setTemp) && (shtTemp > 0)) {
    ssrState = true;
    digitalWrite(pinSSR, HIGH);
    leds[0] = CRGB(100, 0, 0);
    ledValue = 255;
    //led1.on();
  }
  if (shtTemp > (setTemp + hysteresis)) {
    ssrState = false;
    digitalWrite(pinSSR, LOW);
    if (!partymode) { leds[0] = CRGB(0, 0, 0); }
    ledValue = 0;
    //led1.off();
  }
  //###############################################################################################
  //###############################################################################################
  //###############################################################################################

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
    if (lastSteadyState == HIGH && currentState == LOW) {
      count = 0;
      buttonstate = true;
    } else if (lastSteadyState == LOW && currentState == HIGH) {
      buttonstate = false;
    }

    // save the the last steady state
    lastSteadyState = currentState;
  }



  if (millis() - millisTemp >= 10000)  //if it's been 30 seconds
  {
    millisTemp = millis();
    shtTemp = sht31.readTemperature();
    shtHum = sht31.readHumidity();
    shtTemp += tempOffset;
    buttoncounter = 0;
    partymode = false;
      if (haschanged) {
    Blynk.virtualWrite(V40, setTemp);
    Blynk.virtualWrite(V5, setTemp);
    Blynk.virtualWrite(V41, ledValue);
      terminal.print("> Knob fiddled to ");
  terminal.print(setTemp);
  terminal.print(" at ");
  printLocalTime();
  terminal.flush();
    haschanged = false;
       }

       if (haschanged2) {
  terminal.print("> Temp set to ");
  terminal.print(setTemp);
  terminal.print(" at ");
  printLocalTime();
  terminal.flush();
  haschanged2 = false;
       }
  }

  if (millis() - millisBlynk >= 30000)  //if it's been 30 seconds
  {
    millisBlynk = millis();
    sensors.requestTemperatures();
    temperatureC = sensors.getTempCByIndex(0);
    shtTemp = sht31.readTemperature();
    shtHum = sht31.readHumidity();
    absHum = (6.112 * pow(2.71828, ((17.67 * shtTemp) / (shtTemp + 243.5))) * shtHum * 2.1674) / (273.15 + shtTemp);
    shtTemp += tempOffset;
    if ((shtTemp > 0) && (shtHum > 0)) {
      //Blynk.virtualWrite(V1, dhtTemp);
      //Blynk.virtualWrite(V3, humAvgHolder);
      Blynk.virtualWrite(V7, shtTemp);
      Blynk.virtualWrite(V8, shtHum);
      Blynk.virtualWrite(V4, absHum);
    }

    Blynk.virtualWrite(V2, temperatureC);
    Blynk.virtualWrite(V5, setTemp);
    Blynk.virtualWrite(V6, ssrState);
        Blynk.virtualWrite(V40, setTemp);
    Blynk.virtualWrite(V41, ledValue);
  }
}



void pinChangeISR() {
  static int icount;
  enum { upMask = 0x66,
         downMask = 0x99 };
  byte abNew = (digitalRead(PinA) << 1) | digitalRead(PinB);
  byte criterion = abNew ^ abOld;
  if (criterion == 1 || criterion == 2) {
    if (upMask & (1 << (2 * abOld + abNew / 2))) {
      count++;
      setTemp += 0.05;
    } else {
      count--;  // upMask = ~downMask
      setTemp -= 0.05;
    }
  }
  abOld = abNew;  // Save new state
  haschanged = true;
}
