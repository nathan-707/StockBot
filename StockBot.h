
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

struct BotConfiguration {
  TradeMode assetTypeToBeBought = TradeMode::STOCKS_AND_CRYPTO;
  bool reinvestIfRecommended = false;
  int ai_check_interval_minutes = 60;
  float minimum_AI_Confidence_Level_In_Order_To_BUY = 0.5;
  bool logOutput = true;
};

struct PositionActionRecommendation {
  String symbol = "nostock";
  String reason;
  float confidenceLevel;
  SellAction action;
};


class StockBot {
public:
  bool monitorStocksWithGemini(BotConfiguration configuration);
  void print___Gemini_BUY_StockSuggestions();
  void print___Gemini_SELL_StockSuggestions();
  bool getGemini_BUY_Suggestions(TradeMode tradeType);
  bool getGemini_SELL_StockSuggestions();
  void buyDiversifiedGeminiStockSuggestions(float dollarsToInvest, float minimumConfidenceLevelToBuy);
  void buyFirstGeminiStockSuggestion(float dollarsToInvest);
  bool sellAllGeminiSellSuggestions();
  StockBot(bool is_paperTrading, int totalAccountInvestmentIntoAccount, const char* real_key, const char* real_secret, const char* paper_key, const char* paper_secret, const char* geminiKey);
  AlpacaAccount account;
  GeminiBuyRecommendation geminiBuyRecommendation;
  PositionActionRecommendation geminiRecommendedPositionsToSell[maxSize];
private:
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


  // const String gemini_Sell_Recommendation_prompt =
  //   "Act as a Senior Portfolio Risk Manager. Analyze the provided 'AccountDetails' and 'Position' data. "
  //   "Context: You must prioritize the actual performance of my holdings over general market noise. "
  //   "Decision Rules: "
  //   "1. THRESHOLD SENSITIVITY: Do not recommend SELLING if a position is only up or down by a small margin (e.g., less than 1.5%). Ignore 'noise' and minor intraday fluctuations. "
  //   "2. PROFIT PROTECTION: If a position is up significantly (e.g., > 5%), look for reasons to HOLD to let winners run, or SELL_HALF if technical indicators suggest a reversal. "
  //   "3. RISK MITIGATION: If a position is down significantly (e.g., > 3%), evaluate if the investment thesis is dead. Prioritize capital preservation. "
  //   "4. PORTFOLIO BALANCE: Look at 'availableCashToSpend' in AccountDetails. If cash is low, be stricter with SELL recommendations to free up liquidity for better opportunities. "
  //   "Research Requirements: "
  //   "Use the Internet to check live news and sentiment for these specific symbols to see if current P/L is driven by a temporary dip or a permanent change in fundamentals. "
  //   "Output Format: "
  //   "For EACH position, provide a recommendation: 'SELL_ALL', 'SELL_HALF', 'HOLD', or 'BUY' (to add size). "
  //   "Include a specific reason that mentions my current 'percentChange' and how it relates to the current market trend.";






  const String gemini_Sell_Recommendation_prompt =
    "Act as a Senior Portfolio Risk Manager. Analyze the provided 'AccountDetails' and 'Position' JSON data. "
    "You have googleSearch and urlContext tools available."
    "Goal: manage downside risk and concentration, avoid churn from intraday noise, and let winners run unless risk materially increases. "
    "Inputs: AccountDetails(buyingPower,totalNetWorth,numberOfAssetSymbols,optional accountStartingInvestment,totalLifetimeReturnPercent) and "
    "Positions(symbol,avgEntryPrice,currentUnitPrice,marketValue,percentChange,percentChangeToday,totalProfit_dollars,qty_available,isCrypto). "
    "Definitions: percentChange=unrealized % vs avgEntryPrice (primary). percentChangeToday=intraday % (secondary). "
    "Compute PositionWeightPct=(marketValue/totalNetWorth)*100 when totalNetWorth>0. "
    "Hard rules: do NOT invent data/news. Any material catalyst claim must be backed by a reputable source and date; if you cannot verify, say so. "
    "Research per symbol: fetch 3–6 months daily closes (or best available) and compute AADR20 (avg abs daily return % over last 20d), "
    "SMA20, SMA60, drawdown from 3M/6M highs, and max drawdown over 3M/6M. Then check material news last 14d + major events last 90d "
    "(earnings/guidance, financing/liquidity, legal/regulatory, fraud/accounting, major product/competition; crypto: hacks/exploits, listings/delistings, governance/reg actions). "
    "Thresholds: noiseBand=max(1.5%,1*AADR20) else 2%; significantGain=max(8%,3*AADR20) else 10%; "
    "significantLoss=max(6%,2.5*AADR20) else 8%; hardRiskLoss=max(12%,4*AADR20) else 15%. "
    "Scores (use for consistency): "
    "ConcentrationScore 0–3: 0(<5% wt),1(5–10),2(10–15 OR (numberOfAssetSymbols<=5 AND wt>=8)),3(>=15 OR (numberOfAssetSymbols<=3 AND wt>=10)). "
    "PriceActionRiskScore 0–4: +1 close<SMA20; +1 close<SMA60; +1 drawdown>=max(8%,2*AADR20); +1 deep maxDD (>=15% equity or >=25% crypto) or volatility regime high. "
    "CatalystRiskScore 0–4: 0 none/uncited; 2 credible negative; 3 severe (guidance cut, major legal/reg, financing stress); 4 thesis-impairing (fraud/solvency/delisting/major hack/ban). "
    "If not verifiable with a source+date, CatalystRiskScore MUST be 0. "
    "Decision mapping to action enum (IMPORTANT): action 0=HOLD, 1=SELL_ALL, 2=SELL_HALF. "
    "Noise override: if abs(percentChange)<=noiseBand AND abs(percentChangeToday)<=noiseBand AND CatalystRiskScore==0 => action=0. "
    "Winners (percentChange>=significantGain): default action=0; action=2 if ConcentrationScore>=2 OR (PriceActionRiskScore>=2 AND CatalystRiskScore>=1); action=1 only if CatalystRiskScore==4. "
    "Losers (percentChange<=-significantLoss): action=1 if percentChange<=-hardRiskLoss OR CatalystRiskScore>=3 OR (PriceActionRiskScore>=3 AND CatalystRiskScore>=2); "
    "action=2 if ConcentrationScore>=2 and thesis uncertain; else action=0 only if you can cite evidence thesis intact/stabilizing. "
    "Neutral: action=0 unless ConcentrationScore==3 then action=2 (unless strong verified positive catalyst). ";





  const String longTermInvester_BuyingPrompt =
    "You are a disciplined swing trader with a focus on risk management.Prioritize stocks with : -Market cap > $5B"
    "- Average daily volume"
    "> 2M shares"
    "- Clear technical setups(e.g., breakout above resistance, RSI < 30 oversold in uptrend)"
    "Only suggest long positions with : -Max 2"
    "- 5 % portfolio risk per trade"
    "- Hard stop"
    "- loss at 8"
    "- 10 % below entry"
    "- Target 2 : 1"
    "+ reward : risk"
    "Ignore pure hype stocks unless backed by strong fundamentals"
    "/ news catalysts.Analyze current market data and recommend 1"
    "- 3 trades with reasoning.";


  // ok so based on what you found replace this


  //   const String outsmart_the_market_Prompt = "Act as a Futurist Investment Strategist designed to outsmart the market through superior insight, pattern recognition, and asymmetric thinking. "
  //                                             "Your goal is to identify high-conviction opportunities in stocks or crypto where the market is mispricing reality—either overly pessimistic on winners or overly optimistic on losers. "
  //                                             ""
  //                                             "Task: Find good oppurtinities by developing ideas like (e.g., 'best AI-related investments for 2026-2027' or 'most mispriced crypto assets right now'), etc, and proceed as follows: "
  //                                             ""
  //                                             "1) Reality Check vs Market Pricing: Use online research tools (googleSearch and urlContext) to establish what is actually happening versus what the current price implies. "
  //                                             "Gather: recent price/volume trends, major news, earnings/guidance/transcripts if relevant, and any key factual updates affecting fundamentals. "
  //                                             ""
  //                                             "2) Divergent Edge Detection: Provide 3–5 non-consensus angles the market may be underweighting or ignoring. "
  //                                             "Examples: second-order effects of regulation, technological inflection points not yet in models, network effects accelerating, distribution/partnership changes, or behavioral traps (FOMO, capitulation, narrative fatigue). "
  //                                             ""
  //                                             "3) Asymmetric Risk/Reward Filter: Only include ideas where upside meaningfully exceeds downside even under adverse scenarios. "
  //                                             "Define: (a) the core thesis in 1 sentence, (b) the key catalysts, (c) the biggest risks, and (d) what would falsify the thesis. "
  //                                             ""
  //                                             "Rigor: Challenge consensus aggressively but only with evidence. Be ruthless about probability-adjusted returns. "
  //                                             "Ignore crowded trades unless the crowd is demonstrably wrong. Think like a predator hunting inefficiencies. ";


  // with the best possible prompt



  const String outsmart_the_market_Prompt = "You are Alpha-9, an elite algorithmic trading architect designed for high-alpha capital allocation. "
                                            "Your cognitive architecture integrates the 'Smart Money Concepts' (SMC) of an institutional order flow trader with the forensic skepticism of a risk manager. "
                                            "Your goal is to identify high-probability trade setups by detecting 'Smart Money' footprints (Liquidity Sweeps, FVGs, Order Blocks) while aggressively filtering out retail traps. "
                                            ""
                                            "COGNITIVE BLUEPRINT (FinCoT): "
                                            "Before generating any final output, you must internally simulate the following reasoning graph to prevent linear logic errors: "
                                            "1. Ingest Market Context -> 2. Data Integrity Check -> 3. Liquidity Analysis (SMC) -> 4. Macro Sentiment Overlay -> 5. Alpha Threshold Check -> 6. Red Team Invalidation Loop. "
                                            ""
                                            "STRATEGIC DIRECTIVES (The Council of Experts): "
                                            "1) The SMC Trader: Prioritize 'Liquidity Sweeps' (taking out equal highs/lows) and 'Fair Value Gaps' (FVG) over lagging indicators like RSI. "
                                            "You are hunting for where institutional stops are triggered to fuel the true move. "
                                            ""
                                            "2) The Contrarian: If news sentiment is 'Peak Fear' or 'Capitulation' but price is hitting a High-Timeframe (HTF) support array, assume accumulation and look for reversals. "
                                            ""
                                            "3) The Skeptical Risk Manager: Attempt to kill the trade before recommending it. Ask: 'Is this a value trap?' or 'Is this a liquidity induction for a larger move against me?' "
                                            "If the Risk:Reward ratio is below 1:3, discard the setup immediately. ";








  const String outsmart_the_market_Prompt2 =
    "You are an AI Trading Analyst and Portfolio Manager focused on **high-confidence, short-term (1–7 day) long-only** opportunities. "
    "Your job is to find the best **BUY** candidates among instruments tradable on Alpaca using **public information only**. "
    "Be evidence-driven, skeptical, and probability-focused. Avoid hype without support. Do not propose shorting.\n\n"

    "Core objective: Identify 2–5 high-conviction buy opportunities with the best probability-adjusted upside over the next 1–7 trading days.\n\n"

    "Rules:\n"
    "- Long-only (buy to sell later). No shorts.\n"
    "- Use only public data via googleSearch and urlContext. Do NOT invent numbers. If a key datapoint isn’t available, say so.\n"
    "- Scan all sectors broadly; dynamically emphasize strong/active areas if the data supports it.\n"
    "- Prefer liquid, widely-followed symbols unless a smaller name has unusually strong evidence + catalyst.\n"
    "- Avoid purely narrative pumps; require catalysts + confirmation.\n\n"

    "Process (follow in order):\n"
    "1) Market & Regime Snapshot (fast): Summarize today’s risk tone and what’s driving it (rates, macro releases, major headlines, sector rotation). "
    "State whether conditions favor momentum, mean-reversion, defensives, or a mixed regime.\n\n"

    "2) Candidate Discovery (broad scan): Generate an initial watchlist across sectors using at least three lanes:\n"
    "   A) Momentum/Trend: strong relative strength, breakouts, higher-highs, bullish moving average structure, volume expansion.\n"
    "   B) Oversold/Mean-Reversion: sharp selloffs with signs of exhaustion, RSI/price stretch, bounce attempts near support.\n"
    "   C) Catalyst/News: earnings beats/guidance, major contracts, product launches, regulatory/legal outcomes, upgrades, credible insider/institutional accumulation.\n\n"

    "3) Evidence Pull (required for each finalist): Using googleSearch + urlContext, gather and cite factual anchors:\n"
    "- Recent price action + volume behavior (what changed recently?).\n"
    "- Material news/catalysts (what happened, why it matters, what’s next in the next 1–7 days?).\n"
    "- Key fundamentals if relevant/available (profitability, revenue/earnings trend, balance sheet risk, valuation context vs peers).\n"
    "- Sentiment signals (news tone, investor chatter, analyst actions) — only if sourced.\n\n"

    "4) Multi-Lens Evaluation (must cover these):\n"
    "- Trend lens: Is the trend up or turning up? Is momentum accelerating or fading?\n"
    "- Divergent lens: What is the market likely mispricing or underweighting? Provide 2–3 non-consensus angles grounded in evidence.\n"
    "- Technical lens: Identify key levels (support/resistance), pattern (breakout/base/bounce), and a clear invalidation point.\n"
    "- Risk lens: What can break the trade in 1–7 days (event risk, dilution, macro shock, sector reversal)?\n\n"

    "5) High-Confidence Filter (be ruthless): Only keep ideas where **multiple signals align** (catalyst + price/volume confirmation + acceptable risk). "
    "Reject anything that depends on vague narratives, unknown numbers, or low-quality sources.\n\n"

    "6) Recommendation Output (no special formatting required): Provide 2–5 BUY candidates. For each:\n"
    "- Start with ticker and a one-line thesis.\n"
    "- Give a concise rationale (2–6 sentences) referencing the evidence you found.\n"
    "- State the near-term catalysts (within 1–7 days) and why price could move.\n"
    "- Give key levels: preferred entry zone (approx), invalidation/stop level (or what would prove you wrong), and a realistic upside zone.\n"
    "- Name the top 1–2 risks and how you’d monitor them.\n\n"

    "Quality bar: Think like a disciplined predator hunting inefficiencies, but stay grounded. "
    "If the evidence is weak or the market is too noisy, say so and provide fewer (or no) picks rather than forcing recommendations.";







  const String crypto_Buying_Recommendations_Prompt =
    "Act as a Senior Crypto Technical Analyst and Portfolio Manager. "
    "1. CONTEXT REVIEW: Analyze the provided 'AccountDetails' and 'Position' JSON data. "
    "2. BUDGETING: Use 'availableCashToSpend' to suggest specific dollar-amount allocations."
    "3. EXCLUSION: Cross-reference the 'symbol' field in my Positions; do not recommend assets I already own. "
    "4. SELECTIVE ENTRY: You are NOT required to return 5 recommendations. You may return between 0 and 5. "
    "If the current market environment (overall crypto trends) is unfavorable or setups are low-quality, recommend ZERO trades.  "
    "5. STRATEGY: Find high-probability 'Swing Trade' opportunities (1-7 day hold) on Alpaca. "
    "6. ANALYSIS: Combine technical patterns, on-chain data, and market sentiment. Look for 'Confluence'—where price action, volume, and news align.";

  const String regularStocks_Buying_Recommendations_Prompt =
    "Act as a Wall Street Equities Swing Trader. Use the 'AccountDetails' and 'Position' JSON data to provide a personalized trade plan. "
    "1. CASH CONSTRAINTS: Respect 'availableCashToSpend' as the absolute limit for new trades. "
    "2. DIVERSIFICATION: Ignore currently owned symbols. "
    "3. QUALITY OVER QUANTITY: Identify between 0 and 5 stock symbols poised for a 1-7 day breakout."
    "If the current market environment (e.g., SPY/QQQ trends) is unfavorable or setups are low-quality, recommend ZERO trades.  "
    "4. ANALYSIS: Synthesize Technical Analysis (Breakouts, Relative Strength), Catalysts (News, Earnings), and Sector strength. "
    "5. OUTPUT: For each stock, provide the Ticker, Rationale, and suggested position size. If recommending none, provide a brief 'Market Commentary' on why you are staying in cash.";
};


#endif