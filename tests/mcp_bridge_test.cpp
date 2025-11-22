#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/app.hpp"
#include "core/checklist_store.hpp"
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
  explicit TestServer(std::string db_path) : db_path_(std::move(db_path)), store_(db_path_) {
    store_.Initialize(true);
    core::ConfigureServer(server_, store_);
  }

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
    if (!db_path_.empty()) {
      std::error_code ec;
      std::filesystem::remove(db_path_, ec);
    }
  }

 private:
  platform::HttpServer server_;
  std::string db_path_;
  core::ChecklistStore store_;
  std::thread worker_;
  std::string host_;
  int port_ = 0;
  std::string error_;
  std::mutex mutex_;
};

void RunTests() {
  constexpr int kTestPort = 18888;
  const auto db_path =
      (std::filesystem::temp_directory_path() / "apim-mcp-bridge-test.db").string();
  TestServer server(db_path);
  server.Start("127.0.0.1", kTestPort);

  core::mcp::Bridge bridge("http://127.0.0.1:" + std::to_string(kTestPort));

  const auto tools = bridge.ToolSchemasJson();
  Assert(tools.is_array(), "Tool schema response must be an array");
  Assert(tools.size() == 10, "Unexpected number of MCP tools exposed");

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

  const auto export_response = bridge.CallTool("apim.export_json", nlohmann::json::object());
  Assert(export_response.status == 200, "apim.export_json status must be 200");
  const auto export_json = nlohmann::json::parse(export_response.body, nullptr, false);
  Assert(export_json.is_array(), "apim.export_json must return an array");
  Assert(!export_json.empty(), "apim.export_json should return seeded slugs");

  const auto checklist_id = export_json.at(0).value("checklist_id", "");
  Assert(!checklist_id.empty(), "Seed slug must include checklist_id");

  const auto slug_response =
      bridge.CallTool("apim.get_slug", nlohmann::json::object({{"checklist_id", checklist_id}}));
  Assert(slug_response.status == 200, "apim.get_slug status must be 200");
  const auto slug_json = nlohmann::json::parse(slug_response.body, nullptr, false);
  Assert(slug_json.value("checklist_id", "") == checklist_id, "Slug ID mismatch");

  const std::string updated_comment = "Updated via MCP test";
  const auto update_response = bridge.CallTool(
      "apim.update_slug",
      nlohmann::json::object({{"checklist_id", checklist_id}, {"comment", updated_comment}}));
  Assert(update_response.status == 200, "apim.update_slug status must be 200");
  const auto update_json = nlohmann::json::parse(update_response.body, nullptr, false);
  Assert(update_json.value("comment", "") == updated_comment,
         "apim.update_slug did not persist comment");

  const auto relationships_response = bridge.CallTool(
      "apim.relationships", nlohmann::json::object({{"checklist_id", checklist_id}}));
  Assert(relationships_response.status == 200, "apim.relationships status must be 200");

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
