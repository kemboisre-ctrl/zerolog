#include "zerolog/logger.hpp"
#include <iostream>

namespace zerolog {

// Explicit instantiations for common configurations
template class Logger<NullSink, LogLevel::TRACE>;
template class Logger<StdoutSink, LogLevel::TRACE>;

// Factory functions
std::shared_ptr<Logger<StdoutSink>> stdout_logger_mt(const char* name) {
    return std::make_shared<Logger<StdoutSink>>(StdoutSink{}, true);
}

std::shared_ptr<Logger<NullSink>> null_logger_mt(const char* name) {
    return std::make_shared<Logger<NullSink>>(NullSink{}, true);
}

} // namespace zerolog
