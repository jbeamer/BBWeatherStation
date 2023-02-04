# BirksBeamer Weather Station

Arduino, ThingSpeak project for modest weather station at our home.  

Measures: Wind Speed, Wind Direction, Temperature, Humidity and Barometric Pressure

Shown locally on internal site (currently http://10.0.0.87/, but it moves around) -- current local IP displayed on the local display (hold button to turn on display).

Published remotely to ThingSpeak here: https://thingspeak.com/channels/265281

I'll add pictures of my install at some point.

## Hardware: 
  NodeMCE v1.0 - ESP-12E Module
  BME280 Pressure / Temperature and Humidity   
(installed in shady portion of upstairs balcony)
  Anemometer:  TP1080/2700 
  Wind Direction Sensor: TP1080/2700
  SSD1306 Display
  
All of these parts can be procured for about $50.  Mounting on the roof is a thing, as is wiring from
wind measurement location to the display, CPU and BME280 install location, which will require power as well

## Software 
Anemometer code is all custom

BME and Display libaries from Adafruit.  What would we do without Lady Ada?

The installed location makes updating the firmware difficult, so I have been using Ayush Sharma's ElegantOTA library to remotely upload compiled binaries.  

## Wish list / Known issues:
* local site is bare bones -- would be good to show the data more completely
* thingspeak display also weak -- limited history -- maybe can be confugured on their site?
* integration into my home assistant install would likely make both of those problems irrelevant
* OTA updates are super flaky -- takes me about 10 tries to get through an upload without a reset.  
  Could be:
  * an interrupt collision (anemometer is interrupt driven) -- try disabling when handling OTA
  * a power issue? 
* On reset, the device advertises Adafruit Industries... love them, but this is weird.
  
