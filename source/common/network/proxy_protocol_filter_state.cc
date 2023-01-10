#include "source/common/network/proxy_protocol_filter_state.h"

#include <string>
#include <cstdint>
#include <vector>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <iterator>

#include "source/common/common/fmt.h"
#include "source/common/common/hex.h"
#include "source/common/common/macros.h"

namespace Envoy {
namespace Network {

const std::string& ProxyProtocolFilterState::key() {
  CONSTRUCT_ON_FIRST_USE(std::string, "envoy.network.proxy_protocol_options");
}

} // namespace Network
} // namespace Envoy
