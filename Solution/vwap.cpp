#include "vwap.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <arpa/inet.h> 
#include <fcntl.h>   
#include <unistd.h>  
#include <sys/mman.h> 

//std::unordered_map<LocateId, Symbol> id2Symbol;
std::unordered_map<OrderId, Symbol> OrderId2Symbol;
std::unordered_map<OrderId, Shares> OrderId2Shares;
SymbolHrVwap symbolHrVwap;
SymbolHrCum symbolHrCum;

void updateVWAP(const Trade& trade)
{  
    int hour = trade.timestamp / HOUR;
    Symbol symbol = trade.symbol;
    uint64_t& cumulativeVolume  = symbolHrCum[symbol][hour].first;
    uint64_t& cumulativePriceVolume = symbolHrCum[symbol][hour].second;

    cumulativeVolume += trade.volume;
    cumulativePriceVolume += (uint64_t)trade.price * (uint64_t)trade.volume;

    symbolHrCum[symbol][hour] = std::make_pair(cumulativeVolume, cumulativePriceVolume);
    symbolHrVwap[symbol][hour] = static_cast<double>(cumulativePriceVolume)/(cumulativeVolume*divisor);     
}

void processFinalVWAP(const SymbolHrVwap& symbolHrVwap)
{
    std::ofstream outputFile("outputVwap.txt");
    if (!outputFile.is_open()) {
        std::cerr << "Failed to open the output file. Error: " << strerror(errno) << std::endl;
        return;
    }

    std::stringstream outputContent;  // Create a stringstream for building the output content

    for (const auto& symbolHourVwap : symbolHrVwap) {
        Symbol symbol = symbolHourVwap.first;
        HrVwap hrVwap = symbolHourVwap.second;
        outputContent << "Stock: " << symbol << "\n";
        
        for (const auto& hourData : hrVwap) {
            int hour = hourData.first;
            double vwapValue = hourData.second;

            // Format the VWAP value with 4 decimal places using stringstream
            std::stringstream vwapStream;
            vwapStream.str("");  // Clear the stringstream
            vwapStream.str().reserve(10);  // Preallocate buffer with an estimated size
            vwapStream << std::fixed << std::setprecision(4) << vwapValue;
            std::string formattedVwap = vwapStream.str();

            outputContent << "Hour " << hour << ": VWAP = " << formattedVwap << "\n";      
        } 

        outputContent << "\n";
    }

    outputFile << outputContent.str();  // Write the output content to the file
    outputFile.close();
}

Symbol decodeStockSymbol(const char* buffer) {
    Symbol symbol(buffer, 8);
    size_t pos = symbol.find(' ');  // Find the first space character
    if (pos != std::string::npos) {
        symbol = symbol.substr(0, pos);  
    }
    return symbol;
}  

Shares decodeVolume(const char* buffer) {
    return  ntohl(*reinterpret_cast<const uint32_t*>(buffer));
}

Price_t decodePrice(const char* buffer){
    Price_t rawPrice = 0;
    for (int i = 0; i < 4; ++i) {
        rawPrice = (rawPrice << 8) | static_cast<uint8_t>(buffer[i]);
    }
    return rawPrice;
}

uint64_t decodeTimestamp(const char* buffer) {
    uint64_t timestamp = 0;
    for (int i = 0; i < 6; ++i){
        timestamp = (timestamp << 8) | static_cast<uint8_t>(buffer[i]);  
    }
    return timestamp;
}

OrderId decodeReferenceId(const char* buffer) {
    OrderId ref_id = 0;
    for (int i = 0; i < 8; ++i){
        ref_id = (ref_id << 8) | static_cast<uint8_t>(buffer[i]);  
    }
    return ref_id;
}

Shares decodeShares(const char* buffer) {
    Shares shares = 0;
    for (int i = 0; i < 4; ++i){
        shares = (shares << 8) | static_cast<uint8_t>(buffer[i]);  
    }
    return shares;
}

LocateId decodeLocateId(const char* message)
{
    return (static_cast<uint16_t>(message[1]) << 8) | static_cast<uint16_t>(message[2]);
}

void initializeVWAPCalculation() {
    //id2Symbol.clear();
    symbolHrVwap.clear();
    symbolHrCum.clear();
    OrderId2Symbol.clear();
    OrderId2Shares.clear();

}

void handleSystemEventMessage(const char* message) {
    const char* eventCode = message + 11;
    if (*eventCode == 'O') {
        std::cout<<"Trading session starts"<<std::endl;
        initializeVWAPCalculation();
    } else if (*eventCode == 'C') {
        processFinalVWAP(symbolHrVwap);
        std::cout<<"Trading session Ends"<<std::endl;
    }
}

void processStockDirectoryMessage(const char* message)
{
    LocateId locateId = decodeLocateId(message);
    Symbol symbol = decodeStockSymbol(message + 11);
    //id2Symbol[locateId] = symbol;
}

void processOrderExecutedMessagePrice(const char* message)
{
    static Trade trade;
    LocateId locateId = decodeLocateId(message);
    trade.timestamp = decodeTimestamp(message + 5);
    trade.volume = decodeVolume(message + 19);
    trade.price = decodePrice(message + 32);

    OrderId ref_id = decodeReferenceId(message+11);
    trade.symbol = OrderId2Symbol[ref_id];
    //id2Symbol[locateId] = trade.symbol;

    //Running Vwap update
    updateVWAP(trade);

}

void processTradeMessageNonCross(const char* message)
{
    static Trade trade;
    LocateId locateId = decodeLocateId(message);
    trade.timestamp = decodeTimestamp(message + 5);
    trade.volume = decodeVolume(message + 20);
    trade.price = decodePrice(message + 32);
    trade.symbol = decodeStockSymbol(message + 24);
    //id2Symbol[locateId] = trade.symbol;

    //Running Vwap update
    updateVWAP(trade);
}   

//Considering AddOrder, Cancel Order, Delete Order,
//Replace Order message to maintain
//Order Reference number to Symbol mapping
//This ll help in finding Symbol using Order Reference number
//during the process of Order executed with price message
void processAddOrderMessage(const char* message)
{
    OrderId ref_id = decodeReferenceId(message+11);
    OrderId2Symbol[ref_id] = decodeStockSymbol(message+24);
    OrderId2Shares[ref_id] = decodeShares(message+20);
}

void processOrderCancelMessage(const char* message)
{
    OrderId ref_id = decodeReferenceId(message+11);
    OrderId2Shares[ref_id] -= decodeShares(message+19); 
    if(OrderId2Shares[ref_id] <= 0)
    {
        OrderId2Shares.erase(ref_id);
        OrderId2Symbol.erase(ref_id);
    }
}

void processOrderDeleteMessage(const char* message)
{
    OrderId ref_id = decodeReferenceId(message+11);
    OrderId2Shares.erase(ref_id);
    OrderId2Symbol.erase(ref_id);
}

void processOrderReplaceMessage(const char* message)
{
    OrderId original_ref_id = decodeReferenceId(message+11);
    OrderId new_ref_id = decodeReferenceId(message+19);
    OrderId2Symbol[new_ref_id] = OrderId2Symbol[original_ref_id];
    OrderId2Symbol.erase(original_ref_id);
    OrderId2Shares.erase(original_ref_id);
    OrderId2Shares[new_ref_id] = decodeShares(message+27);
    
}

void processITCHMessage(const char* message, const char* end) {
    while (message + 2 <= end) {
        uint16_t msg_size = (static_cast<uint16_t>(message[0]) << 8) | static_cast<uint16_t>(message[1]);
        message += 2;
        if (message + msg_size > end) {
            std::cerr << "Incomplete message. Error: " << strerror(errno)<< std::endl;
            break;
        }
        char messageType = message[0];
        switch (messageType) {
            case 'S':
                // System Event Message
                handleSystemEventMessage(message); 
                break;
            case 'R':
                // Stock Directory Message
                //processStockDirectoryMessage(message);
                break;
            case 'C':
                // Order Executed Message with price
                processOrderExecutedMessagePrice(message);
                break;
            case 'P':
                // Trade Message (Non-Cross)
                processTradeMessageNonCross(message);
                break;
            case 'A':
                //Add Order – No MPID Attribution
                processAddOrderMessage(message);
                break;
            case 'F':
                //Add Order with MPID Attribution
                 processAddOrderMessage(message);
                break;
            case 'X':
                //Order Cancel Message
                processOrderCancelMessage(message);
                break;
            case 'D':
                //Order Delete Message
                processOrderDeleteMessage(message);
                break;
            case 'U':
                //Order Replace Message
                processOrderReplaceMessage(message);
                break;
            default:
                break;
        }
        message += msg_size;
    }
}

void readAndParseItchData(const char* file_path)
{
    int file_descriptor = open(file_path, O_RDONLY | O_NOATIME);
    if (file_descriptor == -1) {
        std::cerr << "Failed to open the file. Error: " << strerror(errno)<< std::endl;
        return;
    }

    off_t file_size = lseek(file_descriptor, 0, SEEK_END);
    off_t read_size = static_cast<off_t>(file_size);
    lseek(file_descriptor, 0, SEEK_SET);

    void* file_memory = mmap(nullptr, read_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, file_descriptor, 0);
    if (file_memory == MAP_FAILED) {
        std::cerr << "Failed to map the file into memory. Error: " << strerror(errno)<< std::endl;
        close(file_descriptor);
        return;
    }

    char* file_data = static_cast<char*>(file_memory); 
    processITCHMessage(file_data, file_data + read_size);

    int munmapResult = munmap(file_memory, read_size);
    if (munmapResult == -1) {
        std::cerr << "Failed to unmap file memory. Error: " << strerror(errno) << std::endl;
        return;
    }
    
    close(file_descriptor);
}

