#pragma once

#include <map>
#include <string>

namespace httplib {
class Response;
}

namespace platform {

struct HttpClientResponse {
  int status = 0;
  std::string content_type;
  std::string body;
  std::map<std::string, std::string> headers;
};

class HttpClient {
 public:
  explicit HttpClient(std::string base_url);

  HttpClientResponse Get(const std::string& path,
                         const std::map<std::string, std::string>& query = {}) const;
  HttpClientResponse Post(const std::string& path, const std::string& body,
                          const std::map<std::string, std::string>& query = {},
                          const std::string& content_type = "application/json") const;
  HttpClientResponse Patch(const std::string& path, const std::string& body,
                           const std::map<std::string, std::string>& query = {},
                           const std::string& content_type = "application/json") const;

 private:
  std::string BuildTarget(const std::string& path,
                          const std::map<std::string, std::string>& query) const;
  HttpClientResponse ConvertResponse(const httplib::Response& response) const;
  [[noreturn]] void Raise(const std::string& target) const;

  std::string scheme_;
  std::string host_;
  int port_;
  int timeout_seconds_ = 5;
};

}  // namespace platform
