#pragma once

#include <memory>
#include <string>

#include "envoy/common/pure.h"
#include "envoy/data/accesslog/v3/accesslog.pb.h"
#include "envoy/filesystem/filesystem.h"
#include "envoy/http/header_map.h"
#include "envoy/stream_info/stream_info.h"

#include "source/common/protobuf/protobuf.h"

namespace Envoy {
namespace AccessLog {

class AccessLogFile {
public:
  virtual ~AccessLogFile() = default;

  /**
   * Write data to the file.
   */
  virtual void write(absl::string_view) PURE;

  /**
   * Reopen the file.
   */
  virtual void reopen() PURE;

  /**
   * Synchronously flush all pending data to disk.
   */
  virtual void flush() PURE;
};

using AccessLogFileSharedPtr = std::shared_ptr<AccessLogFile>;

class AccessLogManager {
public:
  virtual ~AccessLogManager() = default;

  /**
   * Reopen all of the access log files.
   */
  virtual void reopen() PURE;

  /**
   * Create a new access log file managed by the access log manager.
   * @param file_info specifies the file to create/open.
   * @return the opened file.
   */
  virtual AccessLogFileSharedPtr
  createAccessLog(const Envoy::Filesystem::FilePathAndType& file_info) PURE;
};

using AccessLogManagerPtr = std::unique_ptr<AccessLogManager>;
using AccessLogType = envoy::data::accesslog::v3::AccessLogType;

/**
 * Templated interface for access log filters.
 */
template <class Context> class FilterBase {
public:
  virtual ~FilterBase() = default;

  /**
   * Evaluate whether an access log should be written based on request and response data.
   * @return TRUE if the log should be written.
   */
  virtual bool evaluate(const Context& context, const StreamInfo::StreamInfo& info) const PURE;
};
template <class Context> using FilterBasePtr = std::unique_ptr<FilterBase<Context>>;

/**
 * Templated interface for access log instances.
 * TODO(wbpcode): refactor existing access log instances and related other interfaces to use this
 * interface. See https://github.com/envoyproxy/envoy/issues/28773.
 */
template <class Context> class InstanceBase {
public:
  virtual ~InstanceBase() = default;

  /**
   * Log a completed request.
   * @param context supplies the context for the log.
   * @param stream_info supplies additional information about the request not
   * contained in the request headers.
   */
  virtual void log(const Context& context, const StreamInfo::StreamInfo& stream_info) PURE;
};
template <class Context> using InstanceBaseSharedPtr = std::shared_ptr<InstanceBase<Context>>;

/**
 * Interface for access log filters.
 */
class Filter {
public:
  virtual ~Filter() = default;

  /**
   * Evaluate whether an access log should be written based on request and response data.
   * @return TRUE if the log should be written.
   */
  virtual bool evaluate(const StreamInfo::StreamInfo& info,
                        const Http::RequestHeaderMap& request_headers,
                        const Http::ResponseHeaderMap& response_headers,
                        const Http::ResponseTrailerMap& response_trailers,
                        AccessLogType access_log_type) const PURE;
};

using FilterPtr = std::unique_ptr<Filter>;

/**
 * Abstract access logger for requests and connections.
 */
class Instance {
public:
  virtual ~Instance() = default;

  /**
   * Log a completed request.
   * @param request_headers supplies the incoming request headers after filtering.
   * @param response_headers supplies response headers.
   * @param response_trailers supplies response trailers.
   * @param stream_info supplies additional information about the request not
   * contained in the request headers.
   * @param access_log_type supplies additional information about the type of the
   * log record, i.e the location in the code which recorded the log.
   */
  virtual void log(const Http::RequestHeaderMap* request_headers,
                   const Http::ResponseHeaderMap* response_headers,
                   const Http::ResponseTrailerMap* response_trailers,
                   const StreamInfo::StreamInfo& stream_info, AccessLogType access_log_type) PURE;
};

using InstanceSharedPtr = std::shared_ptr<Instance>;

} // namespace AccessLog
} // namespace Envoy
