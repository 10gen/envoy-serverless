#pragma once

#include <string>

#include "envoy/network/proxy_protocol.h"
#include "envoy/stream_info/filter_state.h"

namespace Envoy {
namespace Network {

/**
 * PROXY protocol info to be used in connections.
 */
class ProxyProtocolFilterState : public StreamInfo::FilterState::Object {
public:
  ProxyProtocolFilterState(Network::ProxyProtocolData options) : options_(options) {}
  const Network::ProxyProtocolData& value() const { return options_; }
  static const std::string& key();

private:
  const Network::ProxyProtocolData options_;

  // std::string getTlvVectorAsString();
// TODO Betsy: need to add a method here to parse the TLV vector into strings that can be printed as headers
//  Add the header for the function here and then implement it in the cc file 
};

} // namespace Network
} // namespace Envoy
