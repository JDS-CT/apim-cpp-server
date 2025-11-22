#pragma once

#include <string>

namespace core {
class ChecklistStore;
}

namespace platform {
class HttpServer;
}  // namespace platform

namespace core {

struct ServerConfig {
  std::string host = "127.0.0.1";
  int port = 8080;
  std::string database_path = ".apim/checklists.db";
  bool seed_demo_data = true;
};

void ConfigureServer(platform::HttpServer& server, ChecklistStore& store);
ServerConfig LoadServerConfig();

}  // namespace core
