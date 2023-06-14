#include "source/extensions/upstreams/tcp/generic/config.h"

#include "envoy/stream_info/bool_accessor.h"
#include "envoy/upstream/cluster_manager.h"

#include "source/common/http/codec_client.h"
#include "source/common/tcp_proxy/upstream.h"

namespace Envoy {
namespace Extensions {
namespace Upstreams {
namespace Tcp {
namespace Generic {

TcpProxy::GenericConnPoolPtr GenericConnPoolFactory::createGenericConnPool(
    Upstream::ThreadLocalCluster& thread_local_cluster,
    TcpProxy::TunnelingConfigHelperOptConstRef config, Upstream::LoadBalancerContext* context,
    Envoy::Tcp::ConnectionPool::UpstreamCallbacks& upstream_callbacks,
    StreamInfo::StreamInfo& downstream_info) const {
  if (config.has_value() && !disableTunnelingByFilterState(downstream_info)) {
    Http::CodecType pool_type;
    if ((thread_local_cluster.info()->features() & Upstream::ClusterInfo::Features::HTTP2) != 0) {
      pool_type = Http::CodecType::HTTP2;
    } else if ((thread_local_cluster.info()->features() & Upstream::ClusterInfo::Features::HTTP3) !=
               0) {
      pool_type = Http::CodecType::HTTP3;
    } else {
      pool_type = Http::CodecType::HTTP1;
    }
    auto ret = std::make_unique<TcpProxy::HttpConnPool>(
        thread_local_cluster, context, *config, upstream_callbacks, pool_type, downstream_info);
    return (ret->valid() ? std::move(ret) : nullptr);
  }
  auto ret =
      std::make_unique<TcpProxy::TcpConnPool>(thread_local_cluster, context, upstream_callbacks);
  return (ret->valid() ? std::move(ret) : nullptr);
}

bool GenericConnPoolFactory::disableTunnelingByFilterState(
    StreamInfo::StreamInfo& downstream_info) const {
  const StreamInfo::BoolAccessor* disable_tunneling =
      downstream_info.filterState()->getDataReadOnly<StreamInfo::BoolAccessor>(
          TcpProxy::DisableTunnelingFilterStateKey);

  return disable_tunneling != nullptr && disable_tunneling->value() == true;
}

REGISTER_FACTORY(GenericConnPoolFactory, TcpProxy::GenericConnPoolFactory);

} // namespace Generic
} // namespace Tcp
} // namespace Upstreams
} // namespace Extensions
} // namespace Envoy
