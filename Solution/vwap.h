#pragma once

#include <unordered_map>
#include <cstdint>
#include <cmath>
#include <map>
#include <string>

// =========================== Used Types ====================================== 

static const uint64_t HOUR = 3600000000000LL; //hour in nanosecond
const double divisor = std::pow(10.0, 4);// precision for price(4)

using LocateId = uint16_t;
using Symbol = std::string;
using Price_t = uint32_t;
using OrderId = uint64_t;
using Shares = uint32_t;

struct Trade {
    uint64_t timestamp;
    uint32_t price;
    uint32_t volume;
    std::string symbol;
};

using HourlyCumData = std::map<int, std::pair<uint64_t, uint64_t>>;
using SymbolHrCum = std::map<Symbol, HourlyCumData>;
using HrVwap = std::map<int, double>;
using SymbolHrVwap = std::map<Symbol, HrVwap>;

// =========================== Used Functions ======================================

void updateVWAP(const Trade& trade);
void processFinalVWAP(const SymbolHrVwap& symbolHrVwap);
Symbol decodeStockSymbol(const char* buffer);
Shares decodeVolume(const char* buffer);
Price_t decodePrice(const char* buffer);
uint64_t decodeTimestamp(const char* buffer);
OrderId decodeReferenceId(const char* buffer);
Shares decodeShares(const char* buffer);
LocateId decodeLocateId(const char* message);
void initializeVWAPCalculation();
void handleSystemEventMessage(const char* message);
void processStockDirectoryMessage(const char* message);
void processOrderExecutedMessagePrice(const char* message);
void processTradeMessageNonCross(const char* message);
void processAddOrderMessage(const char* message);
void processOrderCancelMessage(const char* message);
void processOrderDeleteMessage(const char* message);
void processOrderReplaceMessage(const char* message);
void processITCHMessage(const char* message, const char* end);
void readAndParseItchData(const char* file_path);
