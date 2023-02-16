#pragma once

#include <string>

#include "headers_builder.h"
#include "request_headers.h"
#include "request_method.h"
#include "retry_policy.h"
#include "upstream_http_protocol.h"

namespace Envoy {
namespace Platform {

// Available algorithms to compress requests.
enum CompressionAlgorithm {
  Gzip,
  Brotli,
};

class RequestHeaders;
struct RetryPolicy;

class RequestHeadersBuilder : public HeadersBuilder {
public:
  RequestHeadersBuilder(RequestMethod request_method, std::string scheme, std::string authority,
                        std::string path);
  RequestHeadersBuilder(RequestMethod request_method, absl::string_view url);

  RequestHeadersBuilder& addRetryPolicy(const RetryPolicy& retry_policy);
  RequestHeadersBuilder& addUpstreamHttpProtocol(UpstreamHttpProtocol upstream_http_protocol);
  RequestHeadersBuilder& enableRequestCompression(CompressionAlgorithm algorithm);

  RequestHeaders build() const;

private:
  void initialize(RequestMethod request_method, std::string scheme, std::string authority,
                  std::string path);
};

using RequestHeadersBuilderSharedPtr = std::shared_ptr<RequestHeadersBuilder>;

} // namespace Platform
} // namespace Envoy
