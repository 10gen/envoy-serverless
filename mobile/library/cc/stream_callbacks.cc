#include "stream_callbacks.h"

#include "library/cc/bridge_utility.h"
#include "library/cc/response_headers_builder.h"
#include "library/cc/response_trailers_builder.h"
#include "library/common/data/utility.h"

namespace Envoy {
namespace Platform {

namespace {

void c_on_headers(envoy_headers headers, bool end_stream, envoy_stream_intel intel, void* context) {
  auto stream_callbacks = *static_cast<StreamCallbacksSharedPtr*>(context);
  if (stream_callbacks->on_headers.has_value()) {
    auto raw_headers = envoyHeadersAsRawHeaderMap(headers);
    ResponseHeadersBuilder builder;
    for (const auto& pair : raw_headers) {
      if (pair.first == ":status") {
        builder.addHttpStatus(std::stoi(pair.second[0]));
      }
      builder.set(pair.first, pair.second);
    }
    auto on_headers = stream_callbacks->on_headers.value();
    on_headers(builder.build(), end_stream, intel);
  } else {
    release_envoy_headers(headers);
  }
}

void c_on_data(envoy_data data, bool end_stream, envoy_stream_intel, void* context) {
  auto stream_callbacks = *static_cast<StreamCallbacksSharedPtr*>(context);
  if (stream_callbacks->on_data.has_value()) {
    auto on_data = stream_callbacks->on_data.value();
    on_data(data, end_stream);
  } else {
    release_envoy_data(data);
  }
}

void c_on_trailers(envoy_headers metadata, envoy_stream_intel intel, void* context) {
  auto stream_callbacks = *static_cast<StreamCallbacksSharedPtr*>(context);
  if (stream_callbacks->on_trailers.has_value()) {
    auto raw_headers = envoyHeadersAsRawHeaderMap(metadata);
    ResponseTrailersBuilder builder;
    for (const auto& pair : raw_headers) {
      builder.set(pair.first, pair.second);
    }
    auto on_trailers = stream_callbacks->on_trailers.value();
    on_trailers(builder.build(), intel);
  } else {
    release_envoy_headers(metadata);
  }
}

void c_on_error(envoy_error raw_error, envoy_stream_intel intel,
                envoy_final_stream_intel final_intel, void* context) {
  auto stream_callbacks_ptr = static_cast<StreamCallbacksSharedPtr*>(context);
  auto stream_callbacks = *stream_callbacks_ptr;
  if (stream_callbacks->on_error.has_value()) {
    EnvoyErrorSharedPtr error = std::make_shared<EnvoyError>();
    error->error_code = raw_error.error_code;
    error->message = Data::Utility::copyToString(raw_error.message);
    error->attempt_count = absl::optional<int>(raw_error.attempt_count);
    auto on_error = stream_callbacks->on_error.value();
    on_error(error, intel, final_intel);
  }
  release_envoy_error(raw_error);
  delete stream_callbacks_ptr;
}

void c_on_complete(envoy_stream_intel intel, envoy_final_stream_intel final_intel, void* context) {
  auto stream_callbacks_ptr = static_cast<StreamCallbacksSharedPtr*>(context);
  auto stream_callbacks = *stream_callbacks_ptr;
  if (stream_callbacks->on_complete.has_value()) {
    auto on_complete = stream_callbacks->on_complete.value();
    on_complete(intel, final_intel);
  }
  delete stream_callbacks_ptr;
}

void c_on_cancel(envoy_stream_intel intel, envoy_final_stream_intel final_intel, void* context) {
  auto stream_callbacks_ptr = static_cast<StreamCallbacksSharedPtr*>(context);
  auto stream_callbacks = *stream_callbacks_ptr;
  if (stream_callbacks->on_cancel.has_value()) {
    auto on_cancel = stream_callbacks->on_cancel.value();
    on_cancel(intel, final_intel);
  }
  delete stream_callbacks_ptr;
}

void c_on_send_window_available(envoy_stream_intel intel, void* context) {
  auto stream_callbacks_ptr = static_cast<StreamCallbacksSharedPtr*>(context);
  auto stream_callbacks = *stream_callbacks_ptr;
  if (stream_callbacks->on_send_window_available.has_value()) {
    auto on_send_window_available = stream_callbacks->on_send_window_available.value();
    on_send_window_available(intel);
  }
}

} // namespace

envoy_http_callbacks StreamCallbacks::asEnvoyHttpCallbacks() {
  return envoy_http_callbacks{
      &c_on_headers,
      &c_on_data,
      nullptr,
      &c_on_trailers,
      &c_on_error,
      &c_on_complete,
      &c_on_cancel,
      &c_on_send_window_available,
      // Each of the function pointers in the returned `envoy_http_callbacks` struct have a
      // `void* context` parameter. The value of the `context` field of this struct is passed in as
      // the value of that parameter. Because this context passes through JNI, the context field
      // can not be a smart pointer and must instead be a standard C-pointer. However, the
      // `StreamCallbacks` object is reference counted and so will be destroyed when the final
      // shared_ptr is destroyed. So in order to make sure that it lives long enough, the `context`
      // field here stores a pointer to a newly created `shared_ptr` which will not go out of scope
      // when this method returns. When the `StreamCallbacks` is no longer need (c_on_complete,
      // c_on_cancel, c_on_error), this new `shared_ptr` will need to be deleted.
      new StreamCallbacksSharedPtr(shared_from_this()),
  };
}

} // namespace Platform
} // namespace Envoy
