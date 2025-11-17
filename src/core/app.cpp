#include "core/app.hpp"

#include <array>
#include <chrono>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "core/logging.hpp"
#include "platform/http_server.hpp"

namespace core {
namespace {

using core::logging::LogInfo;
using core::logging::LogWarn;

struct DemoCommand {
  std::string_view method;
  std::string_view path;
  std::string_view description;
};

constexpr std::array<DemoCommand, 4> kCommandCatalog = {
    DemoCommand{"GET", "/api/commands", "List every demo API command."},
    DemoCommand{"GET", "/api/health", "Report server readiness, uptime, and version."},
    DemoCommand{"GET", "/api/hello", "Send a friendly greeting back. Optional query parameter 'name'."},
    DemoCommand{"POST", "/api/echo", "Echo the provided payload for integration smoke tests."},
};

const auto kServerStart = std::chrono::steady_clock::now();

std::string EscapeJson(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (const char ch : value) {
    switch (ch) {
      case '\"':
        escaped += "\\\"";
        break;
      case '\\':
        escaped += "\\\\";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped += ch;
        break;
    }
  }
  return escaped;
}

platform::HttpResponse JsonResponse(std::string body, int status = 200) {
  platform::HttpResponse response;
  response.status = status;
  response.content_type = "application/json";
  response.body = std::move(body);
  return response;
}

std::string GetQueryParam(const platform::HttpRequest& request, const std::string& key,
                          const std::string& fallback) {
  if (const auto it = request.query_params.find(key); it != request.query_params.end()) {
    return it->second;
  }
  return fallback;
}

platform::HttpResponse HandleCommands(const platform::HttpRequest&) {
  std::ostringstream payload;
  payload << "{\"commands\":[";
  for (std::size_t i = 0; i < kCommandCatalog.size(); ++i) {
    if (i > 0) {
      payload << ',';
    }
    const auto& cmd = kCommandCatalog[i];
    payload << "{\"method\":\"" << cmd.method << "\",\"path\":\"" << cmd.path
            << "\",\"description\":\"" << EscapeJson(std::string{cmd.description}) << "\"}";
  }
  payload << "]}";
  LogInfo("GET /api/commands");
  return JsonResponse(payload.str());
}

platform::HttpResponse HandleHealth(const platform::HttpRequest&) {
  const auto now = std::chrono::steady_clock::now();
  const auto uptime_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - kServerStart).count();
  std::ostringstream payload;
  payload << "{\"status\":\"ok\",\"uptime_ms\":" << uptime_ms << ",\"version\":\"0.1.0\"}";
  LogInfo("GET /api/health");
  return JsonResponse(payload.str());
}

platform::HttpResponse HandleHello(const platform::HttpRequest& request) {
  const std::string name = GetQueryParam(request, "name", "world");
  LogInfo("GET /api/hello name=" + name);
  const std::string message = "Hello, " + name + "!";
  return JsonResponse(std::string{"{\"message\":\""} + EscapeJson(message) + "\"}");
}

platform::HttpResponse HandleEcho(const platform::HttpRequest& request) {
  LogInfo("POST /api/echo bytes=" + std::to_string(request.body.size()));
  std::ostringstream payload;
  payload << "{\"received\":\"" << EscapeJson(request.body) << "\"}";
  return JsonResponse(payload.str());
}

}  // namespace

void ConfigureServer(platform::HttpServer& server) {
  server.AddHandler(platform::HttpMethod::kGet, "/api/commands", HandleCommands);
  server.AddHandler(platform::HttpMethod::kGet, "/api/health", HandleHealth);
  server.AddHandler(platform::HttpMethod::kGet, "/api/hello", HandleHello);
  server.AddHandler(platform::HttpMethod::kPost, "/api/echo", HandleEcho);
}

ServerConfig LoadServerConfig() {
  ServerConfig config;
  if (const char* host = std::getenv("APIM_CPP_HOST")) {
    config.host = host;
  }
  if (const char* port = std::getenv("APIM_CPP_PORT")) {
    try {
      const int parsed = std::stoi(port);
      if (parsed > 0 && parsed <= 65535) {
        config.port = parsed;
      } else {
        LogWarn("APIM_CPP_PORT is outside the valid range, falling back to default 8080");
      }
    } catch (const std::exception& ex) {
      LogWarn(std::string{"Failed to parse APIM_CPP_PORT: "} + ex.what());
    }
  }
  return config;
}

}  // namespace core
