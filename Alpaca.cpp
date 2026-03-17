#include "HardwareSerial.h"
#include "StockBot.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>



void logger(String ham) {
  if (verboseOutput) {
    Serial.println(ham);
  }
}

// In StockBot.cpp (Implementation)
bool AlpacaAccount::closePosition(String symbol) {
  if (WiFi.status() != WL_CONNECTED) return false;

  // 1. Setup Secure Client (Exact same way as placeSellOrder)
  WiFiClientSecure client;
  client.setInsecure();  // Skip cert validation (Critical for ESP32)

  // 2. Construct the Correct URL
  // url_orders() gives ".../v2/orders". We need ".../v2/positions/{symbol}"
  String server = url_orders();
  server.replace("orders", "positions");  // Switch endpoint
  server += "/" + symbol;                 // Add symbol (e.g., .../positions/ETHUSD)

  Serial.print("Liquidating via: ");
  Serial.println(server);

  HTTPClient http;

  // 3. Connect securely
  if (!http.begin(client, server)) {
    Serial.println("Connection failed!");
    return false;
  }

  // 4. Headers (Copied exactly from working placeSellOrder)
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Apca-Api-Key-Id", apiKey());
  http.addHeader("Apca-Api-Secret-Key", api_secret());

  // 5. Send DELETE Request
  int httpResponseCode = http.sendRequest("DELETE");

  bool success = false;
  if (httpResponseCode == 200 || httpResponseCode == 204 || httpResponseCode == 201) {
    Serial.print(symbol);
    Serial.println(" Position Closed Successfully (Liquidated).");
    success = true;
  } else {
    Serial.print(symbol);
    Serial.print(" Failed to Close. Code: ");
    Serial.println(httpResponseCode);
    String response = http.getString();
    Serial.println("Response: " + response);
    success = false;
  }

  // 6. Cleanup
  http.end();
  return success;
}

bool AlpacaAccount::placeSellOrder(String symbol, float qty) {
  bool success = false;

  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();  // Needed for HTTPS

  String server = url_orders();
  // Pass the secure client to http.begin (Best practice for ESP32)
  http.begin(client, server);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Apca-Api-Key-Id", apiKey());
  http.addHeader("Apca-Api-Secret-Key", api_secret());

  String timeForce;

  // FIX: Check for slash OR if it ends in "USD" to correctly identify Crypto
  if (symbol.indexOf('/') != -1 || symbol.endsWith("USD")) {
    timeForce = "gtc";  // Crypto must be 'Good Till Canceled'
  } else {
    timeForce = "day";  // Stocks default to 'Day'
  }

  // Use high precision for crypto quantities
  String conversion = String(qty, 9);

  String httpRequestData = "{\"symbol\": \"{whatToBuy}\",\"qty\": \"{quantity}\",\"side\": \"sell\",\"type\": \"market\",\"time_in_force\": \"{tif}\"}";
  httpRequestData.replace("{quantity}", conversion);
  httpRequestData.replace("{whatToBuy}", symbol);
  httpRequestData.replace("{tif}", timeForce);

  // Debug: Print what we are actually sending
  Serial.println("Selling " + symbol + " using TIF: " + timeForce);

  int httpResponseCode = http.POST(httpRequestData);

  if (httpResponseCode > 0) {
    String response = http.getString();
    DynamicJsonDocument doc(5000);
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      success = false;
    } else {
      const char* orderId = doc["id"];
      if (orderId != nullptr) {
        Serial.print("Order Successful! Sold: ");
        Serial.println(conversion + " of " + symbol);
        success = true;
        delay(3000);
      } else {
        // Print the error message from Alpaca
        Serial.print("Sell Order Failed! Reason: ");
        Serial.println(doc["message"].as<const char*>());
        Serial.println("Raw Response: " + response);
        success = false;
      }
    }
  } else {
    Serial.print("Error on sending POST: ");
    Serial.println(httpResponseCode);
    success = false;
  }

  http.end();  // CRITICAL: Close connection to free up memory
  return success;
}


void AlpacaAccount::closeAllPositions() {
  Serial.println("Closing all positions...");
  updateAccount();
  for (int i = 0; i < maxSize; i++) {
    if (allPositions[i].symbol != "nostock") {
      bool success = closePosition(allPositions[i].symbol);
    }
  }
}

String AlpacaAccount::serializeAllPositionsForGemini(bool onlySerializeIfCanTrade, bool marketOpen) {
  String finalJson = "{\"type\": \"PortfolioHoldings\", \"positions\": [";

  bool firstEntry = true;
  for (int i = 0; i < maxSize; i++) {
    // 1. Basic validation to skip empty slots
    if (allPositions[i].symbol == "" || allPositions[i].symbol == "nostock") {
      continue;
    }

    // 2. Tradeability Logic
    if (onlySerializeIfCanTrade) {
      // Logic:
      // If it is NOT crypto (meaning it is a stock) AND the market is closed (!marketOpen),
      // then we cannot trade it right now, so we skip it.
      // (Note: If it IS crypto, this if-statement is skipped and the asset is included).
      if (!allPositions[i].isCrypto && !marketOpen) {
        continue;
      }
    }

    // 3. Formatting: Add comma for every item EXCEPT the first one
    if (!firstEntry) {
      finalJson += ",";
    }

    // 4. Append the serialized position
    finalJson += allPositions[i].serializeInfo();
    firstEntry = false;
  }

  finalJson += "]}";
  return finalJson;
}

String AlpacaAccount::AccountURL() {
  if (paperTrading) {
    return paperAccountURL;

  } else {
    return realAccountURL;
  }
}

String AlpacaAccount::url() {
  if (paperTrading) {
    return paperTradeURLpositions;
  } else {
    return realTradeURLpositions;
  }
}

String AlpacaAccount::apiKey() {
  if (paperTrading) {
    return apikey_PaperTrade;
  } else {
    return apikey_RealTrade;
  }
}

String AlpacaAccount::url_orders() {
  if (paperTrading) {
    return paperTradeURLBUY;
  } else {
    return realTradeURLBUY;
  }
}

String AlpacaAccount::api_secret() {
  if (paperTrading) {
    return apiSecret_PaperTrade;
  } else {
    return apiSecret_RealTrade;
  }
}

bool AlpacaAccount::updateAccount() {

  bool success = getAccountDetails();
  if (!success) {
    Serial.println("Failed to update account details.");
    return false;
  }
  success = updateHoldings();
  if (!success) {
    Serial.println("Failed to update account holdings.");
    return false;
  }
  return true;
}

bool AlpacaAccount::getAccountDetails() {
  bool result = false;

  HTTPClient https;
  WiFiClientSecure client;
  client.setInsecure();
  if (https.begin(client, AccountURL())) {
    https.setTimeout(60000);  // 60000 milliseconds = 6 seconds
    https.addHeader("APCA-API-KEY-ID", apiKey());
    https.addHeader("APCA-API-SECRET-KEY", api_secret());
    https.addHeader("accept", "application/json");
    int httpResponseCode = https.GET();
    String response;
    if (httpResponseCode > 0) {
      result = true;
      response = https.getString();
      // Serial.println(response);
      DynamicJsonDocument doc(12000);
      DeserializationError error = deserializeJson(doc, response);
      if (error) {
        result = false;
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return false;
      }
      const char* id = doc["id"];          // "aa10ff78-0fad-4b83-a969-bfea7f0fe50b"
      const char* equity = doc["equity"];  // "2865"
      details.buyingPower = doc["buying_power"].as<float>();
      details.totalNetWorth = doc["equity"].as<float>();
    } else {
      Serial.println(response);
      result = false;
      response = https.getString();
    }
  }

  return result;
}

void AlpacaAccount::clearHoldings() {
  for (int i = 0; i < maxSize; i++) {
    allPositions[i].symbol = "nostock";
  }
}

String AlpacaAccount::print__serialized_portfolio_and_account() {
  String accountString = details.serializeAccountDetails();
  return (accountString + serializeAllPositionsForGemini(false, true));
}

void AlpacaAccount::printAccountInformation(int startingInvestmentInAccount) {
  Serial.println(">>>>>>PORTFOLIO<<<<<<");

  float totalProfit = 0.0;  // Track cumulative profit across all positions

  for (int i = 0; i < maxSize; i++) {
    // 1. Skip if this slot is empty (no symbol assigned)
    if (allPositions[i].symbol == "nostock") {
      continue;
    }

    totalProfit += allPositions[i].totalProfit_dollars;  // Accumulate profit

    Serial.print("======== ");
    Serial.print("ASSET# ");
    Serial.print(i + 1);
    Serial.println(" ===========");
    Serial.print("Symbol:          ");
    Serial.print(allPositions[i].symbol);
    Serial.println(allPositions[i].isCrypto ? " (CRYPTO)" : "");
    // Serial.print("Qty Available:   ");
    // Serial.println(allPositions[i].qty_available, 6);
    // Serial.print("Avg Price Paid:  $");
    // Serial.println(allPositions[i].pricePaid, 2);
    // Serial.print("Current Price:   $");
    // Serial.println(allPositions[i].currentUnitPrice, 2);
    Serial.print("Market Value:    $");
    Serial.println(allPositions[i].marketValue, 2);
    Serial.print("Total Profit:    $");
    if (allPositions[i].totalProfit_dollars > 0) {
      Serial.print("+");
    }
    Serial.println(allPositions[i].totalProfit_dollars, 2);
    Serial.print("Change (Total):  ");
    if (allPositions[i].percentChange > 0) {
      Serial.print("+");
    }
    Serial.print(allPositions[i].percentChange * 100, 2);
    Serial.println("%");


    Serial.print("Change (Today):  ");
    if (allPositions[i].percentChangeToday > 0) {
      Serial.print("+");
    }
    Serial.print(allPositions[i].percentChangeToday * 100, 2);
    Serial.println("%");
  }

  Serial.println("==============================");
  Serial.println(">>>>>>>>>>>>ACCOUNT<<<<<<<<<<<<");
  Serial.print("Buying Power:     $");
  Serial.println(details.buyingPower);
  Serial.print("Networth:         $");
  Serial.println(details.totalNetWorth);
  Serial.print("Number of Assets: ");
  Serial.println(details.numberOfAssetSymbols);

  // New Logic: Only print All-Time % if starting amount is provided
  if (startingInvestmentInAccount > 0) {
    float totalGainLoss = details.totalNetWorth - startingInvestmentInAccount;
    float totalPercentChange = (totalGainLoss / startingInvestmentInAccount) * 100.0;

    Serial.print("Account Profit:   $");
    Serial.print(totalGainLoss, 2);

    Serial.print("  (");
    if (totalPercentChange > 0) {
      Serial.print("+");
    } else {
      Serial.print("-");
    }
    Serial.print(totalPercentChange, 2);
    Serial.println(" %)");
  }

  Serial.println(">>>>>>>>>>>>>>>><<<<<<<<<<<<<<<");
}


bool AlpacaAccount::updateHoldings() {
  bool result = false;

  HTTPClient https;
  WiFiClientSecure client;
  client.setInsecure();
  if (https.begin(client, url())) {
    https.setTimeout(60000);  // 60000 milliseconds = 6 seconds
    https.addHeader("APCA-API-KEY-ID", apiKey());
    https.addHeader("APCA-API-SECRET-KEY", api_secret());
    https.addHeader("accept", "application/json");
    int httpResponseCode = https.GET();
    if (httpResponseCode > 0) {
      clearHoldings();
      result = true;

      DynamicJsonDocument doc(24576);
      DeserializationError error = deserializeJson(doc, https.getString());
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return false;
      }
      int index = 0;
      for (JsonObject item : doc.as<JsonArray>()) {
        const char* asset_id = item["asset_id"];                                  // "64bbff51-59d6-4b3c-9351-13ad85e3c752", ...
        const char* symbol = item["symbol"];                                      // "BTCUSD", "ETHUSD"
        const char* exchange = item["exchange"];                                  // "CRYPTO", "CRYPTO"
        const char* asset_class = item["asset_class"];                            // "crypto", "crypto"
        bool asset_marginable = item["asset_marginable"];                         // false, false
        const char* qty = item["qty"];                                            // "0.000246951", "0.000620359"
        const char* avg_entry_price = item["avg_entry_price"];                    // "68660.287361156", "3152.812669838"
        const char* side = item["side"];                                          // "long", "long"
        const char* market_value = item["market_value"];                          // "17.303620978746", "2.13092696141"
        const char* cost_basis = item["cost_basis"];                              // "16.955726624", "1.955875715"
        const char* unrealized_pl = item["unrealized_pl"];                        // "0.347894354746", "0.17505124641"
        const char* unrealized_plpc = item["unrealized_plpc"];                    // "0.0205178086708223", "0.0895001891313938"
        const char* unrealized_intraday_pl = item["unrealized_intraday_pl"];      // "0.347894354621164644", ...
        const char* unrealized_intraday_plpc = item["unrealized_intraday_plpc"];  // "0.0205178086633088", ...
        const char* current_price = item["current_price"];                        // "70069.046", "3434.99"
        const char* lastday_price = item["lastday_price"];                        // "66446.9", "3054.229"
        const char* change_today = item["change_today"];                          // "0.0545118884402433", "0.1246668144399127"
        const char* qty_available = item["qty_available"];                        // "0.000246951", "0.000620359"
        // 1. Assign the fields you already parsed (just to be safe)
        if (index < maxSize) {
          if (strcmp(exchange, "CRYPTO") == 0) {
            allPositions[index].isCrypto = true;
          } else {
            allPositions[index].isCrypto = false;
          }
          allPositions[index].symbol = symbol;                                  // Works because String can take const char*
          allPositions[index].pricePaid = item["avg_entry_price"].as<float>();  // 2. Parse and assign the rest using .as<float>()
          allPositions[index].currentUnitPrice = item["current_price"].as<float>();
          allPositions[index].marketValue = item["market_value"].as<float>();
          allPositions[index].percentChange = item["unrealized_plpc"].as<float>();
          allPositions[index].percentChangeToday = item["unrealized_intraday_plpc"].as<float>();
          allPositions[index].totalProfit_dollars = item["unrealized_pl"].as<float>();
          allPositions[index].qty_available = item["qty_available"].as<float>();
        }
        index++;
      }
      details.numberOfAssetSymbols = index;
      if (verboseOutput) {
        Serial.println("HOLDINGS UPDATED.");
        // printAllAccountHoldings();
      }
    } else {
      result = false;
      Serial.print("Error on sending GET: ");
      Serial.println(httpResponseCode);
    }
    https.end();
  }
  return result;
}

bool AlpacaAccount::placeBuyOrder(String symbol, float notionAmount) {
  bool success = false;

  // 1. Basic Validation
  if (notionAmount <= 0) {
    Serial.println("Error: Buy amount must be greater than $0.00");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();  // Skip certificate validation (Required for ESP32)

  HTTPClient http;
  String server = url_orders();

  // 2. Connect securely
  // Passing 'client' is required for modern ESP32 board definitions
  if (!http.begin(client, server)) {
    Serial.println("Connection failed!");
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Apca-Api-Key-Id", apiKey());
  http.addHeader("Apca-Api-Secret-Key", api_secret());

  // 3. Crypto vs Stock Logic
  // Stocks = "day" (Market Close)
  // Crypto = "gtc" (Good Till Canceled - 24/7 market)
  String timeForce = "day";
  if (symbol.indexOf('/') != -1 || symbol.endsWith("USD")) {
    timeForce = "gtc";
  }

  // 4. Construct JSON Payload
  // usage of 'notional' allows us to buy by Dollar Amount ($) instead of Share Qty
  String amountString = String(notionAmount, 2);  // Force 2 decimal places (e.g. "50.00")

  String httpRequestData = "{\"symbol\": \"{whatToBuy}\",\"notional\": \"{amount}\",\"side\": \"buy\",\"type\": \"market\",\"time_in_force\": \"{tif}\"}";
  httpRequestData.replace("{amount}", amountString);
  httpRequestData.replace("{whatToBuy}", symbol);
  httpRequestData.replace("{tif}", timeForce);

  // Debug Print
  Serial.print("Buying $");
  Serial.print(amountString);
  Serial.print(" of ");
  Serial.print(symbol);
  Serial.print(" (TIF: ");
  Serial.print(timeForce);
  Serial.println(")...");

  // 5. Send Request
  int httpResponseCode = http.POST(httpRequestData);

  if (httpResponseCode > 0) {
    String response = http.getString();

    // Alpaca returns 200 (OK) or 201 (Created) for successful orders
    if (httpResponseCode == 200 || httpResponseCode == 201) {
      DynamicJsonDocument doc(4096);
      DeserializationError error = deserializeJson(doc, response);

      if (!error) {
        const char* id = doc["id"];
        if (id != nullptr) {
          success = true;
          Serial.print("Buy Order Successful:");
          Serial.print(" $");
          Serial.print(amountString);
          Serial.print(" of ");
          Serial.println(symbol);
          delay(1000);
        }
      }
    }

    // If success is still false, print the error
    if (!success) {
      Serial.print("Buy Order Failed (Code ");
      Serial.print(httpResponseCode);
      Serial.println(")");

      // Parse the error message if possible
      DynamicJsonDocument errDoc(1024);
      deserializeJson(errDoc, response);
      if (errDoc.containsKey("message")) {
        Serial.print("Reason: ");
        Serial.println(errDoc["message"].as<const char*>());
      } else {
        Serial.println("Raw Response: " + response);
      }
    }

  } else {
    Serial.print("Failed: Error on sending POST: ");
    Serial.println(http.errorToString(httpResponseCode));
  }

  // 6. Cleanup
  http.end();  // Critical to prevent memory leaks
  return success;
}
