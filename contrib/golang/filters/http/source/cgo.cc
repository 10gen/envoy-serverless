#include "contrib/golang/filters/http/source/golang_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Golang {

//
// These functions may be invoked in another go thread,
// which means may introduce race between go thread and envoy thread.
// So we use the envoy's dispatcher in the filter to post it, and make it only executes in the envoy
// thread.
//

absl::string_view copyGoString(void* str) {
  if (str == nullptr) {
    return "";
  }
  auto goStr = reinterpret_cast<GoString*>(str);
  return absl::string_view(goStr->p, goStr->n); // NOLINT(modernize-return-braced-init-list)
}

extern "C" {

CAPIStatus envoyGoFilterHandlerWrapper(void* r,
                                       std::function<CAPIStatus(std::shared_ptr<Filter>&)> f) {
  auto req = reinterpret_cast<httpRequestInternal*>(r);
  auto weakFilter = req->weakFilter();
  if (auto filter = weakFilter.lock()) {
    return f(filter);
  }
  return CAPIStatus::CAPIFilterIsGone;
}

CAPIStatus envoyGoFilterHttpContinue(void* r, int status) {
  return envoyGoFilterHandlerWrapper(r, [status](std::shared_ptr<Filter>& filter) -> CAPIStatus {
    return filter->continueStatus(static_cast<GolangStatus>(status));
  });
}

CAPIStatus envoyGoFilterHttpSendLocalReply(void* r, int response_code, void* body_text,
                                           void* headers, long long int grpc_status,
                                           void* details) {
  return envoyGoFilterHandlerWrapper(
      r,
      [response_code, body_text, headers, grpc_status,
       details](std::shared_ptr<Filter>& filter) -> CAPIStatus {
        UNREFERENCED_PARAMETER(headers);
        auto grpcStatus = static_cast<Grpc::Status::GrpcStatus>(grpc_status);
        return filter->sendLocalReply(static_cast<Http::Code>(response_code),
                                      copyGoString(body_text), nullptr, grpcStatus,
                                      copyGoString(details));
      });
}

// unsafe API, without copy memory from c to go.
CAPIStatus envoyGoFilterHttpGetHeader(void* r, void* key, void* value) {
  return envoyGoFilterHandlerWrapper(r,
                                     [key, value](std::shared_ptr<Filter>& filter) -> CAPIStatus {
                                       auto keyStr = copyGoString(key);
                                       auto goValue = reinterpret_cast<GoString*>(value);
                                       return filter->getHeader(keyStr, goValue);
                                     });
}

CAPIStatus envoyGoFilterHttpCopyHeaders(void* r, void* strs, void* buf) {
  return envoyGoFilterHandlerWrapper(r, [strs, buf](std::shared_ptr<Filter>& filter) -> CAPIStatus {
    auto goStrs = reinterpret_cast<GoString*>(strs);
    auto goBuf = reinterpret_cast<char*>(buf);
    return filter->copyHeaders(goStrs, goBuf);
  });
}

CAPIStatus envoyGoFilterHttpSetHeader(void* r, void* key, void* value) {
  return envoyGoFilterHandlerWrapper(r,
                                     [key, value](std::shared_ptr<Filter>& filter) -> CAPIStatus {
                                       auto keyStr = copyGoString(key);
                                       auto valueStr = copyGoString(value);
                                       return filter->setHeader(keyStr, valueStr);
                                     });
}

CAPIStatus envoyGoFilterHttpRemoveHeader(void* r, void* key) {
  return envoyGoFilterHandlerWrapper(r, [key](std::shared_ptr<Filter>& filter) -> CAPIStatus {
    // TODO: it's safe to skip copy
    auto keyStr = copyGoString(key);
    return filter->removeHeader(keyStr);
  });
}

CAPIStatus envoyGoFilterHttpGetBuffer(void* r, unsigned long long int buffer_ptr, void* data) {
  return envoyGoFilterHandlerWrapper(
      r, [buffer_ptr, data](std::shared_ptr<Filter>& filter) -> CAPIStatus {
        auto buffer = reinterpret_cast<Buffer::Instance*>(buffer_ptr);
        return filter->copyBuffer(buffer, reinterpret_cast<char*>(data));
      });
}

CAPIStatus envoyGoFilterHttpSetBufferHelper(void* r, unsigned long long int buffer_ptr, void* data,
                                            int length, bufferAction action) {
  return envoyGoFilterHandlerWrapper(
      r, [buffer_ptr, data, length, action](std::shared_ptr<Filter>& filter) -> CAPIStatus {
        auto buffer = reinterpret_cast<Buffer::Instance*>(buffer_ptr);
        auto value = absl::string_view(reinterpret_cast<const char*>(data), length);
        return filter->setBufferHelper(buffer, value, action);
      });
}

CAPIStatus envoyGoFilterHttpCopyTrailers(void* r, void* strs, void* buf) {
  return envoyGoFilterHandlerWrapper(r, [strs, buf](std::shared_ptr<Filter>& filter) -> CAPIStatus {
    auto goStrs = reinterpret_cast<GoString*>(strs);
    auto goBuf = reinterpret_cast<char*>(buf);
    return filter->copyTrailers(goStrs, goBuf);
  });
}

CAPIStatus envoyGoFilterHttpSetTrailer(void* r, void* key, void* value) {
  return envoyGoFilterHandlerWrapper(r,
                                     [key, value](std::shared_ptr<Filter>& filter) -> CAPIStatus {
                                       auto keyStr = copyGoString(key);
                                       auto valueStr = copyGoString(value);
                                       return filter->setTrailer(keyStr, valueStr);
                                     });
}

CAPIStatus envoyGoFilterHttpGetStringValue(void* r, int id, void* value) {
  return envoyGoFilterHandlerWrapper(r, [id, value](std::shared_ptr<Filter>& filter) -> CAPIStatus {
    auto valueStr = reinterpret_cast<GoString*>(value);
    return filter->getStringValue(id, valueStr);
  });
}

void envoyGoFilterHttpFinalize(void* r, int reason) {
  UNREFERENCED_PARAMETER(reason);
  // req is used by go, so need to use raw memory and then it is safe to release at the gc finalize
  // phase of the go object.
  auto req = reinterpret_cast<httpRequestInternal*>(r);
  delete req;
}
}

} // namespace Golang
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
