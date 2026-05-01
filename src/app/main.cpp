// ClaudeEngine application entrypoint.
// Responsibilities (src/CLAUDE.md): load configuration, run the engine.

#include "core/Logger.h"

#include <loguru.hpp>

int main(int argc, char* argv[]) {
  core::Logger::Init(argc, argv);
  LOG_F(INFO, "ClaudeEngine starting up");

  // TODO(#1): Load engine configuration from data/config.yaml
  // TODO(#2): Instantiate and run the Engine

  LOG_F(INFO, "ClaudeEngine shutting down");
  core::Logger::Shutdown();
  return 0;
}
