#include <regex>

#include "envoy/config/bootstrap/v3/bootstrap.pb.h"
#include "envoy/event/dispatcher.h"
#include "envoy/extensions/access_loggers/file/v3/file.pb.h"
#include "envoy/network/connection.h"
#include "envoy/network/filter.h"
#include "envoy/server/filter_config.h"

#include "source/common/buffer/buffer_impl.h"
#include "source/common/router/string_accessor_impl.h"
#include "source/extensions/filters/network/common/factory_base.h"
#include "source/extensions/transport_sockets/common/passthrough.h"

#include "test/integration/http_integration.h"
#include "test/integration/integration.h"
#include "test/integration/upstream_socket.pb.h"
#include "test/integration/upstream_socket.pb.validate.h"
#include "test/integration/utility.h"
#include "test/test_common/registry.h"
#include "test/test_common/utility.h"

#include "gtest/gtest.h"

/**
 * Integration tests for upstream filter state.
 */

namespace Envoy {

/*
 * A custom socket implementation to enable intercepting upstream events and data.
 */
class Socket : public Extensions::TransportSockets::PassthroughSocket {
public:
  Socket(Network::TransportSocketPtr inner_socket) : PassthroughSocket(std::move(inner_socket)) {}

  void setTransportSocketCallbacks(Network::TransportSocketCallbacks& callbacks) override {
    callbacks_ = &callbacks;

    transport_socket_->setTransportSocketCallbacks(callbacks);
  }

  void onConnected() override {
    const Envoy::StreamInfo::FilterStateSharedPtr& filter_state =
        callbacks_->connection().streamInfo().filterState();
    filter_state->setData("test_key", std::make_unique<Router::StringAccessorImpl>("test_value"),
                          StreamInfo::FilterState::StateType::ReadOnly);
    transport_socket_->onConnected();
  }

  void closeSocket(Network::ConnectionEvent event) override {
    transport_socket_->closeSocket(event);
  }

  Network::TransportSocketCallbacks* callbacks_{};
};

class SocketFactory : public Extensions::TransportSockets::PassthroughFactory {
public:
  SocketFactory(Network::UpstreamTransportSocketFactoryPtr&& inner_factory)
      : PassthroughFactory(std::move(inner_factory)) {}

  Network::TransportSocketPtr
  createTransportSocket(Network::TransportSocketOptionsConstSharedPtr options,
                        Upstream::HostDescriptionConstSharedPtr host) const override {
    auto inner_socket = transport_socket_factory_->createTransportSocket(options, host);
    if (inner_socket == nullptr) {
      return nullptr;
    }
    return std::make_unique<Socket>(std::move(inner_socket));
  }
};

class SocketConfigFactory : public Server::Configuration::UpstreamTransportSocketConfigFactory {
public:
  std::string name() const override { return "envoy.test.integration.upstreamt_socket"; }

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<test::integration::upstream_socket::v3::Config>();
  }

  Network::UpstreamTransportSocketFactoryPtr createTransportSocketFactory(
      const Protobuf::Message& config,
      Server::Configuration::TransportSocketFactoryContext& context) override {
    const auto& outer_config =
        MessageUtil::downcastAndValidate<const test::integration::upstream_socket::v3::Config&>(
            config, context.messageValidationVisitor());

    auto& inner_config_factory = Envoy::Config::Utility::getAndCheckFactory<
        Server::Configuration::UpstreamTransportSocketConfigFactory>(
        outer_config.transport_socket());

    ProtobufTypes::MessagePtr inner_factory_config =
        Envoy::Config::Utility::translateToFactoryConfig(outer_config.transport_socket(),
                                                         context.messageValidationVisitor(),
                                                         inner_config_factory);
    auto inner_transport_factory =
        inner_config_factory.createTransportSocketFactory(*inner_factory_config, context);
    return std::make_unique<SocketFactory>(std::move(inner_transport_factory));
  }
};

class UpstreamAccessLogTest : public testing::TestWithParam<Network::Address::IpVersion>,
                              public HttpIntegrationTest {
public:
  UpstreamAccessLogTest() : HttpIntegrationTest(Http::CodecType::HTTP1, GetParam()) {}
  SocketConfigFactory socket_factory_;

  Registry::InjectFactory<Server::Configuration::UpstreamTransportSocketConfigFactory>
      registered_socket_factory_{socket_factory_};
};

INSTANTIATE_TEST_SUITE_P(Params, UpstreamAccessLogTest,
                         testing::ValuesIn(TestEnvironment::getIpVersionsForTest()),
                         TestUtility::ipTestParamsToString);

/*
 * Verifies that the Http Router's `upstream_log` correctly reflects the upstream filter state data
 * when the access log format has `UPSTREAM_FILTER_STATE` specifier.
 */
TEST_P(UpstreamAccessLogTest, UpstreamFilterState) {
  auto log_file = TestEnvironment::temporaryPath(TestUtility::uniqueFilename());

  config_helper_.addConfigModifier([](envoy::config::bootstrap::v3::Bootstrap& bootstrap) {
    envoy::config::core::v3::TransportSocket inner_socket;
    inner_socket.set_name("envoy.transport_sockets.raw_buffer");
    test::integration::upstream_socket::v3::Config proto_config;
    proto_config.mutable_transport_socket()->MergeFrom(inner_socket);

    auto* cluster_transport_socket =
        bootstrap.mutable_static_resources()->mutable_clusters(0)->mutable_transport_socket();
    cluster_transport_socket->set_name("envoy.test.integration.upstreamt_socket");
    cluster_transport_socket->mutable_typed_config()->PackFrom(proto_config);
  });
  config_helper_.addConfigModifier(
      [&](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
              hcm) {
        auto* typed_config =
            hcm.mutable_http_filters(hcm.http_filters_size() - 1)->mutable_typed_config();

        envoy::extensions::filters::http::router::v3::Router router_config;
        auto* upstream_log_config = router_config.add_upstream_log();
        upstream_log_config->set_name("accesslog");
        envoy::extensions::access_loggers::file::v3::FileAccessLog access_log_config;
        access_log_config.set_path(log_file);
        access_log_config.mutable_log_format()->mutable_text_format_source()->set_inline_string(
            "%UPSTREAM_FILTER_STATE(test_key)%\n");
        upstream_log_config->mutable_typed_config()->PackFrom(access_log_config);
        typed_config->PackFrom(router_config);
      });

  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  Http::TestRequestHeaderMapImpl headers{
      {":method", "POST"}, {":path", "/api"}, {":authority", "host"}, {":scheme", "http"}};
  auto response = codec_client_->makeRequestWithBody(headers, "hello!");

  waitForNextUpstreamRequest({}, std::chrono::milliseconds(300000));

  EXPECT_TRUE(upstream_request_->complete());
  EXPECT_EQ("hello!", upstream_request_->body().toString());

  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "200"}}, false);
  Buffer::OwnedImpl response_data{"greetings"};
  upstream_request_->encodeData(response_data, true);

  ASSERT_TRUE(response->waitForEndStream());
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
  EXPECT_EQ("greetings", response->body());

  EXPECT_THAT(waitForAccessLog(log_file), testing::HasSubstr("test_value"));
}

} // namespace Envoy
