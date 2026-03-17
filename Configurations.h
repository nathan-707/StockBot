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

// enum class TradeMode {
//   STOCKS_AND_CRYPTO,  // 0
//   STOCKS_ONLY,        // 1
//   CRYPTO_ONLY         // 2
// };
// enum class TradeRoutine {
//   Routine_1,
// }
// struct BotConfiguration {
//   TradeMode assetTypeToBeBought = TradeMode::STOCKS_AND_CRYPTO;
//   bool reinvestIfRecommended = false;
//   int ai_check_interval_HOURS = 2;
//   float minimum_AI_Confidence_Level_In_Order_To_BUY = 0.5;
//   bool logOutput = true;
//   TradeRoutine routine = TradeRoutine::Routine_1;
//   String buying_prompt_stocks;
//   String buying_prompt_crypto;
//   String selling_prompt;
//   String name = "";
// }


// TEMPLATE BEGIN:
BotConfiguration templateConfig = BotConfiguration(
  /*TradeMode enum, assets that can be bought. options are 'STOCKS_AND_CRYPTO', 'STOCKS_ONLY', 'CRYPTO_ONLY'*/ TradeMode::STOCKS_AND_CRYPTO,
  /*bool, indicates whether or not config should reinvest autonomous or only sell.*/ true,
  /*int, number of hours to wait to call ai for buy and sell recommendations*/ 3,
  /*float, minimum confidence ai must have in order to buy*/ 0.5,
  /*bool that determines to log output.*/ true,
  /*TradeRoutine enum, only use Routine_1*/ TradeRoutine::Routine_1,
  /*String, prompt used to find good regular stocks to buy. not used if crypto only.*/ "ai, what are good stocks to buy?",
  /*String, prompt used to find good crypto to buy. not used if stocks only.*/ "ai, what is good crypto to buy?",
  /*String, prompt used to find out what are good current positions you have to sell.*/ "ai, which of my positions should i sell?",
  /*String, name of the config. explains what the idea of its strategy is.*/ "Make Money Config");
  // END OF TEMPLATE.



// Create different bot configs here.
BotConfiguration longTermBotConfiguration = BotConfiguration(TradeMode::STOCKS_ONLY, true, oneHour, defaultConfidenceLevel, alwaysLogOutput, TradeRoutine::Routine_1, buying_prompt_longterm, buying_prompt_longterm, gemini_Sell_Recommendation_prompt, "Long Term Config");


BotConfiguration cryptoScalperConfig = BotConfiguration(
  TradeMode::CRYPTO_ONLY,
  true,
  2,
  0.80,
  true,
  TradeRoutine::Routine_1,
  "",  // Not used for Crypto mode
  "Analyze the 15-minute candle charts for high-volume breakouts. Buy only if the RSI is below 30 and moving upward, or if there is a clear bullish engulfing pattern on the 1-hour chart.",
  "Sell immediately if the price drops 2% from entry (Stop Loss) or if the profit reaches 5% (Take Profit). Use trailing stops if possible.",
  "Crypto Scalper");


  BotConfiguration blueChipConfig = BotConfiguration(
  TradeMode::STOCKS_ONLY, 
  true, 
  2, 
  0.70, 
  true, 
  TradeRoutine::Routine_1, 
  "Look for S&P 500 companies with a P/E ratio below the 5-year average and a dividend yield above 3%. Prioritize companies with a positive cash flow and low debt-to-equity ratios.", 
  "", // Not used for Stocks mode
  "Sell only if the company cuts its dividend, or if the stock becomes overvalued by more than 40% based on fundamental analysis.", 

  "Dividend Accumulator"
);



BotConfiguration dipBuyerConfig = BotConfiguration(
  TradeMode::STOCKS_AND_CRYPTO, 
  false, 
  2, 
  0.45, 
  true, 
  TradeRoutine::Routine_1, 
  "Identify high-quality tech stocks that have dropped more than 10% in the last week without negative fundamental news.", 
  "Look for 'Fear and Greed Index' levels below 20. Buy major assets like BTC or ETH during sharp liquidations where the price is significantly below the 200-day EMA.", 
  "Sell when the asset recovers to the 50-day moving average or after a 15% bounce from the purchase price.", 
  "Contrarian Dip Buyer"
);


BotConfiguration momentumConfig = BotConfiguration(
  TradeMode::STOCKS_AND_CRYPTO, 
  true, 
  2, 
  0.65, 
  true, 
  TradeRoutine::Routine_1, 
  "Search for stocks hitting new 52-week highs on increasing volume. Buy if the 50-day moving average has recently crossed above the 200-day moving average (Golden Cross).", 
  "Identify crypto assets in the top 50 by market cap that have increased 5% in the last 24 hours. Enter position if the trend shows sustained buying pressure.", 
  "Sell if the price closes below the 20-day exponential moving average (EMA) or if volume begins to trend downward significantly.", 
  "Trend Follower"
);


BotConfiguration contrarianFearBuyer = BotConfiguration(
  TradeMode::STOCKS_ONLY, 
  false,                        // reinvestIfRecommended: No—keep cash liquid for the next crash
  2,                            // ai_check_interval_HOURS: Checks 3x a day to catch sentiment shifts
  0.50,                         // minimum_AI_Confidence_Level: Allows entry on "blood in the streets" signals
  true,                         // logOutput
  TradeRoutine::Routine_1, 
  "IGNORE all 'Breaking News' about new all-time highs. Your mission is to find stocks where the RSI is below 30 and the 'Fear & Greed Index' is under 40. Look for high-quality companies (S&P 500) being dumped due to macro-panic or temporary earnings misses. If Jim Cramer or mainstream media says 'Get out now,' analyze the support levels near 6,800 and look for an entry point.", 
  "",                           // buying_prompt_crypto: (Not used)
  "Sell strictly when the RSI crosses above 70 or when the 'Fear & Greed Index' hits 'Extreme Greed' (above 80). If the crowd is euphoric and mainstream media is celebrating a 'new era of growth,' exit the position and move to cash.", 
  "Contrarian Fear Buyer"
);











#endif