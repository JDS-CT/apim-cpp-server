#pragma once

#include <string>

namespace platform {
class HttpServer;
}  // namespace platform

namespace core {

struct ServerConfig {
  std::string host = "127.0.0.1";
  int port = 8080;
};

void ConfigureServer(platform::HttpServer& server);
ServerConfig LoadServerConfig();

}  // namespace core
