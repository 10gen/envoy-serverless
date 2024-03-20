#include "source/extensions/transport_sockets/tls/config.h"

#include "envoy/extensions/transport_sockets/tls/v3/cert.pb.h"
#include "envoy/extensions/transport_sockets/tls/v3/tls.pb.validate.h"

#include "source/common/protobuf/utility.h"
#include "source/common/tls/context_config_impl.h"
#include "source/common/tls/ssl_socket.h"

namespace Envoy {
namespace Extensions {
namespace TransportSockets {
namespace Tls {

Network::UpstreamTransportSocketFactoryPtr UpstreamSslSocketFactory::createTransportSocketFactory(
    const Protobuf::Message& message,
    Server::Configuration::TransportSocketFactoryContext& context) {
  auto client_config = std::make_unique<ClientContextConfigImpl>(
      MessageUtil::downcastAndValidate<
          const envoy::extensions::transport_sockets::tls::v3::UpstreamTlsContext&>(
          message, context.messageValidationVisitor()),
      context);
  return std::make_unique<ClientSslSocketFactory>(
      std::move(client_config), context.sslContextManager(), context.statsScope());
}

ProtobufTypes::MessagePtr UpstreamSslSocketFactory::createEmptyConfigProto() {
  return std::make_unique<envoy::extensions::transport_sockets::tls::v3::UpstreamTlsContext>();
}

LEGACY_REGISTER_FACTORY(UpstreamSslSocketFactory,
                        Server::Configuration::UpstreamTransportSocketConfigFactory, "tls");

Network::DownstreamTransportSocketFactoryPtr
DownstreamSslSocketFactory::createTransportSocketFactory(
    const Protobuf::Message& message, Server::Configuration::TransportSocketFactoryContext& context,
    const std::vector<std::string>& server_names) {
  auto server_config = std::make_unique<ServerContextConfigImpl>(
      MessageUtil::downcastAndValidate<
          const envoy::extensions::transport_sockets::tls::v3::DownstreamTlsContext&>(
          message, context.messageValidationVisitor()),
      context);
  return std::make_unique<ServerSslSocketFactory>(
      std::move(server_config), context.sslContextManager(), context.statsScope(), server_names);
}

ProtobufTypes::MessagePtr DownstreamSslSocketFactory::createEmptyConfigProto() {
  return std::make_unique<envoy::extensions::transport_sockets::tls::v3::DownstreamTlsContext>();
}

LEGACY_REGISTER_FACTORY(DownstreamSslSocketFactory,
                        Server::Configuration::DownstreamTransportSocketConfigFactory, "tls");

} // namespace Tls
} // namespace TransportSockets
} // namespace Extensions
} // namespace Envoy
