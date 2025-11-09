#include "zerolog/logger.hpp"
#include <thread>
#include <vector>
#include <iostream>  // ✅ FIX: Added missing include

int main() {
    using namespace zerolog;
    
    auto logger = stdout_logger_mt("main");
    
    logger->info("Server listening on port {}", 8080);
    logger->debug("Debug mode enabled");
    logger->warn("High memory usage: {}MB", 2048);
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([logger, i] {
            for (int j = 0; j < 1000; ++j) {
                logger->info("Thread {} processing request {}", i, j);
            }
        });
    }
    
    for (auto& t : threads) t.join();
    logger->flush();
    
    std::cout << "All threads completed logging\n";
    return 0;
}
