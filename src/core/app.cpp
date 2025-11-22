#include "core/app.hpp"

#include <chrono>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "core/checklist_store.hpp"
#include "core/logging.hpp"
#include "nlohmann/json.hpp"
#include "platform/http_server.hpp"

namespace core {
namespace {

using core::logging::LogInfo;
using core::logging::LogWarn;
using nlohmann::json;

struct DemoCommand {
  std::string_view method;
  std::string_view path;
  std::string_view description;
};

const std::vector<DemoCommand> kCommandCatalog = {
    {"GET", "/api/commands", "List every API command exposed by the server."},
    {"GET", "/api/health", "Report server readiness, uptime, and version."},
    {"GET", "/api/hello", "Send a greeting back. Optional query parameter 'name'."},
    {"POST", "/api/echo", "Echo the provided payload for integration smoke tests."},
    {"GET", "/api/checklists", "List available checklist slugs in the runtime store."},
    {"GET", "/api/slug/<checklist_id>", "Return a single checklist slug by ID."},
    {"GET", "/api/checklist/<checklist>", "Return every slug for the named checklist."},
    {"GET", "/api/relationships/<checklist_id>",
     "Return incoming and outgoing relationships for a slug."},
    {"PATCH", "/api/update", "Apply a minimal state update to a single slug."},
    {"PATCH", "/api/update_bulk", "Apply minimal state updates to multiple slugs."},
    {"GET", "/api/export/json", "Export all slugs as a JSON array."},
    {"GET", "/api/export/jsonl", "Export all slugs as JSON Lines."},
};

const auto kServerStart = std::chrono::steady_clock::now();

void ApplyCors(platform::HttpResponse& response) {
  response.headers["Access-Control-Allow-Origin"] = "*";
  response.headers["Access-Control-Allow-Methods"] = "GET,POST,PATCH,OPTIONS";
  response.headers["Access-Control-Allow-Headers"] = "Content-Type";
}

platform::HttpResponse JsonResponse(const json& body, int status = 200) {
  platform::HttpResponse response;
  response.status = status;
  response.content_type = "application/json";
  response.body = body.dump();
  ApplyCors(response);
  return response;
}

platform::HttpResponse TextResponse(const std::string& body, const std::string& content_type,
                                    int status = 200) {
  platform::HttpResponse response;
  response.status = status;
  response.content_type = content_type;
  response.body = body;
  ApplyCors(response);
  return response;
}

platform::HttpResponse ErrorResponse(const std::string& message, int status = 400) {
  return JsonResponse(json{{"error", message}}, status);
}

std::string GetQueryParam(const platform::HttpRequest& request, const std::string& key,
                          const std::string& fallback) {
  if (const auto it = request.query_params.find(key); it != request.query_params.end()) {
    return it->second;
  }
  return fallback;
}

json RelationshipsToJson(const RelationshipGraph& graph) {
  json outgoing = json::array();
  for (const auto& edge : graph.outgoing) {
    outgoing.push_back({{"predicate", edge.predicate}, {"target", edge.target}});
  }
  json incoming = json::array();
  for (const auto& edge : graph.incoming) {
    incoming.push_back({{"predicate", edge.predicate}, {"source", edge.target}});
  }
  return {{"outgoing", outgoing}, {"incoming", incoming}};
}

json SlugToJson(const ChecklistSlug& slug) {
  json relationships = json::array();
  for (const auto& edge : slug.relationships) {
    relationships.push_back({{"predicate", edge.predicate}, {"target", edge.target}});
  }

  return {{"checklist_id", slug.checklist_id},
          {"checklist", slug.checklist},
          {"section", slug.section},
          {"procedure", slug.procedure},
          {"action", slug.action},
          {"spec", slug.spec},
          {"result", slug.result},
          {"status", StatusToString(slug.status)},
          {"comment", slug.comment},
          {"timestamp", slug.timestamp},
          {"instructions", slug.instructions},
          {"relationships", relationships}};
}

SlugUpdate ParseUpdatePayload(const json& payload) {
  if (!payload.is_object()) {
    throw std::invalid_argument("Payload must be a JSON object.");
  }
  const auto it = payload.find("checklist_id");
  if (it == payload.end() || !it->is_string()) {
    throw std::invalid_argument("Field 'checklist_id' is required and must be a string.");
  }

  SlugUpdate update;
  update.checklist_id = it->get<std::string>();

  if (const auto result_it = payload.find("result"); result_it != payload.end() &&
                                                     (result_it->is_string() || result_it->is_null())) {
    update.result = result_it->is_null() ? std::string{} : result_it->get<std::string>();
  }

  if (const auto status_it = payload.find("status"); status_it != payload.end()) {
    if (!status_it->is_string()) {
      throw std::invalid_argument("Field 'status' must be a string when provided.");
    }
    const auto status = ParseStatus(status_it->get<std::string>());
    if (status == ChecklistStatus::kUnknown) {
      throw std::invalid_argument("Status must be Pass, Fail, NA, or Other.");
    }
    update.status = status;
  }

  if (const auto comment_it = payload.find("comment");
      comment_it != payload.end() && (comment_it->is_string() || comment_it->is_null())) {
    update.comment = comment_it->is_null() ? std::string{} : comment_it->get<std::string>();
  }

  if (const auto ts_it = payload.find("timestamp"); ts_it != payload.end()) {
    if (!ts_it->is_string()) {
      throw std::invalid_argument("Field 'timestamp' must be a string when provided.");
    }
    update.timestamp = ts_it->get<std::string>();
  }

  return update;
}

std::vector<SlugUpdate> ParseBulkPayload(const json& payload) {
  if (!payload.is_array()) {
    throw std::invalid_argument("Bulk payload must be a JSON array.");
  }
  std::vector<SlugUpdate> updates;
  for (const auto& item : payload) {
    updates.push_back(ParseUpdatePayload(item));
  }
  return updates;
}

platform::HttpResponse HandleCorsPreflight(const platform::HttpRequest&) {
  platform::HttpResponse response;
  response.status = 204;
  response.content_type = "text/plain";
  ApplyCors(response);
  return response;
}

}  // namespace

void ConfigureServer(platform::HttpServer& server, ChecklistStore& store) {
  auto handle_commands = [](const platform::HttpRequest&) {
    json commands = json::array();
    for (const auto& cmd : kCommandCatalog) {
      commands.push_back(
          {{"method", cmd.method}, {"path", cmd.path}, {"description", cmd.description}});
    }
    LogInfo("GET /api/commands");
    return JsonResponse(json{{"commands", commands}});
  };

  auto handle_health = [&store](const platform::HttpRequest&) {
    const auto now = std::chrono::steady_clock::now();
    const auto uptime_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - kServerStart).count();
    json payload{{"status", "ok"},
                 {"uptime_ms", uptime_ms},
                 {"version", "0.2.0"},
                 {"checklists", store.ListChecklists()}};
    LogInfo("GET /api/health");
    return JsonResponse(payload);
  };

  auto handle_hello = [](const platform::HttpRequest& request) {
    const std::string name = GetQueryParam(request, "name", "world");
    LogInfo("GET /api/hello name=" + name);
    const std::string message = "Hello, " + name + "!";
    return JsonResponse(json{{"message", message}});
  };

  auto handle_echo = [](const platform::HttpRequest& request) {
    LogInfo("POST /api/echo bytes=" + std::to_string(request.body.size()));
    return JsonResponse(json{{"received", request.body}});
  };

  auto handle_checklists = [&store](const platform::HttpRequest&) {
    const auto names = store.ListChecklists();
    LogInfo("GET /api/checklists");
    return JsonResponse(json{{"checklists", names}});
  };

  auto handle_slug = [&store](const platform::HttpRequest& request) {
    if (request.path_params.empty()) {
      return ErrorResponse("Missing checklist_id path parameter.", 400);
    }
    const std::string checklist_id = request.path_params.front();
    LogInfo("GET /api/slug/" + checklist_id);
    const auto slug = store.GetSlugOrThrow(checklist_id);
    return JsonResponse(SlugToJson(slug));
  };

  auto handle_checklist = [&store](const platform::HttpRequest& request) {
    if (request.path_params.empty()) {
      return ErrorResponse("Missing checklist path parameter.", 400);
    }
    const std::string checklist = request.path_params.front();
    LogInfo("GET /api/checklist/" + checklist);
    const auto slugs = store.GetSlugsForChecklist(checklist);
    json payload = json::array();
    for (const auto& slug : slugs) {
      payload.push_back(SlugToJson(slug));
    }
    return JsonResponse(json{{"checklist", checklist}, {"slugs", payload}});
  };

  auto handle_relationships = [&store](const platform::HttpRequest& request) {
    if (request.path_params.empty()) {
      return ErrorResponse("Missing checklist_id path parameter.", 400);
    }
    const std::string checklist_id = request.path_params.front();
    LogInfo("GET /api/relationships/" + checklist_id);
    const auto graph = store.GetRelationships(checklist_id);
    json payload = RelationshipsToJson(graph);
    payload["checklist_id"] = checklist_id;
    return JsonResponse(payload);
  };

  auto handle_update = [&store](const platform::HttpRequest& request) {
    const auto payload = json::parse(request.body, nullptr, false);
    if (payload.is_discarded()) {
      return ErrorResponse("Invalid JSON payload.", 400);
    }
    try {
      const auto update = ParseUpdatePayload(payload);
      store.ApplyUpdate(update);
      const auto updated = store.GetSlugOrThrow(update.checklist_id);
      LogInfo("PATCH /api/update checklist_id=" + update.checklist_id);
      return JsonResponse(SlugToJson(updated));
    } catch (const std::exception& ex) {
      return ErrorResponse(ex.what(), 400);
    }
  };

  auto handle_update_bulk = [&store](const platform::HttpRequest& request) {
    const auto payload = json::parse(request.body, nullptr, false);
    if (payload.is_discarded()) {
      return ErrorResponse("Invalid JSON payload.", 400);
    }
    try {
      const auto updates = ParseBulkPayload(payload);
      store.ApplyBulkUpdates(updates);
      json updated = json::array();
      for (const auto& update : updates) {
        updated.push_back(SlugToJson(store.GetSlugOrThrow(update.checklist_id)));
      }
      LogInfo("PATCH /api/update_bulk count=" + std::to_string(updates.size()));
      return JsonResponse(json{{"updated", updated}});
    } catch (const std::exception& ex) {
      return ErrorResponse(ex.what(), 400);
    }
  };

  auto handle_export_json = [&store](const platform::HttpRequest&) {
    const auto slugs = store.ExportAllSlugs();
    json payload = json::array();
    for (const auto& slug : slugs) {
      payload.push_back(SlugToJson(slug));
    }
    LogInfo("GET /api/export/json");
    return JsonResponse(payload);
  };

  auto handle_export_jsonl = [&store](const platform::HttpRequest&) {
    const auto slugs = store.ExportAllSlugs();
    std::ostringstream stream;
    for (std::size_t i = 0; i < slugs.size(); ++i) {
      stream << SlugToJson(slugs[i]).dump();
      if (i + 1 < slugs.size()) {
        stream << "\n";
      }
    }
    LogInfo("GET /api/export/jsonl");
    return TextResponse(stream.str(), "application/json", 200);
  };

  server.AddHandler(platform::HttpMethod::kGet, "/api/commands", handle_commands);
  server.AddHandler(platform::HttpMethod::kGet, "/api/health", handle_health);
  server.AddHandler(platform::HttpMethod::kGet, "/api/hello", handle_hello);
  server.AddHandler(platform::HttpMethod::kPost, "/api/echo", handle_echo);
  server.AddHandler(platform::HttpMethod::kGet, "/api/checklists", handle_checklists);
  server.AddHandler(platform::HttpMethod::kGet, R"(/api/slug/(.+))", handle_slug);
  server.AddHandler(platform::HttpMethod::kGet, R"(/api/checklist/(.+))", handle_checklist);
  server.AddHandler(platform::HttpMethod::kGet, R"(/api/relationships/(.+))",
                    handle_relationships);
  server.AddHandler(platform::HttpMethod::kPatch, "/api/update", handle_update);
  server.AddHandler(platform::HttpMethod::kPatch, "/api/update_bulk", handle_update_bulk);
  server.AddHandler(platform::HttpMethod::kGet, "/api/export/json", handle_export_json);
  server.AddHandler(platform::HttpMethod::kGet, "/api/export/jsonl", handle_export_jsonl);

  server.AddHandler(platform::HttpMethod::kOptions, "/api/commands", HandleCorsPreflight);
  server.AddHandler(platform::HttpMethod::kOptions, "/api/health", HandleCorsPreflight);
  server.AddHandler(platform::HttpMethod::kOptions, "/api/hello", HandleCorsPreflight);
  server.AddHandler(platform::HttpMethod::kOptions, "/api/echo", HandleCorsPreflight);
  server.AddHandler(platform::HttpMethod::kOptions, "/api/checklists", HandleCorsPreflight);
  server.AddHandler(platform::HttpMethod::kOptions, R"(/api/slug/.*)", HandleCorsPreflight);
  server.AddHandler(platform::HttpMethod::kOptions, R"(/api/checklist/.*)", HandleCorsPreflight);
  server.AddHandler(platform::HttpMethod::kOptions, R"(/api/relationships/.*)",
                    HandleCorsPreflight);
  server.AddHandler(platform::HttpMethod::kOptions, "/api/update", HandleCorsPreflight);
  server.AddHandler(platform::HttpMethod::kOptions, "/api/update_bulk", HandleCorsPreflight);
  server.AddHandler(platform::HttpMethod::kOptions, "/api/export/json", HandleCorsPreflight);
  server.AddHandler(platform::HttpMethod::kOptions, "/api/export/jsonl", HandleCorsPreflight);
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
  if (const char* db_path = std::getenv("APIM_CPP_DB")) {
    config.database_path = db_path;
  }
  if (const char* seed = std::getenv("APIM_CPP_SEED_DEMO")) {
    const std::string value = seed;
    if (value == "0" || value == "false" || value == "FALSE") {
      config.seed_demo_data = false;
    }
  }
  return config;
}

}  // namespace core
