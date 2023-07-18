#include <iostream>
#include <chrono>
#include "vwap.h"

int main() {
    try {
        // Starting execution time
        auto start = std::chrono::high_resolution_clock::now();

        const char* file_path = "01302019.NASDAQ_ITCH50";
        readAndParseItchData(file_path);

        // Ending execution time
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        double durationMinutes = duration.count() / 60.0;
        std::cout << "Execution time: " << durationMinutes << " minutes " << std::endl;
        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "Exception occurred: " << ex.what() << std::endl;
        return 1;
    }
}
