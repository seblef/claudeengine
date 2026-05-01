#include "core/Logger.h"

#include <loguru.hpp>

namespace core {

void Logger::Init(int argc, char* argv[]) {
  loguru::init(argc, argv);
  loguru::add_file("claude_engine.log", loguru::Truncate, loguru::Verbosity_MAX);
  LOG_F(INFO, "Logger initialised (profile: %s)",
#ifdef NDEBUG
        "stable"
#else
        "dev"
#endif
  );
}

void Logger::Shutdown() {
  LOG_F(INFO, "Logger shutting down");
  loguru::shutdown();
}

}  // namespace core
