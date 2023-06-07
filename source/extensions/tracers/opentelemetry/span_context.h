#pragma once

#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

/**
 * This class represents the context of an OpenTelemetry span, including the following
 * characteristics: trace id, span id, parent id, and trace flags.
 */
class SpanContext {
public:
  /*
   * Default constructor creates an empty context.
   */
  SpanContext() = default;

  /*
   * Constructor that creates a context object from the supplied attributes.
   */
  SpanContext(const absl::string_view& version, const absl::string_view& trace_id,
              const absl::string_view& parent_id, bool sampled)
      : version_(version), trace_id_(trace_id), parent_id_(parent_id), sampled_(sampled) {}

  /**
   * @return the span's version as a hex string.
   */
  const std::string& version() const { return version_; }

  /**
   * @return the span's parent id as a hex string.
   */
  const std::string& parentId() const { return parent_id_; }

  /**
   * @return the trace id as an integer.
   */
  const std::string& traceId() const { return trace_id_; }

  /**
   * @return the sampled flag.
   */
  bool sampled() const { return sampled_; }

private:
  const std::string version_;
  const std::string trace_id_;
  const std::string parent_id_;
  const bool sampled_{false};
};

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
