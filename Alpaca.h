#ifndef ALPACA_H
#define ALPACA_H

#include <Arduino.h>
#include <ArduinoJson.h>
const static int maxSize = 50;  // max number of assets to manage.



struct AccountDetails {
  float buyingPower;
  float totalNetWorth;
  int numberOfAssetSymbols;
  float accountStartingInvestment;  // Changed to float for safety

  String serializeAccountDetails() {
    // Increased buffer slightly to accommodate string formatting
    StaticJsonDocument<384> doc;
    doc["type"] = "AccountDetails";
    doc["buyingPower"] = (int)buyingPower;
    doc["totalNetWorth"] = serialized(String(totalNetWorth, 2));
    if (accountStartingInvestment > 0) {
      float percentChange = ((totalNetWorth - accountStartingInvestment) / accountStartingInvestment) * 100.0;
      doc["totalLifetimeReturnPercent"] = serialized(String(percentChange, 2));
    }
    String output;
    serializeJson(doc, output);
    return output;
  }
};

struct Position {
  String symbol = "nostock";
  float pricePaid;
  float currentUnitPrice;
  float marketValue;
  float percentChange;
  float percentChangeToday;
  float totalProfit_dollars;
  float qty_available;
  bool isCrypto;

  String serializeInfo() {
    StaticJsonDocument<512> doc;
    doc["symbol"] = symbol;
    doc["avgEntryPrice"] = serialized(String(pricePaid, 2));
    doc["currentUnitPrice"] = serialized(String(currentUnitPrice, 2));
    doc["marketValue"] = serialized(String(marketValue, 2));
    doc["percentChange"] = serialized(String(percentChange, 2));
    doc["percentChangeToday"] = serialized(String(percentChangeToday, 2));
    doc["totalProfit_dollars"] = serialized(String(totalProfit_dollars, 2));
    doc["qty_available"] = qty_available;
    String output;
    serializeJson(doc, output);
    return output;
  }
};

struct AlpacaAccount {
  // --- CONSTRUCTOR ---
  AlpacaAccount(String realKey, String realSecret, String paperKey, String paperSecret, bool is_paperTrading, float totalAccountInvestmentIntoAccount) {
    apikey_RealTrade = realKey;
    apiSecret_RealTrade = realSecret;
    apikey_PaperTrade = paperKey;
    apiSecret_PaperTrade = paperSecret;
    paperTrading = is_paperTrading;
    details.accountStartingInvestment = totalAccountInvestmentIntoAccount;
  }

  AccountDetails details;
  Position allPositions[maxSize];
  String serializeAllPositionsForGemini(bool onlySerializeIfCanTrade, bool marketOpen);
  void printAccountInformation(int startingInvestmentInAccount = 0);
  bool placeBuyOrder(String symbol, float amount);
  bool placeSellOrder(String symbol, float amount);
  bool closePosition(String symbol);
  bool updateAccount();
  void closeAllPositions();
  String print__serialized_portfolio_and_account();
private:
  bool updateHoldings();
  bool getAccountDetails();
  String AccountURL();
  String url();
  String apiKey();
  String api_secret();
  String url_orders();
  void clearHoldings();
  bool paperTrading = true;
  const char* paperAccountURL = "https://paper-api.alpaca.markets/v2/account";
  const char* realAccountURL = "https://api.alpaca.markets/v2/account";
  const char* paperTradeURLBUY = "https://paper-api.alpaca.markets/v2/orders";
  const char* realTradeURLBUY = "https://api.alpaca.markets/v2/orders";
  const char* paperTradeURLpositions = "https://paper-api.alpaca.markets/v2/positions";
  const char* realTradeURLpositions = "https://api.alpaca.markets/v2/positions";
  String apikey_RealTrade = "";
  String apiSecret_RealTrade = "";
  String apikey_PaperTrade = "";
  String apiSecret_PaperTrade = "";
};

#endif