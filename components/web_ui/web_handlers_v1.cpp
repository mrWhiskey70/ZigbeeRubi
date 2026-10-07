#include "web_routes.hpp"
#ifdef ESP_PLATFORM
#include "cJSON.h"
#include "esp_http_server.h"
#include "service_runtime_api.hpp"
#include <cstring>
#include <memory>
#include <string>
namespace web_ui {
namespace {
esp_err_t v1(httpd_req_t *req) {
  auto *ctx = static_cast<WebRouteContext *>(req->user_ctx);
  if (!ctx || !ctx->runtime)
    return ESP_FAIL;
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  if (req->content_len > 8192) {
    httpd_resp_set_status(req, "413 Payload Too Large");
    return httpd_resp_sendstr(req, "{\"error\":\"too_large\"}");
  }
  std::string body(req->content_len, '\0');
  size_t at = 0;
  while (at < body.size()) {
    int n = httpd_req_recv(req, body.data() + at, body.size() - at);
    if (n <= 0)
      return ESP_FAIL;
    at += n;
  }
  if (body.find('\0') != std::string::npos) {
    httpd_resp_set_status(req, "400 Bad Request");
    return httpd_resp_sendstr(req, "{\"error\":\"invalid_request\"}");
  }
  const char *method = req->method == HTTP_GET    ? "GET"
                       : req->method == HTTP_POST ? "POST"
                       : req->method == HTTP_PUT  ? "PUT"
                                                  : "DELETE";
  auto *env = cJSON_CreateObject();
  cJSON_AddStringToObject(env, "method", method);
  std::string path(req->uri);
  auto q = path.find('?');
  if (q != std::string::npos)
    path.resize(q);
  cJSON_AddStringToObject(env, "path", path.c_str());
  cJSON_AddStringToObject(env, "body", body.c_str());
  char *text = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  if (!text)
    return ESP_FAIL;
  std::string envelope(text);
  cJSON_free(text);
  std::string response;
  if (!ctx->runtime->scenario_request(envelope, response)) {
    httpd_resp_set_status(req, "503 Service Unavailable");
    return httpd_resp_sendstr(req, "{\"error\":\"service_unavailable\"}");
  }
  auto *result = cJSON_Parse(response.c_str());
  if (!result)
    return ESP_FAIL;
  auto *s = cJSON_GetObjectItemCaseSensitive(result, "status");
  auto *b = cJSON_GetObjectItemCaseSensitive(result, "body");
  int code = s ? s->valueint : 503;
  const char *status = code == 200   ? "200 OK"
                       : code == 201 ? "201 Created"
                       : code == 202 ? "202 Accepted"
                       : code == 400 ? "400 Bad Request"
                       : code == 404 ? "404 Not Found"
                       : code == 409 ? "409 Conflict"
                       : code == 422 ? "422 Unprocessable Entity"
                                     : "503 Service Unavailable";
  httpd_resp_set_status(req, status);
  char *json = cJSON_PrintUnformatted(b);
  cJSON_Delete(result);
  if (!json)
    return ESP_FAIL;
  auto err = httpd_resp_sendstr(req, json);
  cJSON_free(json);
  return err;
}
} // namespace
bool register_v1_routes(void *server, WebRouteContext *ctx) noexcept {
  for (auto method : {HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_DELETE}) {
    httpd_uri_t route{};
    route.uri = "/api/v1/*";
    route.method = method;
    route.handler = v1;
    route.user_ctx = ctx;
    if (httpd_register_uri_handler(static_cast<httpd_handle_t>(server),
                                   &route) != ESP_OK)
      return false;
  }
  return true;
}
} // namespace web_ui
#else
namespace web_ui {
bool register_v1_routes(void *, WebRouteContext *) noexcept { return true; }
} // namespace web_ui
#endif
