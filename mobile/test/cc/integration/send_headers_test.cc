#include "test/test_common/utility.h"

#include "absl/synchronization/notification.h"
#include "gtest/gtest.h"
#include "library/cc/engine_builder.h"
#include "library/cc/envoy_error.h"
#include "library/cc/request_headers_builder.h"
#include "library/cc/request_method.h"

namespace Envoy {
namespace {

struct Status {
  int status_code;
  bool end_stream;
};

TEST(SendHeadersTest, CanSendHeaders) {
  Platform::EngineBuilder engine_builder;
  engine_builder.addNativeFilter(
      "test_remote_response",
      "{'@type': "
      "type.googleapis.com/"
      "envoymobile.extensions.filters.http.test_remote_response.TestRemoteResponse}");
  absl::Notification engine_running;
  Platform::EngineSharedPtr engine = engine_builder.addLogLevel(Platform::LogLevel::debug)
                                         .setOnEngineRunning([&]() { engine_running.Notify(); })
                                         .build();
  engine_running.WaitForNotification();

  Status status;
  absl::Notification stream_complete;
  auto stream_prototype = engine->streamClient()->newStreamPrototype();
  Platform::StreamSharedPtr stream =
      (*stream_prototype)
          .setOnHeaders(
              [&](Platform::ResponseHeadersSharedPtr headers, bool end_stream, envoy_stream_intel) {
                status.status_code = headers->httpStatus();
                status.end_stream = end_stream;
              })
          .setOnData([&](envoy_data, bool end_stream) { status.end_stream = end_stream; })
          .setOnComplete(
              [&](envoy_stream_intel, envoy_final_stream_intel) { stream_complete.Notify(); })
          .setOnError([&](Platform::EnvoyErrorSharedPtr, envoy_stream_intel,
                          envoy_final_stream_intel) { stream_complete.Notify(); })
          .setOnCancel(
              [&](envoy_stream_intel, envoy_final_stream_intel) { stream_complete.Notify(); })
          .start();

  Platform::RequestHeadersBuilder request_headers_builder(Platform::RequestMethod::GET, "https",
                                                          "example.com", "/");
  auto request_headers = request_headers_builder.build();
  auto request_headers_ptr =
      Platform::RequestHeadersSharedPtr(new Platform::RequestHeaders(request_headers));
  stream->sendHeaders(request_headers_ptr, true);
  stream_complete.WaitForNotification();

  EXPECT_EQ(status.status_code, 200);
  EXPECT_EQ(status.end_stream, true);

  engine->terminate();
}

} // namespace
} // namespace Envoy
