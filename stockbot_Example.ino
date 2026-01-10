#include "StockBot.h"
#include "secrets.h"
#include <WiFi.h>
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#define PIN 38
#define NUMPIXELS 1

/*
things changed:
got rid of 'minimum investment. whether or not anything is invested is entirely dependany on ai and its prompt.'
allow the ai to recommend 0 things to buy and if it does, it wont buy anything now instead of being forced to. 

added the 'assetminimum' of $20, this will prevent it from have only like pennies or few dolalrs in things.
changed selling prompt to make it make the ai determine what noise levels are 
*/

//////////////////////////////////////////////////////////
// #ifndef PAPER_API_KEY
// #define PAPER_API_KEY "PASTE_PAPER_KEY_HERE"
// #endif
// #ifndef PAPER_API_KEY_SECRET
// #define PAPER_API_KEY_SECRET "PASTE_PAPER_SECRET_HERE"
// #endif
// #ifndef REAL_API_KEY
// #define REAL_API_KEY "PASTE_REAL_KEY_HERE"
// #endif
// #ifndef REAL_API_SECRET
// #define REAL_API_SECRET "PASTE_REAL_SECRET_HERE"
// #endif
// #ifndef GEMINI_KEY
// #define GEMINI_KEY "PASTE_GEMINI_KEY_HERE"
// #endif
/////////////////////////////////////////////
const bool PAPER = false;            //////
const int startingInvestment = 700;  //////
/////////////////////////////////////////////
BotConfiguration defaultConfig;  /////
////////////////////////////////////////////




StockBot stockbot(PAPER, startingInvestment, REAL_API_KEY, REAL_API_SECRET, PAPER_API_KEY, PAPER_API_KEY_SECRET, GEMINI_KEY);
bool everythingWorking = false;
const unsigned long interval = 3600000;  // 1 hour in milliseconds
unsigned long previousMillis = interval + 1000;
unsigned long previousPrintMillis = interval + 1000;
const unsigned long printInterval = 600000;  // 10 minutes in ms
unsigned long blinkTimer;
bool blink;
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_RGB + NEO_KHZ800);
// const char* ssid = "NEiPhone";
// const char* password = "12345678";
// const char* ssid = "Province.SynergyWifi.com";
// const char* password = "coldfang75";
const char* ssid = "eriksen99";
const char* password = "DCchiro99";

void setup() {
  Serial.begin(9600);   // Make sure your Serial Monitor matches this
  pixels.begin();       // begin lights and turn them off
  delay(1000);          // Give serial time to start
  WiFi.mode(WIFI_STA);  // Explicitly set station mode
  WiFi.begin(ssid, password);

  if (psramFound()) {
    Serial.println("PSRAM found and enabled!");
    Serial.printf("Total PSRAM: %d bytes\n", ESP.getPsramSize());
    Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
  } else {
    delay(3000);
    Serial.println("PSRAM NOT found or NOT enabled!");
    delay(999999);
  }

  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }





  // end of setup. put code to run below here.
}



void monitorStocks() {
  pixels.setPixelColor(0, pixels.Color(0, 0, 50));
  pixels.show();
  everythingWorking = stockbot.monitorStocksWithGemini(defaultConfig);
}



void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    monitorStocks();
  }

  if (currentMillis - previousPrintMillis >= printInterval) {
    previousPrintMillis = currentMillis;  // Reset print timer
    unsigned long timePassed = currentMillis - previousMillis;
    unsigned long timeRemaining = interval - timePassed;
    Serial.print(timeRemaining / 60000);  // Convert ms to minutes
    Serial.println(" minutes until next check.");
  }


  if (currentMillis - blinkTimer >= 4000) {
    blinkTimer = millis();
    blink = !blink;
  }

  if (blink) {
    if (everythingWorking) {
      pixels.setPixelColor(0, pixels.Color(50, 0, 0));

    } else {
      pixels.setPixelColor(0, pixels.Color(0, 255, 0));
    }
  } else {

    if (everythingWorking) {
      pixels.setPixelColor(0, pixels.Color(0, 0, 0));

    } else {
      pixels.setPixelColor(0, pixels.Color(0, 255, 0));
    }
  }
  pixels.show();

  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command == "monitor") {
      Serial.println("Manual Override: Running Stock Monitor now...");
      monitorStocks();
      previousMillis = currentMillis;
    } else if (command == "a") {
      Serial.println("Updating account...");
      stockbot.account.updateAccount();
      stockbot.account.printAccountInformation(startingInvestment);
    } else if (command == "rec") {
      stockbot.print___Gemini_BUY_StockSuggestions();
      stockbot.print___Gemini_SELL_StockSuggestions();
    } else if (command == "t" || command == "time") {
      unsigned long timePassed = currentMillis - previousMillis;
      unsigned long timeRemaining = interval - timePassed;
      Serial.print("Manual Check: ");
      Serial.print(timeRemaining / 60000);
      Serial.println(" minutes until next check.");
    } else if (command == "all") {
      stockbot.print___Gemini_SELL_StockSuggestions();
      stockbot.print___Gemini_BUY_StockSuggestions();
      Serial.println("Updating account...");
      stockbot.account.updateAccount();
      stockbot.account.printAccountInformation(startingInvestment);
    }

    else if (command == "closeall") {
      stockbot.account.closeAllPositions();
    }

    else if (command == "printport") {
      Serial.println("== portfolio ==");
      Serial.println(stockbot.account.print__serialized_portfolio_and_account());
      Serial.println("=======");
    }

    else if (command == "h" || command == "help") {
      Serial.print("a: ");
      Serial.println("print account info");
      Serial.print("rec: ");
      Serial.println("print gemini buy and sell recommendations");
      Serial.print("t: ");
      Serial.println("print time until next AI check");
      Serial.print("all: ");
      Serial.println("print all info");
      Serial.print("closeall: ");
      Serial.println("close all positions instantly in account");
      Serial.print("monitor: ");
      Serial.println("force AI recommendations fetch");
    } else {
      Serial.println("unknown command");
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
  }
}