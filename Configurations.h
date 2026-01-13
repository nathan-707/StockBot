#include <type_traits>



#ifndef CONFIGURATIONS_H
#define CONFIGURATIONS_H

#include "HardwareSerial.h"
#include "StockBot.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>  // Include at the top of your file
#include "BuyingPrompts.h"
#include "SellingPrompts.h"

#define doNotReinvest false
#define REINVEST true
#define oneHour 1
#define defaultConfidenceLevel 0.5
#define alwaysLogOutput true








// Create different bot configs here.
BotConfiguration longTermBotConfiguration = BotConfiguration(TradeMode::STOCKS_ONLY, true, oneHour, defaultConfidenceLevel, alwaysLogOutput, TradeRoutine::Routine_1, buying_prompt_longterm, buying_prompt_longterm, gemini_Sell_Recommendation_prompt, "Long Term Config");







#endif