#pragma once

#include "envoy/extensions/transport_sockets/tcp_stats/v3/tcp_stats.pb.h"
#include "envoy/server/transport_socket_config.h"

#include "source/extensions/transport_sockets/common/passthrough.h"
#include "source/extensions/transport_sockets/tcp_stats/tcp_stats.h"

namespace Envoy {
namespace Extensions {
namespace TransportSockets {
namespace TcpStats {

<<<<<<< HEAD
class TcpStatsSocketFactory {
public:
  TcpStatsSocketFactory(Server::Configuration::TransportSocketFactoryContext& context,
                        const envoy::extensions::transport_sockets::tcp_stats::v3::Config& config);

protected:
=======
class TcpStatsSocketFactory : public PassthroughFactory {
public:
  TcpStatsSocketFactory(Server::Configuration::TransportSocketFactoryContext& context,
                        const envoy::extensions::transport_sockets::tcp_stats::v3::Config& config,
                        Network::TransportSocketFactoryPtr&& inner_factory);

  Network::TransportSocketPtr
  createTransportSocket(Network::TransportSocketOptionsConstSharedPtr options) const override;

private:
>>>>>>> parent of fe29083d4b... Apply patch all-remove-tcpstats.patch to remove TCP stats for CentOS 7.
#if defined(__linux__)
  ConfigConstSharedPtr config_;
#endif
};

<<<<<<< HEAD
class UpstreamTcpStatsSocketFactory : public TcpStatsSocketFactory, public PassthroughFactory {
public:
  UpstreamTcpStatsSocketFactory(
      Server::Configuration::TransportSocketFactoryContext& context,
      const envoy::extensions::transport_sockets::tcp_stats::v3::Config& config,
      Network::UpstreamTransportSocketFactoryPtr&& inner_factory);

  Network::TransportSocketPtr
  createTransportSocket(Network::TransportSocketOptionsConstSharedPtr options,
                        Upstream::HostDescriptionConstSharedPtr host) const override;
};

class DownstreamTcpStatsSocketFactory : public TcpStatsSocketFactory,
                                        public DownstreamPassthroughFactory {
public:
  DownstreamTcpStatsSocketFactory(
      Server::Configuration::TransportSocketFactoryContext& context,
      const envoy::extensions::transport_sockets::tcp_stats::v3::Config& config,
      Network::DownstreamTransportSocketFactoryPtr&& inner_factory);

  Network::TransportSocketPtr createDownstreamTransportSocket() const override;
};

=======
>>>>>>> parent of fe29083d4b... Apply patch all-remove-tcpstats.patch to remove TCP stats for CentOS 7.
} // namespace TcpStats
} // namespace TransportSockets
} // namespace Extensions
} // namespace Envoy
