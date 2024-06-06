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
#include <NonBlockingDallas.h>                  //Include the NonBlockingDallas library

#define ONE_WIRE_BUS 33                          //PIN of the Maxim DS18B20 temperature sensor
#define TIME_INTERVAL 15000      
#include <BlynkSimpleEsp32.h>
#include <Average.h>
//#include "DHT.h"
#include <FastLED.h>
#include "Adafruit_SHT31.h"
#include "time.h"
#include <ESP32Time.h>
#include <Preferences.h>

Preferences preferences;

//ESP32Time rtc;
ESP32Time rtc(0);  // offset in seconds, use 0 because NTP already offset

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -18000;  //Replace with your GMT offset (secs)
const int daylightOffset_sec = 3600;   //Replace with your daylight offset (secs)
int hours, mins, secs;
int chours, cmins, shours, smins, whours, wmins, whoursdouble, wminsdouble, shoursdouble, sminsdouble;
bool isAwake = true;
bool inverted = true;
int page = 1;

int ledValue;
bool haschanged = false;
bool haschanged2 = false;
bool timechanged = false;



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
float waketemp = 21.3;
float sleeptemp = 19.0;
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
DallasTemperature dallasTemp(&oneWire);
NonBlockingDallas sensorDs18b20(&dallasTemp); 

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
    terminal.println("invert");
    terminal.println("reset");
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
    terminal.print("SHTTemp: ");
    terminal.print(shtTemp);
    terminal.print(", SHTHum: ");
    terminal.print(shtHum);
    terminal.print("ProbeTemp: ");
    terminal.print(temperatureC);
  }
  if (String("invert") == param.asStr()) {
    display.normalDisplay();
    delay(1000);
    display.invertDisplay();
    delay(1000);
    display.normalDisplay();
    delay(1000);
    display.invertDisplay();
    delay(1000);
    display.normalDisplay();
    delay(1000);
    display.invertDisplay();
    delay(1000);
    display.normalDisplay();
    delay(1000);
    display.invertDisplay();
  }
  if (String("reset") == param.asStr()) {
    terminal.println("Restarting...");
    terminal.flush();
    ESP.restart();
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

void handleTemperatureChange(int deviceIndex, int32_t temperatureRAW){
  temperatureC = sensorDs18b20.rawToCelsius(temperatureRAW);
}

void handleIntervalElapsed(int deviceIndex, int32_t temperatureRAW)
{
  temperatureC = sensorDs18b20.rawToCelsius(temperatureRAW);
}

void handleDeviceDisconnected(int deviceIndex)
{
  terminal.print(F("[NonBlockingDallas] handleDeviceDisconnected ==> deviceIndex="));
  terminal.print(deviceIndex);
  terminal.println(F(" disconnected."));
  printLocalTime();
  terminal.flush();
}

void setup() {
  whours = 5;
  shours = 22;
  
  pinMode(19, INPUT_PULLUP);  //5, 15, 16, 17, 18 in use, disable rest
  pinMode(14, INPUT_PULLUP);
  pinMode(13, INPUT_PULLUP);
  pinMode(12, INPUT_PULLUP);
  for(int i=1; i<=4; i++) {
    pinMode(i, INPUT_PULLUP);
  }
  pinMode(21, INPUT_PULLUP);
  pinMode(22, INPUT_PULLUP);
  pinMode(23, INPUT_PULLUP);
  pinMode(25, INPUT_PULLUP);
  pinMode(26, INPUT_PULLUP);
  pinMode(27, INPUT_PULLUP);
  for(int i=32; i<=36; i++) {
    pinMode(i, INPUT_PULLUP);
  }
  pinMode(39, INPUT_PULLUP);

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
  display.setBrightness(100);
  display.drawStringMaxWidth(0, 0, 64, "Connecting...");
  display.display();
  while (WiFi.status() != WL_CONNECTED && millis() < 15000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED && millis() >= 15000) {
    WiFi.disconnect();
  }
  preferences.begin("my-app", false);
  whours = preferences.getInt("whours", 0);
  wmins  = preferences.getInt("wmins", 0);
  shours = preferences.getInt("shours", 0);
  smins  = preferences.getInt("smins", 0);
  waketemp = preferences.getFloat("waketemp", 0);
  sleeptemp  = preferences.getFloat("sleeptemp", 0);
  preferences.end();
  whoursdouble = whours * 2;
  wminsdouble = wmins * 2;
  shoursdouble = shours * 2;
  sminsdouble = smins * 2;
  


  display.clear();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Blynk.config(auth, IPAddress(192, 168, 50, 197), 8080);
    Blynk.connect();
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
    delay(250);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    delay(250);
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    hours = timeinfo.tm_hour;
    mins = timeinfo.tm_min;
    secs = timeinfo.tm_sec;
    if (hours > whours) {
      isAwake = true;
      setTemp = waketemp;
    }
    else {
      isAwake = false;
      setTemp = sleeptemp;    
    }
    terminal.println("**********Gigatron/Goliath/");
    terminal.println("Smart Thermostat v1.3***********");

    terminal.print("Connected to ");
    terminal.println(ssid);
    terminal.print("IP address: ");
    terminal.println(WiFi.localIP());
    String comma = ", ";
    printLocalTime();
    terminal.println("Loaded values whours, wmins, shours, smins, waketemp, sleeptemp:");
    terminal.println(whours + comma + wmins + comma + shours + comma + smins + comma + waketemp + comma + sleeptemp);  
    terminal.flush();
    Blynk.virtualWrite(V40, setTemp);
  }
  else {
    display.drawStringMaxWidth(0, 0, 64, "ERR: NO WIFI.");
    display.display();  
  }


  delay(1000);
  display.clear();
  display.invertDisplay();
  sensorDs18b20.begin(NonBlockingDallas::resolution_12, TIME_INTERVAL);
  sensorDs18b20.onTemperatureChange(handleTemperatureChange);
  sensorDs18b20.onIntervalElapsed(handleIntervalElapsed);
  sensorDs18b20.onDeviceDisconnected(handleDeviceDisconnected);
  shtTemp = sht31.readTemperature();
  shtHum = sht31.readHumidity();
  shtTemp += tempOffset;
}

unsigned long millisBlynk, millisTemp, millisPage;
bool onwrongpage = false;
int buttoncounter;

void page1() {

  char t1buff[150];
  CStringBuilder sbt1(t1buff, sizeof(t1buff));
  sbt1.print(shtTemp, 1);
  sbt1.print("°C");

  char t2buff[150];
  CStringBuilder sbt2(t2buff, sizeof(t2buff));
  sbt2.print(setTemp, 1);
  sbt2.print("°C");

  display.clear();

  display.setFont(SansSerif_bold_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(32, 4, t1buff);
  display.drawString(32, 25, t2buff);
  display.setFont(ArialMT_Plain_10);
  display.drawString(6, 39, "^");
  display.display();
}

void page2() {
  display.clear();
  display.setFont(Monospaced_plain_8);
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  char tbuff[150];
  CStringBuilder sbt1(tbuff, sizeof(tbuff));
  if (WiFi.status() == WL_CONNECTED) {
  display.drawString(1,1, "WiFi Time:");
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    hours = timeinfo.tm_hour;
    mins = timeinfo.tm_min;
    secs = timeinfo.tm_sec;
    sbt1.print(hours, 1);
    sbt1.print(":");
    if (mins<10){
    sbt1.print("0");  
    }
    sbt1.print(mins, 1);
  }
  else {
  display.drawString(1,1, "Set hour:");
    sbt1.print(hours, 1);
    sbt1.print(":");
    if (mins<10){
    sbt1.print("0");  
    }
    sbt1.print(mins, 1);
  display.setFont(SansSerif_bold_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(16, 29, "^");
  }
  display.setFont(SansSerif_bold_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(32, 12, tbuff);
  display.display();
}

void page3() {
  display.clear();
  display.setFont(Monospaced_plain_8);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(1,1, "Set minute:");
  char tbuff[150];
  CStringBuilder sbt1(tbuff, sizeof(tbuff));
  if (WiFi.status() == WL_CONNECTED) {
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    hours = timeinfo.tm_hour;
    mins = timeinfo.tm_min;
    secs = timeinfo.tm_sec;
    sbt1.print(hours, 1);
    sbt1.print(":");
    if (mins<10){
    sbt1.print("0");  
    }
    sbt1.print(mins, 1);
  }
  else {
    sbt1.print(hours, 1);
    sbt1.print(":");
    if (mins<10){
    sbt1.print("0");  
    }
    sbt1.print(mins, 1);
  }
  display.setFont(SansSerif_bold_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(32, 12, tbuff);
  display.drawString(48, 29, "^");
  display.display();
}

void page4() {
  display.clear();
  display.setFont(Monospaced_plain_8);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(1,1, "Wake hour:");
  char tbuff[150];
  CStringBuilder sbt1(tbuff, sizeof(tbuff));
  sbt1.print(whours, 1);
  sbt1.print(":");
    if (wmins<10){
    sbt1.print("0");  
    }
  sbt1.print(wmins, 1);
  display.setFont(SansSerif_bold_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(32, 12, tbuff);
  display.drawString(16, 29, "^");
  display.display();
}

void page5() {
  display.clear();
  display.setFont(Monospaced_plain_8);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(1,1, "Wake min:");
  char tbuff[150];
  CStringBuilder sbt1(tbuff, sizeof(tbuff));
  sbt1.print(whours, 1);
  sbt1.print(":");
    if (wmins<10){
    sbt1.print("0");  
    }
  sbt1.print(wmins, 1);
  display.setFont(SansSerif_bold_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(32, 12, tbuff);
  display.drawString(48, 29, "^");
  display.display();
}

void page6() {
  display.clear();
  display.setFont(Monospaced_plain_8);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(1,1, "Sleep hour:");
  char tbuff[150];
  CStringBuilder sbt1(tbuff, sizeof(tbuff));
  sbt1.print(shours, 1);
  sbt1.print(":");
    if (smins<10){
    sbt1.print("0");  
    }
  sbt1.print(smins, 1);
  display.setFont(SansSerif_bold_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(32, 12, tbuff);
  display.drawString(16, 29, "^");
  display.display();
}

void page7() {
  display.clear();
  display.setFont(Monospaced_plain_8);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(1,1, "Sleep min:");
  char tbuff[150];
  CStringBuilder sbt1(tbuff, sizeof(tbuff));
  sbt1.print(shours, 1);
  sbt1.print(":");
    if (smins<10){
    sbt1.print("0");  
    }
  sbt1.print(smins, 1);
  display.setFont(SansSerif_bold_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(32, 12, tbuff);
  display.drawString(48, 29, "^");
  display.display();
}

void page8() {
  display.clear();
  display.setFont(Monospaced_plain_8);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(1,1, "Wake temp:");
  char tbuff[150];
  CStringBuilder sbt1(tbuff, sizeof(tbuff));
  sbt1.print(waketemp, 1);
  sbt1.print("°C");
  display.setFont(SansSerif_bold_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(32, 12, tbuff);
  display.drawString(32, 29, "^");
  display.display();
}

void page9() {
  display.clear();
  display.setFont(Monospaced_plain_8);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(1,1, "Sleep temp:");
  char tbuff[150];
  CStringBuilder sbt1(tbuff, sizeof(tbuff));
  sbt1.print(sleeptemp, 1);
  sbt1.print("°C");
  display.setFont(SansSerif_bold_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(32, 12, tbuff);
  display.drawString(32, 29, "^");
  display.display();
}



void loop() {
  sensorDs18b20.update();
  whours = whoursdouble / 2;
  wmins = wminsdouble /2;
  shours = shoursdouble /2;
  smins = sminsdouble/2;
  halfcount = count / 2;
  if (buttonstate) {
    display.drawCircle(60, 3, 3);
    display.display();
  }
  switch (page)
    {
      case 1:
        page1();
        break;
      case 2:
        page2();
        break;
      case 3:
        page3();
        break;
      case 4:
        page4();
        break;
      case 5:
        page5();
        break;
      case 6:
        page6();
        break;
      case 7:
        page7();
        break;
      case 8:
        page8();
        break;
      case 9:
        page9();
        break;
    }

if ((page != 1) && (onwrongpage = false)) {
  onwrongpage = true;
  millisPage = millis();
}
else {onwrongpage = false;}

if ((onwrongpage = true) && ((millis() - millisPage) > 10000)){
  page = 1;
  onwrongpage = false;
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
    leds[0] = CRGB(0, 0, 0); 
    ledValue = 0;
    //led1.off();
  }
  //###############################################################################################
  //###############################################################################################
  //###############################################################################################

  FastLED.show();

    if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
    else {

      }

  currentState = digitalRead(pushbutton);
  // If the switch/button changed, due to noise or pressing:
  if (currentState != lastFlickerableState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
    // save the the last flickerable state
    lastFlickerableState = currentState;
  }
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    // if the button state has changed:
    if (lastSteadyState == HIGH && currentState == LOW) {
      if ((WiFi.status() == WL_CONNECTED) && (page == 2)){page=4;}
      else {
      if (page < 9) {page++;} else {page=1;}                     //MAX PAGES GO HERE
      
      
      }
      millisPage = millis();
      buttonstate = true;
    } else if (lastSteadyState == LOW && currentState == HIGH) {
      buttonstate = false;
    }

    // save the the last steady state
    lastSteadyState = currentState;
  }



  if (millis() - millisTemp >= 10000)  //if it's been 30 seconds
  {
    if (WiFi.status() == WL_CONNECTED) {
      struct tm timeinfo;
      getLocalTime(&timeinfo);
      hours = timeinfo.tm_hour;
      mins = timeinfo.tm_min;
      secs = timeinfo.tm_sec;
    }
    else{
      hours = rtc.getHour(true);
      mins = rtc.getMinute();  
      chours = hours*2;
      cmins = mins*2;
    }
    millisTemp = millis();
    shtTemp = sht31.readTemperature();
    shtHum = sht31.readHumidity();
    shtTemp += tempOffset;
    buttoncounter = 0;
    partymode = false;
      if (haschanged) {
        if (WiFi.status() == WL_CONNECTED) {
          Blynk.virtualWrite(V40, setTemp);
          Blynk.virtualWrite(V5, setTemp);
          Blynk.virtualWrite(V41, ledValue);
          terminal.print("> Knob fiddled to ");
          terminal.print(setTemp);
          terminal.print(" at ");
          printLocalTime();
          terminal.flush();
        }
        preferences.begin("my-app", false);
        preferences.putInt("whours", whours);
        preferences.putInt("wmins", wmins);
        preferences.putInt("shours", shours);
        preferences.putInt("smins", smins);
        preferences.putFloat("waketemp", waketemp);
        preferences.putFloat("sleeptemp", sleeptemp);
        preferences.end();
        haschanged = false;
       }

       if (haschanged2) { //If new Blynk temp setting
           if (WiFi.status() == WL_CONNECTED) {
              terminal.print("> Temp set to ");
              terminal.print(setTemp);
              terminal.print(" at ");
              printLocalTime();
              terminal.flush();
              }
         haschanged2 = false;
       }
  }

       if ((timechanged) && (WiFi.status() != WL_CONNECTED)) {
          hours = chours / 2;
          mins = cmins / 2;
          rtc.setTime(0, mins, hours, 1, 1, 2023);
          timechanged = false;
       }

  if (hours == whours && mins == wmins && page == 1) {
    isAwake = true;
    setTemp = waketemp;
  }

  if (hours == shours && mins == smins && page == 1) {
    isAwake = false;
    setTemp = sleeptemp;    
  }

  if (millis() - millisBlynk >= 30000)  //if it's been 30 seconds
  {
    if (hours > 11) {display.invertDisplay();} else {display.normalDisplay();}
    millisBlynk = millis();
    shtTemp = sht31.readTemperature();
    shtHum = sht31.readHumidity();
    absHum = (6.112 * pow(2.71828, ((17.67 * shtTemp) / (shtTemp + 243.5))) * shtHum * 2.1674) / (273.15 + shtTemp);
    shtTemp += tempOffset;
      if (WiFi.status() == WL_CONNECTED) {
        if ((shtTemp > 0) && (shtHum > 0)) {
          //Blynk.virtualWrite(V1, dhtTemp);
          //Blynk.virtualWrite(V3, humAvgHolder);
          Blynk.virtualWrite(V7, shtTemp);
          Blynk.virtualWrite(V8, shtHum);
          Blynk.virtualWrite(V4, absHum);
        }

        if (temperatureC > 0) {Blynk.virtualWrite(V2, temperatureC);}
        Blynk.virtualWrite(V5, setTemp);
        Blynk.virtualWrite(V6, ssrState);
            Blynk.virtualWrite(V40, setTemp);
        Blynk.virtualWrite(V41, ledValue);}
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
      switch (page)
        {
          case 1:
            setTemp += 0.05;
            break;
          case 2:
            if (WiFi.status() != WL_CONNECTED) {
              if (chours < 47) {chours++;} else if (chours > 46) {chours=0;} //46/2 is 23
              timechanged = true;
            }
            break;
          case 3:
            if (WiFi.status() != WL_CONNECTED) {
              if (cmins < 119) {cmins++;} else {cmins=0;}
              timechanged = true;
            }
            break;
          case 4:
              whoursdouble++; if (whoursdouble > 47) {whoursdouble=0;}
            break;
          case 5:
              wminsdouble++; if (wminsdouble > 119)  {wminsdouble=0;}
            break;
          case 6:
              shoursdouble++; if (shoursdouble > 47) {shoursdouble=0;}
            break;
          case 7:
              sminsdouble++; if (sminsdouble > 119) {sminsdouble=0;}
            break;
          case 8:
            waketemp += 0.05;
            break;
          case 9:
            sleeptemp += 0.05;
            break;
        }
      
    } else {
      count--;  // upMask = ~downMask
      switch (page)
        {
          case 1:
            setTemp -= 0.05;
            break;
          case 2:
            if (WiFi.status() != WL_CONNECTED) {
              if (chours > 0) {chours--;} else {chours=46;}
              timechanged = true;
            }
            break;
          case 3:
            if (WiFi.status() != WL_CONNECTED) {
              if (cmins > 0) {cmins--;} else {cmins=118;}
              timechanged = true;
            }
            break;
          case 4:
            if (whoursdouble > 0) {whoursdouble--;} else {whoursdouble=47;}
            break;
          case 5:
              if (wminsdouble > 0) {wminsdouble--;} else {wminsdouble=119;}
            break;
          case 6:
            if (shoursdouble > 0) {shoursdouble--;} else {shoursdouble=47;}
            break;
          case 7:
              if (sminsdouble > 0) {sminsdouble--;} else {sminsdouble=119;}
            break;
          case 8:
            waketemp -= 0.05;
            break;
          case 9:
            sleeptemp -= 0.05;
            break;
        }

    }
  }
  abOld = abNew;  // Save new state
  haschanged = true;
}
