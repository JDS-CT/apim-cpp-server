#include <chrono>
#include <exception>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/app.hpp"
#include "core/mcp_bridge.hpp"
#include "nlohmann/json.hpp"
#include "platform/http_server.hpp"

namespace {

void Assert(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

class TestServer {
 public:
  TestServer() { core::ConfigureServer(server_); }

  void Start(const std::string& host, int port) {
    host_ = host;
    port_ = port;
    worker_ = std::thread([this] {
      try {
        server_.Start(host_, port_);
      } catch (const std::exception& ex) {
        std::lock_guard<std::mutex> lock(mutex_);
        error_ = ex.what();
      }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  void Stop() {
    server_.Stop();
    if (worker_.joinable()) {
      worker_.join();
    }
    if (!error_.empty()) {
      throw std::runtime_error(error_);
    }
  }

 private:
  platform::HttpServer server_;
  std::thread worker_;
  std::string host_;
  int port_ = 0;
  std::string error_;
  std::mutex mutex_;
};

void RunTests() {
  constexpr int kTestPort = 18888;
  TestServer server;
  server.Start("127.0.0.1", kTestPort);

  core::mcp::Bridge bridge("http://127.0.0.1:" + std::to_string(kTestPort));

  const auto tools = bridge.ToolSchemasJson();
  Assert(tools.is_array(), "Tool schema response must be an array");
  Assert(tools.size() == 4, "Unexpected number of MCP tools exposed");

  const auto hello_response =
      bridge.CallTool("apim.hello", nlohmann::json::object({{"name", "Agent"}}));
  Assert(hello_response.status == 200, "apim.hello status must be 200");
  const auto hello_json = nlohmann::json::parse(hello_response.body, nullptr, false);
  Assert(hello_json.value("message", "") == "Hello, Agent!", "apim.hello message mismatch");

  const auto echo_response =
      bridge.CallTool("apim.echo", nlohmann::json::object({{"payload", R"({"client":"cpp"})"}}));
  Assert(echo_response.status == 200, "apim.echo status must be 200");
  const auto echo_json = nlohmann::json::parse(echo_response.body, nullptr, false);
  Assert(echo_json.value("received", "") == R"({"client":"cpp"})",
         "apim.echo payload mismatch");

  server.Stop();
}

}  // namespace

int main() {
  try {
    RunTests();
  } catch (const std::exception& ex) {
    std::cerr << "MCP bridge test failure: " << ex.what() << std::endl;
    return 1;
  }
  return 0;
}
