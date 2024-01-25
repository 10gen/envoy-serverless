#pragma once

#include <optional>
#include <vector>

#include "absl/types/optional.h"
#include "library/cc/headers.h"
#include "library/cc/request_headers.h"

namespace Envoy {
namespace Platform {

class RequestHeaders;

struct RetryPolicy {
  int max_retry_count;
  std::vector<std::string> retry_on;
  std::vector<int> retry_status_codes;
  absl::optional<int> per_try_timeout_ms;
  absl::optional<int> total_upstream_timeout_ms;

  RawHeaderMap asRawHeaderMap() const;
  static RetryPolicy fromRawHeaderMap(const RawHeaderMap& headers);
};

using RetryPolicySharedPtr = std::shared_ptr<RetryPolicy>;

} // namespace Platform
} // namespace Envoy
