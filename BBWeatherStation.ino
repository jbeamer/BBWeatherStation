/**************************************************************************
 BirksBeamer Weather Station

 Hardware: 
  NodeMCE v1.0 - ESP-12E Module
  BME280 Pressure / Temperature and Humidity   
(installed in shady portion of upstairs balcony)
  Anemometer:  TP1080/2700 
  Wind Direction Sensor: TP1080/2700
  SSD1306 Display
  
 TODO: 
 * better commenting of this code
 * BUG: resetting problem during OTA update (interrupts? async?)
 * replace Adafruit splash screen during startup / reset

 Version Planning and History:
 
 0.3.5:  TODO: update the webpage display
 0.3.4:  TODO: async OTA upgrade - see if this fixes flaky uploads
 
 January, 2023:
 0.3.3:  display sleep/awake with button push
 0.3.2:  display on/off with button push
 0.3.1:  finished hardware, button wired and working

 July 26, 2021 - Version 0.2.1
 **************************************************************************/
char *str_version = "version 0.3.3";

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
// TODO: update to Async OTA
#include <ElegantOTA.h>
#include <ThingSpeak.h>
#include "secrets.h"

// Display Settings:
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// BME280: Temperature, Pressure and Humidioty
Adafruit_BME280 bme;

// Anemometer (wind speed)
bool g_bNewAnemometerTick = false;
unsigned long g_TimeOfLastTick = 0;
unsigned long g_tickPeriod = 0;
float g_windSpeed;         // IN MPH
int   g_WindDirection;     // 0 TO 15 FOR CLOCKWISE POINTS ON COMPASS
const char *strWindDirections[] = {"N","NNE","NE","ENE","E","ESE","SE","SSE","S","SSW","SW","WSW","W","WNW","NW","NNW"};

// loop times
unsigned long g_lastPrintTime = 0;
unsigned long g_lastThingSpeakUpdateTime = 0;

// latest weather readings, updated in printValues
String g_strWeather;

// Wifi connection, Local Web Server and ThingSpeak connection
WiFiClient  client;
ESP8266WebServer server(80);
const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;
unsigned long myChannelNumber = SECRET_THINGSPEAK_CHANNEL_NUMBER;
const char * myWriteAPIKey = SECRET_THINGSPEAK_API_KEY;

// state machine variables for display on/off based on button push:
bool g_bBtnPushed = false;
bool g_bDisplayOn = true;

void 
updateWindSpeed()
{
  // g_windSpeed
  if ((g_TimeOfLastTick == 0) || (millis() - g_TimeOfLastTick > 2000))
  {
    g_windSpeed = 0.0;
  }
  else
  {
    g_windSpeed = 1000.0F / g_tickPeriod * 2.4F / 1.61F;
  }
}

String 
getWindSpeed()
{
  updateWindSpeed();
  return(String(g_windSpeed, 1));
}

void 
updateWindDirection()
{
  // g_WindDirection is integer 0-15
  int anemometer = analogRead(A0);
  if (anemometer < 75)        // ESE: target of 69
    g_WindDirection = 5;
  else if (anemometer < 89)   // ENE: target of 85
    g_WindDirection = 3;
  else if (anemometer < 108)  // E:   target of 93
    g_WindDirection = 4;
  else if (anemometer < 150)  // SSE: target of 123
    g_WindDirection = 7;
  else if (anemometer < 200)  // SE:  target of 177-178
    g_WindDirection = 6;
  else if (anemometer < 250)  // SSW: target of 232
    g_WindDirection = 9;
  else if (anemometer < 300)  // S:   target of 271-272
    g_WindDirection = 8;
  else if (anemometer < 400)  // NNE: target of 384
    g_WindDirection = 1;
  else if (anemometer < 500)  // NE:  target of 440
    g_WindDirection = 2;
  else if (anemometer < 590)  // WSW: target of 578
    g_WindDirection = 11;
  else if (anemometer < 650)  // SW:  target of 608
    g_WindDirection = 10;
  else if (anemometer < 720)  // NNW: target of 684-685
    g_WindDirection = 15;
  else if (anemometer < 800)  // N:   target of 775
    g_WindDirection = 0;
  else if (anemometer < 850)  // WNW: target of 821
    g_WindDirection = 13;
  else if (anemometer < 910)  // NW:  target of 885-886 
    g_WindDirection = 14;
  else                        // W:   target of 951
    g_WindDirection = 12;
}

String 
getWindDirection()
{
  updateWindDirection();
  // return (String(22.5F * g_WindDirection, 1));
  return(String(strWindDirections[g_WindDirection]));
}

void 
printValues() 
{
    String wind_spd = getWindSpeed();
    String wind_dir = getWindDirection();
    // temp: convert to F and just one decimal place
    String temp     = String(bme.readTemperature() * 1.8 + 32.0, 1);  
    // pressure: convert to inches of mercury and just one decimal place
    String pressure = String(bme.readPressure() * 0.0002953, 1);
    // humidity: zero decimal places
    String humidity = String(bme.readHumidity(), 0);

    // Print to serial monitor
    Serial.print("temperature:"); Serial.print(temp);     Serial.print("*F  ");
    Serial.print("pressure:");    Serial.print(pressure); Serial.print("inHG  ");
    Serial.print("humidity:");    Serial.print(humidity); Serial.print("%  ");
    Serial.print("wind:");        Serial.print(wind_spd); Serial.print(" mph from "); Serial.println(wind_dir);

    // Print to display if the display button is pushed
    // TODO: change to display->ssd1306_command(SSD1306_DISPLAYOFF);

    if (g_bBtnPushed) {
      if (!g_bDisplayOn) {
        display.ssd1306_command(SSD1306_DISPLAYON);
        g_bDisplayOn = true;
      } 
      display.clearDisplay();   
      display.setTextSize(1);             // Normal 1:1 pixel scale
      display.setTextColor(SSD1306_WHITE);        // Draw white text
      display.setCursor(0,0);             // Start at top-left corner

      display.print("http://");
      display.println(WiFi.localIP());
      
      display.print(temp);
      display.print("*F  ");
      display.print(pressure);
      display.println("inHG");

      display.print("humidity:");
      display.print(humidity);
      display.println("%");

      display.print("wind: ");
      display.print(wind_spd);
      display.print(" mph from ");
      display.println(wind_dir);
      display.display();
    }
    else 
    {
      // button not pushed, make sure the display is asleep
      if (g_bDisplayOn) {
        display.clearDisplay();
        display.display();
        display.ssd1306_command(SSD1306_DISPLAYOFF);
        g_bDisplayOn = false;
      } 
    }

    // Prepare for web server call
    g_strWeather = temp;
    g_strWeather.concat(String("&deg;F<br/>\n "));
    g_strWeather.concat(pressure);
    g_strWeather.concat(String("inHG<br/>\n "));
    g_strWeather.concat(humidity);
    g_strWeather.concat(String("%<br/>\n "));
    g_strWeather.concat(wind_spd);
    g_strWeather.concat(String(" mph from "));
    g_strWeather.concat(wind_dir);
    g_strWeather.concat(String("<br/>\n "));
}

// Interrupt handler for the anemometer.  Just sets the flag to be handled in the main loop
ICACHE_RAM_ATTR void 
handleInterrupt() 
{
  if (g_bNewAnemometerTick) 
  {
    // this should never happen, but could if the main loop gets really bogged down and the 
    // wind is spinning really fast!
    Serial.println("ERROR: not handling the anemometer ticking fast enough!");
  }
  g_bNewAnemometerTick = true;
}

void 
DisplayLine(const char *strText, bool clearScreen, int row)
{
  // Clear the buffer (library starts with adafruit logo)
  if (clearScreen) {
    display.clearDisplay();
  }
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, (row - 1) * 10);
  display.println(strText);
  display.display();
}

void 
DisplayHeader(const char *strHeader)
{
  DisplayLine(strHeader, true, 1);
}

void 
handleServerCall(void)
{
  String html_page = String("<html>\n<head>\n\t<title>BirksBeamer Weather Station</title>\n</head>\n<body>\n <h1>BirksBeamer Weather Station</h1>\n <p>\n ");
  html_page.concat(g_strWeather);
  html_page.concat(" <br/>\nFirmware version: ");
  html_page.concat(str_version);
  html_page.concat(" <a href='/update'>update</a>\n </p>\n");
  html_page.concat(" <p><a href='https://thingspeak.com/channels/265281/private_show'>ThingSpeak Page</a></p>");
  html_page.concat("</body>\n</html>");
  
  server.send(200, "text/html", html_page.c_str());
}

void 
setup() 
{
    // start up our serial monitor
    Serial.begin(115200);
    while (!Serial);

    // start up the temperature, humidity and pressure sensor (BME280)
    if (!bme.begin()) 
    {
        Serial.println("Init Fail,Please Check your address or the wire you connected!!!");
        while (1);
    }

    // start up the physical display:
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) 
    {
        Serial.println(F("SSD1306 allocation failed"));
        while (1);
    }
    // Show initial display buffer contents on the screen --
    // the library initializes this with an Adafruit splash screen.
    // TODO: fix the splash screen
    display.display();
    delay(1000); // Pause for 2 seconds

    // set our pins
    // onboard LEDs
    pinMode(D0, OUTPUT);        // LED: NodeMCU board 
    pinMode(D4, OUTPUT);        // LED: ESP32 board 
    pinMode(D0, OUTPUT);     // on board LED
    digitalWrite(D0, HIGH);  // start with it OFF
    pinMode(D7, INPUT);      // the anemometer 
    attachInterrupt(digitalPinToInterrupt(D7), handleInterrupt, RISING);
    pinMode(D3, INPUT_PULLUP);      // BTN: onboard button -- labeled as "FLASH"
    pinMode(D6, INPUT_PULLUP);      // the button next to the display

    // initalize the OTA server:
    Serial.print("\nConnecting to wifi");
    WiFi.mode(WIFI_STA);
    ThingSpeak.begin(client);  // Initialize ThingSpeak
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.print("\nConnected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    server.on("/", []() {
      handleServerCall();
    });

    // Start ElegantOTA, served at [IP_ADDRESS]/update for future firmware updates
    ElegantOTA.begin(&server);    
    server.begin();
    Serial.println("HTTP server started");

    // DisplayHeader(WiFi.localIP().toString().c_str());
    // DisplayLine(str_version, false, 2);   
}

void 
handleThingSpeak(void)
{
  if (g_lastThingSpeakUpdateTime == 0 || millis() - g_lastThingSpeakUpdateTime > 20000)
  {
    String wind_spd = getWindSpeed();
    String wind_dir = getWindDirection();
    
    // set the fields with the values
    // temperature (F)
    ThingSpeak.setField(1, bme.readTemperature() * 1.8F + 32.0F);
    
    // humidity (%)
    ThingSpeak.setField(2, bme.readHumidity());
    
    // pressure (inHG)
    ThingSpeak.setField(3, bme.readPressure() * 0.0002953F);
    
    // windSpeed (mph)
    ThingSpeak.setField(4, g_windSpeed);

    // windDirection (degrees from N)
    ThingSpeak.setField(5, g_WindDirection * 22.5F);

    // TODO: set status to IP address:
    // WiFi.localIP().toString().c_str()
    
    // write to the ThingSpeak channel
    ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  }
}


void 
loop() 
{ 
    // check to see if it's time to update the displays, which we do every 1 seconds
    if ((g_lastPrintTime == 0) || (millis() - g_lastPrintTime > 1000))
    {
      g_lastPrintTime = millis();
      printValues();
    }

    // check to see if we have a new anemometer tick to handle
    if (g_bNewAnemometerTick) 
    {
      g_bNewAnemometerTick = false;
      unsigned long timeNow = millis();
      g_tickPeriod = timeNow - g_TimeOfLastTick;
      g_TimeOfLastTick = timeNow;
    }

    // handle updating of ThingSpeak (timing done internal to the function)
    handleThingSpeak();
    
    // handle the webserver
    server.handleClient();

    // read button on front of box
    g_bBtnPushed = (digitalRead(D6) == LOW);

    // test button and onboard LED
    digitalWrite(D0, digitalRead(D3));
}
