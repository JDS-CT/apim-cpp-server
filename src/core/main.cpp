#include <exception>
#include <string>

#include "core/app.hpp"
#include "core/logging.hpp"
#include "platform/http_server.hpp"

int main() {
  core::logging::InitializeFromEnvironment();
  const auto config = core::LoadServerConfig();

  platform::HttpServer server;
  core::ConfigureServer(server);

  core::logging::LogInfo("Starting APIM demo server on " + config.host + ":" +
                         std::to_string(config.port));

  try {
    server.Start(config.host, config.port);
  } catch (const std::exception& ex) {
    core::logging::LogError(std::string{"Server terminated with error: "} + ex.what());
    return 1;
  }

  core::logging::LogInfo("Server shut down gracefully.");
  return 0;
}
