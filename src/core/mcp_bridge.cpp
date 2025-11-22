#include "core/mcp_bridge.hpp"

#include <map>
#include <sstream>
#include <stdexcept>

namespace core::mcp {
namespace {

std::string EncodePathSegment(const std::string& value) {
  static constexpr char kHex[] = "0123456789ABCDEF";
  std::string encoded;
  for (unsigned char ch : value) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
        ch == '-' || ch == '_' || ch == '.' || ch == '~') {
      encoded.push_back(static_cast<char>(ch));
    } else {
      encoded.push_back('%');
      encoded.push_back(kHex[(ch >> 4) & 0x0F]);
      encoded.push_back(kHex[ch & 0x0F]);
    }
  }
  return encoded;
}

const std::vector<ToolDefinition>& ToolCatalog() {
  static const std::vector<ToolDefinition> kTools = {
      {"apim.list_commands",
       "GET",
       "/api/commands",
       "List every HTTP API command exposed by the demo server.",
       nlohmann::json::object()},
      {"apim.list_checklists",
       "GET",
       "/api/checklists",
       "Enumerate every checklist currently present in the runtime store.",
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
      {"apim.get_slug",
       "GET",
       "/api/slug/{checklist_id}",
       "Fetch a single slug by Checklist ID.",
       {{"type", "object"},
        {"properties",
         {{"checklist_id",
           {{"type", "string"}, {"description", "Checklist ID to retrieve."}}}}},
        {"required", {"checklist_id"}},
        {"additionalProperties", false}}},
      {"apim.get_checklist",
       "GET",
       "/api/checklist/{checklist}",
       "Fetch all slugs belonging to a checklist.",
       {{"type", "object"},
        {"properties",
         {{"checklist",
           {{"type", "string"}, {"description", "Checklist name to retrieve."}}}}},
        {"required", {"checklist"}},
        {"additionalProperties", false}}},
      {"apim.relationships",
       "GET",
       "/api/relationships/{checklist_id}",
       "Inspect incoming and outgoing relationships for a slug.",
       {{"type", "object"},
        {"properties",
         {{"checklist_id",
           {{"type", "string"}, {"description", "Checklist ID whose graph should be returned."}}}}},
        {"required", {"checklist_id"}},
        {"additionalProperties", false}}},
      {"apim.update_slug",
       "PATCH",
       "/api/update",
       "Apply the minimal update contract to a slug (result/status/comment/timestamp).",
       {{"type", "object"},
        {"properties",
         {{"checklist_id",
           {{"type", "string"},
            {"description", "Slug to update (required)."}}},
          {"status", {{"type", "string"}, {"description", "Pass, Fail, NA, or Other."}}},
          {"result", {{"type", "string"}, {"description", "New measured result value."}}},
          {"comment", {{"type", "string"}, {"description", "Operator note or annotation."}}},
          {"timestamp",
           {{"type", "string"}, {"description", "ISO8601 timestamp override (optional)."}}}}},
        {"required", {"checklist_id"}},
        {"additionalProperties", false}}},
      {"apim.export_json",
       "GET",
       "/api/export/json",
       "Export every slug as a JSON array for downstream processing.",
       nlohmann::json::object()},
      {"apim.export_markdown",
       "GET",
       "/api/export/markdown/{checklist}",
       "Export a checklist as Markdown for authoring.",
       {{"type", "object"},
        {"properties",
         {{"checklist",
           {{"type", "string"}, {"description", "Checklist name to export."}}}}},
        {"required", {"checklist"}},
        {"additionalProperties", false}}},
      {"apim.import_markdown",
       "POST",
       "/api/import/markdown",
       "Import Markdown for a checklist and replace its runtime state.",
       {{"type", "object"},
        {"properties",
         {{"checklist",
           {{"type", "string"}, {"description", "Checklist name to replace."}}},
          {"markdown",
           {{"type", "string"}, {"description", "Markdown content to ingest."}}}}},
        {"required", {"checklist", "markdown"}},
        {"additionalProperties", false}}},
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

std::string RequireStringArg(const nlohmann::json& arguments, const std::string& key) {
  const auto it = arguments.find(key);
  if (it == arguments.end() || !it->is_string()) {
    throw std::invalid_argument("Missing or invalid argument: " + key);
  }
  return it->get<std::string>();
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
  if (name == "apim.list_checklists") {
    return client_.Get("/api/checklists");
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
  if (name == "apim.get_slug") {
    const auto id = EncodePathSegment(RequireStringArg(arguments, "checklist_id"));
    return client_.Get("/api/slug/" + id);
  }
  if (name == "apim.get_checklist") {
    const auto checklist = EncodePathSegment(RequireStringArg(arguments, "checklist"));
    return client_.Get("/api/checklist/" + checklist);
  }
  if (name == "apim.relationships") {
    const auto id = EncodePathSegment(RequireStringArg(arguments, "checklist_id"));
    return client_.Get("/api/relationships/" + id);
  }
  if (name == "apim.update_slug") {
    nlohmann::json payload = nlohmann::json::object();
    payload["checklist_id"] = RequireStringArg(arguments, "checklist_id");
    if (const auto it = arguments.find("status"); it != arguments.end()) {
      payload["status"] = ToString(*it);
    }
    if (const auto it = arguments.find("result"); it != arguments.end()) {
      payload["result"] = ToString(*it);
    }
    if (const auto it = arguments.find("comment"); it != arguments.end()) {
      payload["comment"] = ToString(*it);
    }
    if (const auto it = arguments.find("timestamp"); it != arguments.end()) {
      payload["timestamp"] = ToString(*it);
    }
    return client_.Patch("/api/update", payload.dump());
  }
  if (name == "apim.export_json") {
    return client_.Get("/api/export/json");
  }
  if (name == "apim.export_markdown") {
    const auto checklist = EncodePathSegment(RequireStringArg(arguments, "checklist"));
    return client_.Get("/api/export/markdown/" + checklist);
  }
  if (name == "apim.import_markdown") {
    const auto checklist = RequireStringArg(arguments, "checklist");
    const auto markdown = RequireStringArg(arguments, "markdown");
    return client_.Post("/api/import/markdown", markdown, {{"checklist", checklist}},
                        "text/markdown");
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
