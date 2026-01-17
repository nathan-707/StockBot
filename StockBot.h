#include "HardwareSerial.h"
#include <ArduinoJson.h>
#include <Arduino.h>
#include "Alpaca.h"


#ifndef STOCKBOT_H
#define STOCKBOT_H

const bool verboseOutput = false;

struct StockRecommendation {
  String symbol = "nostock";
  String reason;
  int expected_hold_period_days;
  float confidenceLevel;
};

struct GeminiBuyRecommendation {
  StockRecommendation recommendedStocks[maxSize];
  float recommendedAmountToInvest;
};

enum class Status {
  WORKING,
  ERROR,
  BUSY
};

enum class SellAction {
  HOLD,      // 0
  SELL_ALL,  // 1
  SELL_HALF  // 2
};

enum class TradeMode {
  STOCKS_AND_CRYPTO,  // 0
  STOCKS_ONLY,        // 1
  CRYPTO_ONLY         // 2
};

enum class TradeRoutine {
  Routine_1,
  Routine_2,
};

struct BotConfiguration {
  TradeMode assetTypeToBeBought = TradeMode::STOCKS_AND_CRYPTO;
  bool reinvestIfRecommended = false;
  int ai_check_interval_HOURS = 2;
  float minimum_AI_Confidence_Level_In_Order_To_BUY = 0.5;
  bool logOutput = true;
  TradeRoutine routine = TradeRoutine::Routine_1;
  String buying_prompt_stocks;
  String buying_prompt_crypto;
  String selling_prompt;
  String name = "";

  BotConfiguration(TradeMode assetTypeToBeBought,
                   bool reinvestIfRecommended,
                   int ai_check_interval_HOURS,
                   float minimum_AI_Confidence_Level_In_Order_To_BUY,
                   bool logOutput,
                   TradeRoutine routine,
                   String buying_prompt_stocks,
                   String buying_prompt_crypto,
                   String selling_prompt,
                   String name
                   ) {
    this->assetTypeToBeBought = assetTypeToBeBought;
    this->reinvestIfRecommended = reinvestIfRecommended;
    this->ai_check_interval_HOURS = ai_check_interval_HOURS;
    this->minimum_AI_Confidence_Level_In_Order_To_BUY = minimum_AI_Confidence_Level_In_Order_To_BUY;
    this->logOutput = true;
    this->routine = routine;
    this->buying_prompt_stocks = buying_prompt_stocks;
    this->buying_prompt_crypto = buying_prompt_crypto;
    this->selling_prompt = selling_prompt;
    this->name = name;
  }

  BotConfiguration() {
    name = "Default config";
  }
};

struct PositionActionRecommendation {
  String symbol = "nostock";
  String reason;
  float confidenceLevel;
  SellAction action;
};


class StockBot {
public:

  void monitor();
  void print___Gemini_BUY_StockSuggestions();
  void print___Gemini_SELL_StockSuggestions();
  bool getGemini_BUY_Suggestions(BotConfiguration configuration);
  bool getGemini_SELL_StockSuggestions(BotConfiguration configuration);
  void buyDiversifiedGeminiStockSuggestions(float dollarsToInvest, float minimumConfidenceLevelToBuy);
  void buyFirstGeminiStockSuggestion(float dollarsToInvest);
  bool sellAllGeminiSellSuggestions();
  StockBot(bool is_paperTrading, int totalAccountInvestmentIntoAccount, const char* real_key, const char* real_secret, const char* paper_key, const char* paper_secret, const char* geminiKey, BotConfiguration configuration);
  AlpacaAccount account;
  GeminiBuyRecommendation geminiBuyRecommendation;
  PositionActionRecommendation geminiRecommendedPositionsToSell[maxSize];
  Status getStatus();
  void getTimeUntilNextRoutine();
private:
  unsigned long monitorTimer = 360000 * 72;
  bool usingAI = false;
  Status stockbotStatus = Status::WORKING;
  BotConfiguration configuration;
  bool accountError = false;
  bool succesfullyGotSellSuggestions = true;
  bool succesfullyGotBuySuggestions = true;
  bool noGeminiBuyRecommendations();
  const int assetBuyMinimum = 20;  // minimum amount that can be put into a asset.
  bool marketIsOpen();
  bool timeNotConfigured = true;
  void clearGeminiBuyRecommendations();
  void clearGeminiSellRecommendations();
  void printStat(String nameOfObject, String printObject);
  void printStat(String nameOfObject, int printObject);
  // gemini info and addresses
  String apiKey;
  const char* host = "generativelanguage.googleapis.com";
  const String url = "/v1beta/models/gemini-3-pro-preview:generateContent";
  // prompts:
};


#endif