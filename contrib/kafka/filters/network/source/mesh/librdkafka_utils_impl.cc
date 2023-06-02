#include "contrib/kafka/filters/network/source/mesh/librdkafka_utils_impl.h"

#include "source/common/common/macros.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace Kafka {
namespace Mesh {

RdKafka::Conf::ConfResult LibRdKafkaUtilsImpl::setConfProperty(RdKafka::Conf& conf,
                                                               const std::string& name,
                                                               const std::string& value,
                                                               std::string& errstr) const {
  return conf.set(name, value, errstr);
}

RdKafka::Conf::ConfResult
LibRdKafkaUtilsImpl::setConfDeliveryCallback(RdKafka::Conf& conf, RdKafka::DeliveryReportCb* dr_cb,
                                             std::string& errstr) const {
  return conf.set("dr_cb", dr_cb, errstr);
}

std::unique_ptr<RdKafka::Producer> LibRdKafkaUtilsImpl::createProducer(RdKafka::Conf* conf,
                                                                       std::string& errstr) const {
  return std::unique_ptr<RdKafka::Producer>(RdKafka::Producer::create(conf, errstr));
}

std::unique_ptr<RdKafka::KafkaConsumer>
LibRdKafkaUtilsImpl::createConsumer(RdKafka::Conf* conf, std::string& errstr) const {
  return std::unique_ptr<RdKafka::KafkaConsumer>(RdKafka::KafkaConsumer::create(conf, errstr));
}

RdKafka::Headers* LibRdKafkaUtilsImpl::convertHeaders(
    const std::vector<std::pair<absl::string_view, absl::string_view>>& headers) const {
  RdKafka::Headers* result = RdKafka::Headers::create();
  for (const auto& header : headers) {
    const RdKafka::Headers::Header librdkafka_header = {
        std::string(header.first), header.second.data(), header.second.length()};
    const auto ec = result->add(librdkafka_header);
    // This should never happen ('add' in 1.7.0 does not return any other error codes).
    if (RdKafka::ERR_NO_ERROR != ec) {
      delete result;
      return nullptr;
    }
  }
  return result;
}

void LibRdKafkaUtilsImpl::deleteHeaders(RdKafka::Headers* librdkafka_headers) const {
  delete librdkafka_headers;
}

const LibRdKafkaUtils& LibRdKafkaUtilsImpl::getDefaultInstance() {
  CONSTRUCT_ON_FIRST_USE(LibRdKafkaUtilsImpl);
}

} // namespace Mesh
} // namespace Kafka
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
