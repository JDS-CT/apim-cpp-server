#include "platform/http_server.hpp"

#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

#include "httplib.h"

namespace platform {

class HttpServer::Impl {
 public:
  httplib::Server server;
  std::mutex lifecycle_mutex;
  bool running = false;
};

namespace {

HttpRequest ConvertRequest(const httplib::Request& req) {
  HttpRequest request;
  request.path = req.path;
  request.body = req.body;
  for (const auto& param : req.params) {
    request.query_params[param.first] = param.second;
  }
  for (const auto& header : req.headers) {
    request.headers[header.first] = header.second;
  }
  return request;
}

httplib::Server::Handler WrapHandler(HttpHandler handler) {
  return [handler = std::move(handler)](const httplib::Request& req,
                                        httplib::Response& res) {
    try {
      const HttpRequest request = ConvertRequest(req);
      HttpResponse response = handler(request);
      if (response.content_type.empty()) {
        response.content_type = "text/plain";
      }
      for (const auto& header : response.headers) {
        res.set_header(header.first.c_str(), header.second.c_str());
      }
      res.status = response.status;
      res.set_content(response.body, response.content_type);
    } catch (const std::exception& ex) {
      res.status = 500;
      res.set_content(std::string{"{\"error\":\""} + ex.what() + "\"}", "application/json");
    } catch (...) {
      res.status = 500;
      res.set_content("{\"error\":\"Unhandled server error\"}", "application/json");
    }
  };
}

}  // namespace

HttpServer::HttpServer() : impl_(std::make_unique<Impl>()) {}

HttpServer::~HttpServer() = default;

void HttpServer::AddHandler(HttpMethod method, const std::string& path, HttpHandler handler) {
  if (!handler) {
    throw std::invalid_argument("HTTP handler must not be empty");
  }

  auto wrapped_handler = WrapHandler(std::move(handler));

  switch (method) {
    case HttpMethod::kGet:
      impl_->server.Get(path, std::move(wrapped_handler));
      break;
    case HttpMethod::kPost:
      impl_->server.Post(path, std::move(wrapped_handler));
      break;
    default:
      throw std::invalid_argument("Unsupported HTTP method");
  }
}

void HttpServer::Start(const std::string& host, int port) {
  {
    std::lock_guard<std::mutex> lock(impl_->lifecycle_mutex);
    if (impl_->running) {
      throw std::runtime_error("Server already running");
    }
    impl_->running = true;
  }

  const bool ok = impl_->server.listen(host, port);

  {
    std::lock_guard<std::mutex> lock(impl_->lifecycle_mutex);
    impl_->running = false;
  }

  if (!ok) {
    throw std::runtime_error("Failed to bind HTTP server to " + host + ":" +
                             std::to_string(port));
  }
}

void HttpServer::Stop() { impl_->server.stop(); }

}  // namespace platform
