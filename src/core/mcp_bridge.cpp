#include "core/mcp_bridge.hpp"

#include <map>
#include <sstream>
#include <stdexcept>

namespace core::mcp {
namespace {

const std::vector<ToolDefinition>& ToolCatalog() {
  static const std::vector<ToolDefinition> kTools = {
      {"apim.list_commands",
       "GET",
       "/api/commands",
       "List every HTTP API command exposed by the demo server.",
       nlohmann::json::object()},
      {"apim.health",
       "GET",
       "/api/health",
       "Check server readiness, uptime, and version metadata.",
       nlohmann::json::object()},
      {"apim.hello",
       "GET",
       "/api/hello",
       "Return a greeting (arguments: optional 'name').",
       {
           {"type", "object"},
           {"properties",
            {
                {"name",
                 {{"type", "string"}, {"description", "Name used in the hello response."}}}
            }},
           {"additionalProperties", false},
       }},
      {"apim.echo",
       "POST",
       "/api/echo",
       "Echo the provided payload for integration smoke testing.",
       {
           {"type", "object"},
           {"properties",
            {{"payload",
              {{"type", "string"},
               {"description", "JSON payload to echo (stringified JSON)."}}}}},
           {"required", {"payload"}},
           {"additionalProperties", false},
       }},
  };
  return kTools;
}

nlohmann::json MakeToolSchemaJson(const ToolDefinition& tool) {
  return {{"name", tool.name}, {"description", tool.description}, {"inputSchema", tool.input_schema}};
}

nlohmann::json TryParseJson(const std::string& text) {
  nlohmann::json parsed = nlohmann::json::parse(text, nullptr, false);
  if (parsed.is_discarded()) {
    return {};
  }
  return parsed;
}

std::string ToString(const nlohmann::json& value) {
  if (value.is_string()) {
    return value.get<std::string>();
  }
  if (value.is_null()) {
    return {};
  }
  return value.dump();
}

}  // namespace

Bridge::Bridge(std::string base_url) : client_(std::move(base_url)) {}

nlohmann::json Bridge::ToolSchemasJson() const {
  nlohmann::json tools = nlohmann::json::array();
  for (const auto& tool : ToolCatalog()) {
    tools.push_back(MakeToolSchemaJson(tool));
  }
  return tools;
}

const std::vector<ToolDefinition>& Bridge::Definitions() const { return ToolCatalog(); }

platform::HttpClientResponse Bridge::CallTool(const std::string& name,
                                              const nlohmann::json& arguments) const {
  if (name == "apim.list_commands") {
    return client_.Get("/api/commands");
  }
  if (name == "apim.health") {
    return client_.Get("/api/health");
  }
  if (name == "apim.hello") {
    std::map<std::string, std::string> query;
    if (const auto it = arguments.find("name"); it != arguments.end()) {
      query["name"] = ToString(*it);
    }
    return client_.Get("/api/hello", query);
  }
  if (name == "apim.echo") {
    const auto it = arguments.find("payload");
    if (it == arguments.end()) {
      throw std::invalid_argument("apim.echo requires a 'payload' argument");
    }
    const std::string payload = ToString(*it);
    return client_.Post("/api/echo", payload);
  }
  throw std::invalid_argument("Unknown tool: " + name);
}

std::string FormatResponseForDisplay(const platform::HttpClientResponse& response) {
  std::ostringstream stream;
  stream << "HTTP " << response.status;
  if (!response.content_type.empty()) {
    stream << " (" << response.content_type << ")";
  }
  stream << '\n';

  const auto parsed = TryParseJson(response.body);
  if (!parsed.empty()) {
    stream << parsed.dump(2);
  } else {
    stream << response.body;
  }
  return stream.str();
}

}  // namespace core::mcp
