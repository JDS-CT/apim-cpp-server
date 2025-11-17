#pragma once

#include <string>
#include <vector>

#include "nlohmann/json.hpp"
#include "platform/http_client.hpp"

namespace core::mcp {

struct ToolDefinition {
  std::string name;
  std::string method;
  std::string path;
  std::string description;
  nlohmann::json input_schema;
};

class Bridge {
 public:
  explicit Bridge(std::string base_url);

  nlohmann::json ToolSchemasJson() const;
  const std::vector<ToolDefinition>& Definitions() const;

  platform::HttpClientResponse CallTool(const std::string& name,
                                        const nlohmann::json& arguments) const;

 private:
  platform::HttpClient client_;
};

std::string FormatResponseForDisplay(const platform::HttpClientResponse& response);

}  // namespace core::mcp
