#include "HardwareSerial.h"
#include "StockBot.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>  // Include at the top of your file

// TODO: implement the interval in the monitorstock functinos.
// DONE:::::::
// TODO: add SellAllAssets function into alpaca.
// todo: move gemini api key to secrets.
// TODO: serialize overall account profit into the prompts too for the ai.
// TODO: find way to disable server time out when calling geminin

// --- CONSTRUCTOR ---
StockBot::StockBot(bool is_paperTrading, int totalAccountInvestmentIntoAccount,
                   const char* real_key, const char* real_secret,
                   const char* paper_key, const char* paper_secret, const char* geminiKey)
  // 1. Initializer List: Calls the AlpacaAccount constructor immediately
  : account(real_key, real_secret, paper_key, paper_secret, is_paperTrading, totalAccountInvestmentIntoAccount) {

  Serial.println("\n--- StockBot Ready ---");
  apiKey = geminiKey;
}

void StockBot::printStat(String nameOfObject, String printObject) {
  Serial.print(nameOfObject + ": ");
  Serial.println(printObject);
}

void StockBot::printStat(String nameOfObject, int printObject) {
  Serial.print(nameOfObject + ": ");
  Serial.println(printObject);
}

String sellActionRawName(SellAction action) {
  if (action == SellAction::HOLD) {  // Added "SellAction::"
    return "HOLD";
  } else if (action == SellAction::SELL_ALL) {  // Added "SellAction::"
    return "SELL_ALL";
  } else if (action == SellAction::SELL_HALF) {  // Added missing case
    return "SELL_HALF";
  }
  return "UNKNOWN";
}


bool StockBot::monitorStocksWithGemini(BotConfiguration configuration) {
  // --- 1. PREPARE ---
  bool succesfullyGotSellSuggestions = false;
  bool succesfullyGotBuySuggestions = false;

  if (account.updateAccount() == false) {
    Serial.println("FAILED TO UPDATE ACCOUNT. ABORTING AI CALLS...");
    return false;
  }

  if (configuration.assetTypeToBeBought == TradeMode::STOCKS_ONLY && marketIsOpen() == false) {
    Serial.println("Trade stocks only is true, and the market is closed. Bailing out.");
    return true;
  }

  // --- 2. GET AI INSIGHTS ---
  if (configuration.reinvestIfRecommended) {
    succesfullyGotBuySuggestions = getGemini_BUY_Suggestions(configuration.assetTypeToBeBought);
  } else {
    Serial.println("Auto reinvest off. Skipping get buy recommendations.");
    succesfullyGotBuySuggestions = true; // no need to get buy suggestions, not buying.
  }


  if (configuration.logOutput) print___Gemini_BUY_StockSuggestions();

  succesfullyGotSellSuggestions = getGemini_SELL_StockSuggestions();
  if (configuration.logOutput && succesfullyGotSellSuggestions) print___Gemini_SELL_StockSuggestions();
  // note: if it fails to get either one of these, all the recommendations will be empty so its safe to still try to buy or sell because if its empty nothing will happen.

  // --- 3. EXECUTE SELLS ---
  bool did_sell_something = sellAllGeminiSellSuggestions();

  // Wait for sells to register, then refresh balance
  if (did_sell_something) {
    delay(2000);
    account.updateAccount();
  }

  // --- 4. CALCULATE INVESTMENT AMOUNT ---
  int totalInvested = account.details.totalNetWorth - account.details.buyingPower;
  // float amountToInvest = 0;

  // // Strategy: Are we topping up to a minimum? Or just investing surplus?
  // if (totalInvested < configuration.minimum_to_have_invested_at_all_times) {
  //   // GAP FILL: We are under-invested. Calculate the gap.
  //   amountToInvest = configuration.minimum_to_have_invested_at_all_times - totalInvested;
  //   Serial.print("Investments are below the investment minimum. Investing the amount needed to reach investment minimum which is ");
  //   Serial.println(amountToInvest);
  // } else {
  // GROWTH: We hit our minimum. Use AI's suggested amount for growth.
  float amountToInvest = geminiBuyRecommendation.recommendedAmountToInvest;
  // }

  // SAFETY: Never invest more than we have!

  if (account.details.buyingPower < assetBuyMinimum || noGeminiBuyRecommendations() == true) {
    Serial.println("Not enough money to buy, or no recommendations. bailing early.");
    return true;
  } else if (geminiBuyRecommendation.recommendedAmountToInvest > account.details.buyingPower || geminiBuyRecommendation.recommendedAmountToInvest < assetBuyMinimum) {
    Serial.println("gemini tried to invest either more than whats in account, or less than the assetBuyMinimum. setting amount to invest to the minimum instead.");
    amountToInvest = assetBuyMinimum;
  }




  // --- 5. DECIDE TO BUY ---
  // We buy if: (We sold & want to reinvest) OR (We are under our minimum target)
  // bool should_buy = (did_sell_something && configuration.reinvest_after_sell) || (totalInvested < configuration.minimum_to_have_invested_at_all_times);




  // // Override: If we are flush with cash (Cash > Target), we should also buy (The "Cash Rich" rule)
  // if (account.details.buyingPower >= configuration.minimum_to_have_invested_at_all_times) {
  //   should_buy = true;
  // }



  // --- 6. EXECUTE BUY ---
  if (configuration.reinvestIfRecommended && succesfullyGotBuySuggestions) {

    if (configuration.logOutput) print___Gemini_BUY_StockSuggestions();

    // if (totalInvested < configuration.minimum_to_have_invested_at_all_times) {
    //   Serial.println("Investments are below minimum. Buying assets...");
    // }

    if (amountToInvest > assetBuyMinimum) {
      buyDiversifiedGeminiStockSuggestions(amountToInvest, configuration.minimum_AI_Confidence_Level_In_Order_To_BUY);
    } else {
      buyFirstGeminiStockSuggestion(amountToInvest);
    }
  }

  // --- 7. REPORTING ---
  if (configuration.logOutput) {
    account.updateAccount();
    account.printAccountInformation(account.details.accountStartingInvestment);
  }


  return (succesfullyGotSellSuggestions && succesfullyGotBuySuggestions);
}

void StockBot::print___Gemini_BUY_StockSuggestions() {
  Serial.println();
  Serial.println(">>>>>> GEMINI BUY SUGGESTIONS <<<<<<<<<<<<<<<<<<<<<<<<<<");
  bool noRecommendations = true;

  for (int i = 0; i < maxSize; i++) {
    // FIX: Check if length is GREATER than 0 to find valid stocks
    if (geminiBuyRecommendation.recommendedStocks[i].symbol == "nostock") {
      continue;
    }
    noRecommendations = false;
    Serial.print("Symbol: ");
    Serial.println(geminiBuyRecommendation.recommendedStocks[i].symbol);
    Serial.print("Confidence level: ");
    Serial.print(geminiBuyRecommendation.recommendedStocks[i].confidenceLevel);
    Serial.println(" %)");
    Serial.print("Reason: ");
    Serial.println(geminiBuyRecommendation.recommendedStocks[i].reason);
    Serial.println("====================================================");
  }
  if (noRecommendations) {
    Serial.println("NO BUY RECOMMENDATIONS RIGHT NOW");
  } else {
    Serial.print("Current Account Balance: $");
    Serial.println(account.details.buyingPower);
    Serial.print("Recommended Amount to Invest: $");
    Serial.println(geminiBuyRecommendation.recommendedAmountToInvest);
  }
  Serial.println(">>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<");
}

void StockBot::print___Gemini_SELL_StockSuggestions() {
  Serial.println();
  Serial.println(">>>>>> GEMINI SELL SUGGESTIONS <<<<<<<<<<");
  bool noRecommendations = true;

  for (int i = 0; i < maxSize; i++) {
    // FIX: Check if length is GREATER than 0
    if (geminiRecommendedPositionsToSell[i].symbol == "nostock") {
      continue;
    }
    noRecommendations = false;
    Serial.print("Symbol: ");
    Serial.println(geminiRecommendedPositionsToSell[i].symbol);
    Serial.print("Recommended Action: ");
    Serial.println(sellActionRawName(geminiRecommendedPositionsToSell[i].action));
    Serial.print("Confidence level: ");
    Serial.print(geminiRecommendedPositionsToSell[i].confidenceLevel);
    Serial.println(" %)");
    Serial.print("Reason: ");
    Serial.println(geminiRecommendedPositionsToSell[i].reason);
    Serial.println("====================================================");
  }
  if (noRecommendations) {
    Serial.println("NO SELL RECOMMENDATIONS");
  }
  Serial.println(">>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<");
}

bool StockBot::noGeminiBuyRecommendations() {
  bool empty = true;
  for (int i = 0; i < maxSize; i++) {
    if (geminiBuyRecommendation.recommendedStocks[i].symbol != "nostock") {
      empty = false;
    }
  }
  return empty;
}



void StockBot::clearGeminiSellRecommendations() {
  for (int i = 0; i < maxSize; i++) {
    geminiRecommendedPositionsToSell[i].symbol = "nostock";
  }
}

void StockBot::clearGeminiBuyRecommendations() {
  for (int i = 0; i < maxSize; i++) {
    geminiBuyRecommendation.recommendedStocks[i].symbol = "nostock";
  }
}

bool StockBot::sellAllGeminiSellSuggestions() {
  Serial.println("Performing what gemini recommended...");

  if (account.updateAccount() == false) {
    Serial.println("Failed to update account. aborting gemini sell request.");
    return false;
  }

  bool soldSomething = false;

  for (int i = 0; i < maxSize; i++) {
    for (int w = 0; w < maxSize; w++) {
      if (account.allPositions[i].symbol == geminiRecommendedPositionsToSell[w].symbol) {  // recommendation matched to actual asset. sell it depending on what was recommeneded.

        if (account.allPositions[i].symbol == "nostock") {
          continue;
        }

        if (geminiRecommendedPositionsToSell[w].action == SellAction::SELL_HALF) {
          float half = account.allPositions[i].qty_available / 2;
          bool success = account.placeSellOrder(account.allPositions[i].symbol, half);
          if (success) {
            Serial.print("Sold HALF of ");
            Serial.println(account.allPositions[i].symbol);
            soldSomething = true;
          } else {
            Serial.print("Failed to sell half of ");
            Serial.println(account.allPositions[i].symbol);
          }
          delay(500);
        }

        else if (geminiRecommendedPositionsToSell[w].action == SellAction::SELL_ALL) {
          float all = account.allPositions[i].qty_available;
          bool success = account.closePosition(account.allPositions[i].symbol);
          if (success) {
            Serial.print("Sold ALL of ");
            Serial.println(account.allPositions[i].symbol);
            soldSomething = true;
          } else {
            Serial.print("Failed to sell all of ");
            Serial.println(account.allPositions[i].symbol);
          }
          delay(500);
        }

        else {
          Serial.print("HOLDING ");
          Serial.println(account.allPositions[i].symbol);
        }
      }
    }
  }

  if (soldSomething) {
    account.updateAccount();
  }
  return soldSomething;
}




bool StockBot::marketIsOpen() {
  // 1. Initial Time Configuration (Run once)
  if (timeNotConfigured) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    // Handle Daylight Saving Time automatically (US Eastern)
    setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
    tzset();

    Serial.print("Waiting for NTP time sync: ");
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2) {
      delay(500);
      Serial.print(".");
      now = time(nullptr);
    }
    Serial.println(" DONE");
    timeNotConfigured = false;
  }

  // 2. Get Local Time
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return false;
  }

  // 3. Weekend Check (0=Sunday, 6=Saturday)
  int weekday = timeinfo.tm_wday;
  if (weekday == 0 || weekday == 6) {
    return false;
  }

  // 4. Calculate Market Minutes
  int currentMinutes = (timeinfo.tm_hour * 60) + timeinfo.tm_min;

  // Start: 9:30 AM (570 minutes)
  const int MARKET_OPEN_MINUTES = (9 * 60) + 30;

  // End: 4:00 PM (16:00 military time -> 960 minutes)
  // FIXED: Changed from 19 (7PM) to 16 (4PM)
  const int MARKET_CLOSE_MINUTES = (16 * 60);

  return (currentMinutes >= MARKET_OPEN_MINUTES && currentMinutes < MARKET_CLOSE_MINUTES);
}


bool StockBot::getGemini_BUY_Suggestions(TradeMode tradeType) {
  bool success = false;

  // 1. Account Safety Check
  if (account.updateAccount() == false) {
    Serial.println("Failed to update account. Aborting Gemini request.");
    return success;
  }

  clearGeminiBuyRecommendations();

  // 2. Determine "Mode" (Crypto vs Stocks)
  bool isCryptoMode = false;

  if (tradeType == TradeMode::CRYPTO_ONLY) {
    isCryptoMode = true;
    Serial.println("Mode: CRYPTO_ONLY");
  } else if (tradeType == TradeMode::STOCKS_ONLY) {
    isCryptoMode = false;
    Serial.println("Mode: STOCKS_ONLY");
  } else if (tradeType == TradeMode::STOCKS_AND_CRYPTO) {
    if (marketIsOpen()) {
      isCryptoMode = false;
      Serial.println("Mode: STOCKS_AND_CRYPTO (Market OPEN -> Stocks)");
    } else {
      isCryptoMode = true;
      Serial.println("Mode: STOCKS_AND_CRYPTO (Market CLOSED -> Crypto)");
    }
  } else {

    Serial.println("ERROR: No trade mode!");
    return false;
  }

  // 3. Prepare Logic Strings

  // String activePrompt = isCryptoMode ? crypto_Buying_Recommendations_Prompt : regularStocks_Buying_Recommendations_Prompt;
  String activePrompt = outsmart_the_market_Prompt;


  String symbolDescription = isCryptoMode ? "The crypto ticker symbol. format 'SYMBOL/USD' (e.g., BTC/USD, ETH/USD)." : "The stock ticker symbol (e.g., AAPL, TSLA).";

  // 4. Construct JSON Payload
  // Using a Raw String Literal for the template
  String payload = R"({
      "contents": [{
        "parts": [{
          "text": "{PROMPT} {userAccountDetails}"
        }]
      }],
      "tools": [
        {"googleSearch": {}},
        {"urlContext": {}}        
      ],
      "generationConfig": {
        "responseMimeType": "application/json",
        "responseJsonSchema": {
          "type": "object",
          "properties": {
            "suggested_stocks": {
              "type": "array",
              "items": {
                "type": "object",
                "properties": {
                  "symbol": { "type": "string", "description": "{SYMBOL_DESC}" },
                  "reason": { "type": "string", "description": "Brief explanation for the suggestion." },
                  "expected_hold_period_days": { "type": "integer", "description": "Suggested number of days to hold (1-7)." },
                  "confidenceLevel": { "type": "number", "description": "0.0-1.0 confidence level representing how confident you are this is a good call" }
                },
                "required": ["symbol", "reason", "expected_hold_period_days", "confidenceLevel"]
              }
            },
            "totalRecommendedAmountToInvest": { 
              "type": "number", 
              "description": "Total USD amount to allocate to these positions." 
            }
          },
          "required": ["suggested_stocks", "totalRecommendedAmountToInvest"]
        }
      }
    })";

  // Serialize Account Data
  String accountJson = account.details.serializeAccountDetails() + account.serializeAllPositionsForGemini(false, marketIsOpen()  // serialize all positinos for buy recommendations, to avoid it recommending to buy same stocks when market is closed
                       );
  accountJson.replace("\"", "\\\"");  // Escape quotes for JSON inside JSON

  // Inject Data into Payload
  payload.replace("{userAccountDetails}", " CURRENT STATUS: " + accountJson);
  payload.replace("{PROMPT}", activePrompt);
  payload.replace("{SYMBOL_DESC}", symbolDescription);


  // 5. Direct Server Connection (No HTTPClient)
  WiFiClientSecure client;
  client.setInsecure();  // Skip certificate validation for speed/simplicity
  // client.setCACert(root_ca); // Uncomment if you want strict SSL



  // Attempt connection
  if (!client.connect(host, 443)) {
    Serial.println("Connection failed!");
    return false;
  }

  Serial.println("waiting for gemini BUY suggestions...");


  // 6. Send Raw HTTP Request
  // Note: 'url' variable should contain the path (e.g., "/v1beta/models/...")
  client.println("POST " + String(url) + " HTTP/1.1");
  client.println("Host: " + String(host));
  client.println("x-goog-api-key: " + String(apiKey));  // Send API Key in Header
  client.println("Content-Type: application/json");
  client.print("Content-Length: ");
  client.println(payload.length());
  client.println("Connection: close");  // Tell server to close connection after response
  client.println();                     // Empty line denotes end of headers
  client.print(payload);                // Send Body

  // 7. Read Response with Manual Timeout
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 600000) {  // 600 Second Timeout
      Serial.println(">>> Client Timeout !");
      client.stop();
      return false;
    }
  }

  // 8. Skip HTTP Headers
  // We read line by line until we find an empty line ("\r")
  bool headersEnded = false;
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      headersEnded = true;
      break;
    }
  }

  if (!headersEnded) {
    Serial.println("Error: No valid HTTP headers found.");
    client.stop();
    return false;
  }

  // 9. Read the Body (JSON)
  // Since we sent "Connection: close", readString() will read until the server closes the connection.
  String responseBody = client.readString();
  client.stop();  // Ensure socket is closed

  Serial.println("Response received!");

  // --- FIX START: Handle Chunked Encoding / Header Garbage ---
  // The raw socket might return "43f6\r\n{...". We must skip to the first '{'.
  int jsonStartIndex = responseBody.indexOf('{');

  if (jsonStartIndex == -1) {
    Serial.println("Error: No JSON object start '{' found in response.");
    Serial.println("Raw Body Dump: " + responseBody);
    return false;
  }

  String cleanJson = responseBody.substring(jsonStartIndex);
  DynamicJsonDocument doc(65000);  // Allocate memory
  DeserializationError error = deserializeJson(doc, cleanJson);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    Serial.println("Cleaned Body Start: " + cleanJson.substring(0, 100));
  } else {
    success = true;


    // ... (The rest of your parsing logic remains exactly the same) ...
    if (doc.containsKey("candidates") && doc["candidates"][0].containsKey("content") && doc["candidates"][0]["content"]["parts"][0].containsKey("text")) {

      // ... existing parsing code ...
      String structuredText = doc["candidates"][0]["content"]["parts"][0]["text"].as<String>();

      if (structuredText.length() > 0) {
        DynamicJsonDocument innerDoc(10000);
        DeserializationError innerError = deserializeJson(innerDoc, structuredText);

        if (innerError) {
          Serial.print("Inner JSON parse failed: ");
          Serial.println(innerError.c_str());
        } else {
          geminiBuyRecommendation.recommendedAmountToInvest = innerDoc["totalRecommendedAmountToInvest"].as<float>();

          Serial.print("AI Recommends Budget: $");
          Serial.println(geminiBuyRecommendation.recommendedAmountToInvest);

          int i = 0;
          JsonArray stockArray = innerDoc["suggested_stocks"].as<JsonArray>();

          for (JsonObject suggested_item : stockArray) {
            if (i >= maxSize) break;
            geminiBuyRecommendation.recommendedStocks[i].symbol = suggested_item["symbol"].as<String>();
            geminiBuyRecommendation.recommendedStocks[i].reason = suggested_item["reason"].as<String>();
            geminiBuyRecommendation.recommendedStocks[i].expected_hold_period_days = suggested_item["expected_hold_period_days"].as<int>();
            geminiBuyRecommendation.recommendedStocks[i].confidenceLevel = suggested_item["confidenceLevel"].as<float>();
            i++;
          }
        }
      }
    }
  }

  return success;
}

bool StockBot::getGemini_SELL_StockSuggestions() {
  bool success = false;

  // 1. Account Safety Check
  if (account.updateAccount() == false) {
    Serial.println("Failed to update account. Aborting Gemini SELL request.");
    return success;
  }

  clearGeminiSellRecommendations();


  // 2. Prepare Data & Payload
  // Serialize account positions
  String serializedPositions = account.details.serializeAccountDetails() + account.serializeAllPositionsForGemini(true, marketIsOpen());
  serializedPositions.replace("\"", "\\\"");  // Escape quotes for JSON injection

  // Prepare prompt text
  String cleanPromptText = gemini_Sell_Recommendation_prompt;
  cleanPromptText.replace("\"", "\\\"");  // Escape quotes inside the prompt itself

  // Combine into final prompt string
  String finalPrompt = "CURRENT STATUS: " + serializedPositions + "\\n\\nINSTRUCTIONS: " + cleanPromptText;

  // Define JSON Schema Payload
  // Note: Using 'action' as integer (0=HOLD, 1=SELL_ALL, 2=SELL_HALF) to match your struct/enum
  String payload = R"({
      "contents": [{
        "parts": [{
          "text": "{PLACEHOLDER}"
        }]
      }],
      "tools": [
        {"googleSearch": {}},
        {"urlContext": {}}        
      ],
      "generationConfig": {
        "responseMimeType": "application/json",
        "responseJsonSchema": {
          "type": "object",
          "properties": {
            "suggested_stocks": {
              "type": "array",
              "items": {
                "type": "object",
                "properties": {
                  "symbol": { "type": "string", "description": "The asset ticker symbol. Use the exact format provided." },
                  "reason": { "type": "string", "description": "Brief explanation." },
                  "confidenceLevel": { "type": "number", "description": "0.0-1.0 confidence." },
                  "action": { "type": "integer", "description": "Action Enum: 0=HOLD, 1=SELL_ALL, 2=SELL_HALF" }
                },
                "required": ["symbol", "reason", "confidenceLevel", "action"]
              }
            },
          },
          "required": ["suggested_stocks"]
        }
      }
    })";

  payload.replace("{PLACEHOLDER}", finalPrompt);


  // 3. Direct Server Connection (Replaces HTTPClient)
  WiFiClientSecure client;
  client.setInsecure();  // Skip certificate check

  if (!client.connect(host, 443)) {
    Serial.println("Connection failed!");
    return false;
  }

  Serial.println("waiting for gemini SELL recommendations...");

  // 4. Send Raw HTTP Request
  client.println("POST " + String(url) + " HTTP/1.1");
  client.println("Host: " + String(host));
  client.println("x-goog-api-key: " + String(apiKey));
  client.println("Content-Type: application/json");
  client.print("Content-Length: ");
  client.println(payload.length());
  client.println("Connection: close");
  client.println();  // End of headers
  client.print(payload);

  // 5. Read Response with Timeout
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 600000) {  // 600s Timeout
      Serial.println(">>> Client Timeout (Sell Logic) !");
      client.stop();
      return false;
    }
  }

  // 6. Skip HTTP Headers
  bool headersEnded = false;
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      headersEnded = true;
      break;
    }
  }

  if (!headersEnded) {
    Serial.println("Error: No valid headers found.");
    client.stop();
    return false;
  }

  // 7. Read Body & Clean Garbage (Chunked Encoding Fix)
  String responseBody = client.readString();
  client.stop();  // Close socket

  Serial.println("Sell Response received!");

  // Find the first '{' to ignore hex chunk sizes (e.g. "43f6")
  int jsonStartIndex = responseBody.indexOf('{');
  if (jsonStartIndex == -1) {
    Serial.println("Error: No JSON object start '{' found.");
    return false;
  }

  String cleanJson = responseBody.substring(jsonStartIndex);

  // 8. Parse JSON
  DynamicJsonDocument doc(54576);
  DeserializationError error = deserializeJson(doc, cleanJson);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    Serial.println("Cleaned Body Start: " + cleanJson.substring(0, 100));
  } else {
    // Navigate Gemini Response Structure
    if (doc.containsKey("candidates") && doc["candidates"][0].containsKey("content") && doc["candidates"][0]["content"]["parts"][0].containsKey("text")) {

      String structuredText = doc["candidates"][0]["content"]["parts"][0]["text"].as<String>();

      if (structuredText.length() > 0) {
        DynamicJsonDocument innerDoc(10000);
        DeserializationError innerError = deserializeJson(innerDoc, structuredText);

        if (innerError) {
          Serial.print("Inner JSON parse failed: ");
          Serial.println(innerError.c_str());
        } else {
          // Success! Process the array
          success = true;

          int i = 0;
          JsonArray stockArray = innerDoc["suggested_stocks"].as<JsonArray>();

          for (JsonObject suggested_stock : stockArray) {
            if (i >= maxSize) break;

            geminiRecommendedPositionsToSell[i].symbol = suggested_stock["symbol"].as<String>();
            geminiRecommendedPositionsToSell[i].reason = suggested_stock["reason"].as<String>();
            geminiRecommendedPositionsToSell[i].confidenceLevel = suggested_stock["confidenceLevel"].as<float>();

            // Map Integer to Enum (0=HOLD, 1=SELL_ALL, 2=SELL_HALF)
            int actionInt = suggested_stock["action"].as<int>();
            geminiRecommendedPositionsToSell[i].action = static_cast<SellAction>(actionInt);

            i++;
          }
          Serial.print("Parsed ");
          Serial.print(i);
          Serial.println(" sell recommendations.");
        }
      }
    } else {
      Serial.println("No 'text' field found in Gemini response.");
    }
  }

  return success;
}


void StockBot::buyDiversifiedGeminiStockSuggestions(float dollarsToInvest, float minimumConfidenceLevelToBuy) {

  if (account.updateAccount() == false) {
    Serial.println("buy diversified aborted. failed to update account.");
  }


  Serial.println();
  Serial.print("Investing ");
  Serial.print(dollarsToInvest);
  Serial.println(" diversified in recommended stocks.");
  int numberOfValidStocks = 0;

  for (int i = 0; i < maxSize; i++) {  // count valid stocks.
    if (geminiBuyRecommendation.recommendedStocks[i].symbol != "nostock") {
      numberOfValidStocks++;
    }
  }

  if (numberOfValidStocks == 0) {
    Serial.println("No valid stocks to buy.");
    return;
  }

  int lastStockedThatWasAbleToBeBought = -1;
  float spent = 0;

  float wagerChunk = dollarsToInvest / numberOfValidStocks;

  if (wagerChunk < assetBuyMinimum) {
    wagerChunk = 20;
  }

  // 2. Iterate through the WHOLE list again
  for (int i = 0; i < maxSize; i++) {
    if (spent < dollarsToInvest && geminiBuyRecommendation.recommendedStocks[i].symbol != "nostock" && geminiBuyRecommendation.recommendedStocks[i].confidenceLevel >= minimumConfidenceLevelToBuy) {
      // *** FIX 1: Add .symbol here ***
      bool success = account.placeBuyOrder(geminiBuyRecommendation.recommendedStocks[i].symbol, wagerChunk);
      if (success) {
        spent += wagerChunk;
        lastStockedThatWasAbleToBeBought = i;
      }
    }
  }

  // 3. Handle Leftover Cash
  float remainingCash = dollarsToInvest - spent;
  if (remainingCash > 5.00 && lastStockedThatWasAbleToBeBought >= 0) {
    Serial.print("Attempting to spend remainder: $");
    Serial.println(remainingCash);
    bool success = account.placeBuyOrder(geminiBuyRecommendation.recommendedStocks[lastStockedThatWasAbleToBeBought].symbol, remainingCash);
    if (success) {
      spent += remainingCash;
    } else {
      Serial.println("Could not spend remainder.");
    }
  }
}

void StockBot::buyFirstGeminiStockSuggestion(float dollarsToInvest) {

  if (account.updateAccount() == false) {
    Serial.println("buy first stock aborted. failed to update account.");
  }

  if (dollarsToInvest < assetBuyMinimum) {
    Serial.print(dollarsToInvest);
    Serial.print(" is less than the minimum invest amount of $");
    Serial.print(assetBuyMinimum);
    Serial.println(". Aborting buy first suggestion.");
  }

  bool noRecommendations = true;

  for (int i = 0; i < maxSize; i++) {
    if (geminiBuyRecommendation.recommendedStocks[i].symbol != "nostock") {
      noRecommendations = false;
    }
  }

  if (noRecommendations) {
    Serial.println("No gemini buy recommendations. Aborting buy first gemini recommendation.");
    return;
  }



  for (int i = 0; i < maxSize; i++) {
    if (geminiBuyRecommendation.recommendedStocks[i].symbol == "nostock") {
      continue;
    }
    bool success = account.placeBuyOrder(geminiBuyRecommendation.recommendedStocks[i].symbol, dollarsToInvest);
    if (success) {
      Serial.println("");
      Serial.print("BOUGHT ");
      Serial.print(dollarsToInvest);
      Serial.print(" of ");
      Serial.println(geminiBuyRecommendation.recommendedStocks[i].symbol);
      Serial.print("Recommended to hold ");
      Serial.print(geminiBuyRecommendation.recommendedStocks[i].expected_hold_period_days);
      Serial.println(" days");
      Serial.print(geminiBuyRecommendation.recommendedStocks[i].confidenceLevel);
      Serial.println(" confident");
      Serial.println(geminiBuyRecommendation.recommendedStocks[i].reason);
      return;
    } else {
      Serial.println("");
      Serial.print("FAILED TO BUY ");
      Serial.println(geminiBuyRecommendation.recommendedStocks[i].symbol);
      Serial.println("Trying another recommendation instead...");
    }
  }
}

const char* harleyPersonality = "Harley is a risk taker. he wants to make big money.";
const char* jarvisPersonality = "Jarvis is very anaytical and seeks to provide the most resonable answers as objectivetly as possible.";
const char* tonyStarkPersonality = "Toney Stark is charming also very inteligent. he strikes in the middle of being a risk taker, with also very logical and collected.";
const char* counselMeetingPrompt = "You are the judge to a cousel of 3 people. all 3 people will give their cases as to whether or not they they think the cousel should buy a given symbol to make a over net profit in the day trade. You are also to decide on the next date the cousel will reassemble to discuss the position or next day trade buy.";

// test data to replace real 'serializedPositions' with in 'fetchGemini_sellRecommendations' if want to to test what it would do in cetain snarios.
// String serializedPositions = R"([
//   {
//     "symbol": "CRASH_INC",
//     "pricePaid": 100.00,
//     "currentUnitPrice": 88.00,
//     "marketValue": 880.00,
//     "percentChange": -0.12,
//     "percentChangeToday": -0.12,
//     "totalProfit_dollars": -120.00,
//     "qty_available": 10.0
//   },
//   {
//     "symbol": "MOON_CORP",
//     "pricePaid": 50.00,
//     "currentUnitPrice": 62.50,
//     "marketValue": 1250.00,
//     "percentChange": 0.25,
//     "percentChangeToday": 0.05,
//     "totalProfit_dollars": 250.00,
//     "qty_available": 20.0
//   },
//   {
//     "symbol": "BORING_LTD",
//     "pricePaid": 10.00,
//     "currentUnitPrice": 10.05,
//     "marketValue": 1005.00,
//     "percentChange": 0.005,
//     "percentChangeToday": 0.001,
//     "totalProfit_dollars": 5.00,
//     "qty_available": 100.0
//   }
// ])";
