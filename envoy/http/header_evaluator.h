#pragma once

#include "envoy/http/header_map.h"
#include "envoy/stream_info/stream_info.h"

namespace Envoy {
namespace Http {

// The interface of applying operations to a header map.
class HeaderEvaluator {
public:
  virtual ~HeaderEvaluator() = default;
  /**
   * Apply the header operations that are saved in the HeaderEvaluator. An example of the operation
   * is to add a new header name `foo` to the target header map and the header value is extracted
   * from the `bar` field in the stream_info, request headers or response headers.
   *
   * @param headers the target header map to be mutated.
   * @param request_headers request headers to be used in the header manipulation.
   * @param response_headers response headers to be used in the header manipulation.
   * @param stream_info the source of values that can be used in the evaluation.
   */
  virtual void evaluateHeaders(Http::HeaderMap& headers,
                               const Http::RequestHeaderMap& request_headers,
                               const Http::ResponseHeaderMap& response_headers,
                               const StreamInfo::StreamInfo& stream_info) const PURE;
};
} // namespace Http
} // namespace Envoy
