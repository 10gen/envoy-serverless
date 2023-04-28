#include "test/mocks/server/factory_context.h"

#include "contrib/generic_proxy/filters/network/source/match.h"
#include "contrib/generic_proxy/filters/network/test/fake_codec.h"
#include "gtest/gtest.h"

using testing::NiceMock;

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace GenericProxy {
namespace {

TEST(ServiceMatchDataInputTest, ServiceMatchDataInputTest) {
  NiceMock<Server::Configuration::MockFactoryContext> factory_context;
  ServiceMatchDataInputFactory factory;
  auto proto_config = factory.createEmptyConfigProto();
  auto input =
      factory.createDataInputFactoryCb(*proto_config, factory_context.messageValidationVisitor())();

  FakeStreamCodecFactory::FakeRequest request;

  EXPECT_EQ("", absl::get<std::string>(input->get(request).data_));

  request.host_ = "fake_host_as_service";

  EXPECT_EQ("fake_host_as_service", absl::get<std::string>(input->get(request).data_));
}

TEST(MethodMatchDataInputTest, MethodMatchDataInputTest) {
  NiceMock<Server::Configuration::MockFactoryContext> factory_context;
  MethodMatchDataInputFactory factory;
  auto proto_config = factory.createEmptyConfigProto();
  auto input =
      factory.createDataInputFactoryCb(*proto_config, factory_context.messageValidationVisitor())();

  FakeStreamCodecFactory::FakeRequest request;

  EXPECT_EQ("", absl::get<std::string>(input->get(request).data_));

  request.method_ = "fake_method";

  EXPECT_EQ("fake_method", absl::get<std::string>(input->get(request).data_));
}

TEST(PropertyMatchDataInputTest, PropertyMatchDataInputTest) {
  NiceMock<Server::Configuration::MockFactoryContext> factory_context;
  PropertyMatchDataInputFactory factory;
  auto proto_config = factory.createEmptyConfigProto();

  auto& typed_proto_config = static_cast<PropertyDataInputProto&>(*proto_config);

  typed_proto_config.set_property_name("key_0");

  auto input =
      factory.createDataInputFactoryCb(*proto_config, factory_context.messageValidationVisitor())();

  FakeStreamCodecFactory::FakeRequest request;

  EXPECT_TRUE(absl::holds_alternative<absl::monostate>(input->get(request).data_));

  request.data_["key_0"] = "value_0";

  EXPECT_EQ("value_0", absl::get<std::string>(input->get(request).data_));
}

} // namespace
} // namespace GenericProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
