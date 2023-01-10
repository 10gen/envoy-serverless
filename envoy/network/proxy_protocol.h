#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <iterator>

#include "source/common/common/fmt.h"
#include "envoy/network/address.h"

namespace Envoy {
namespace Network {

struct ProxyProtocolTLV {
  const uint8_t type;
  const std::string value;
};

using ProxyProtocolTLVVector = std::vector<ProxyProtocolTLV>;

struct ProxyProtocolData {
  const Network::Address::InstanceConstSharedPtr src_addr_;
  const Network::Address::InstanceConstSharedPtr dst_addr_;
  const ProxyProtocolTLVVector tlv_vector_{};
  std::string asStringForHash() const {
    return std::string(src_addr_ ? src_addr_->asString() : "null") +
           (dst_addr_ ? dst_addr_->asString() : "null");
  }
  // std::string getTlvVectorAsString() const {
  //   // TODO betsy: edit to convert the tlvs vector into a string

  //   std::ostringstream oss;
  //   if (!tlv_vector_.empty()) {
  //     // oss << tlv_vector_.begin();
  //     // Convert all but the last element to avoid a trailing ","
  //     for(auto & tlv : tlv_vector_) {

  //     // ENVOY_LOG(debug, "type: {}, value: {}, ", tlv.type, tlv.value);
  //     // using str.data() and str.length() to encode the value 
  //       oss << fmt::format("type: {}, value: {}, ", tlv.type, Envoy::Hex::encode(tlv.value.data(), tlv.value.length()));
  //     }

  //     // std::copy(tlv_vector_.begin(), tlv_vector_.end()-1,
  //     //     std::ostream_iterator<ProxyProtocolTLVVector>(oss, ","));

  //     // Now add the last element with no delimiter
  //     // oss << tlv_vector_.back();
  //   }
  //   // for(auto & tlv : tlv_vector_) {
      
  //   return oss.str();
  // }
};

} // namespace Network
} // namespace Envoy
