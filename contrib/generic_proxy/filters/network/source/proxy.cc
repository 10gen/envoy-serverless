#include "contrib/generic_proxy/filters/network/source/proxy.h"

#include "envoy/common/exception.h"
#include "envoy/network/connection.h"

#include "source/common/config/utility.h"
#include "source/common/protobuf/protobuf.h"
#include "source/common/stream_info/stream_info_impl.h"

#include "contrib/generic_proxy/filters/network/source/interface/config.h"
#include "contrib/generic_proxy/filters/network/source/interface/filter.h"
#include "contrib/generic_proxy/filters/network/source/route.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace GenericProxy {

namespace {

Tracing::Decision tracingDecision(const Tracing::ConnectionManagerTracingConfig& tracing_config,
                                  Runtime::Loader& runtime) {
  bool traced = runtime.snapshot().featureEnabled("tracing.random_sampling",
                                                  tracing_config.getRandomSampling());

  if (traced) {
    return {Tracing::Reason::Sampling, true};
  }
  return {Tracing::Reason::NotTraceable, false};
}

} // namespace

ActiveStream::ActiveStream(Filter& parent, RequestPtr request)
    : parent_(parent), downstream_request_stream_(std::move(request)),
      stream_info_(parent_.time_source_,
                   parent_.callbacks_->connection().connectionInfoProviderSharedPtr()) {

  connection_manager_tracing_config_ = parent_.config_->tracingConfig();

  auto tracer = parent_.config_->tracingProvider();

  if (!connection_manager_tracing_config_.has_value() || !tracer.has_value()) {
    return;
  }

  auto decision = tracingDecision(connection_manager_tracing_config_.value(), parent_.runtime_);
  if (decision.traced) {
    stream_info_.setTraceReason(decision.reason);
  }
  active_span_ = tracer->startSpan(*this, *downstream_request_stream_, stream_info_, decision);
}

Tracing::OperationName ActiveStream::operationName() const {
  ASSERT(connection_manager_tracing_config_.has_value());
  return connection_manager_tracing_config_->operationName();
}

const Tracing::CustomTagMap* ActiveStream::customTags() const {
  ASSERT(connection_manager_tracing_config_.has_value());
  return &connection_manager_tracing_config_->getCustomTags();
}

bool ActiveStream::verbose() const {
  ASSERT(connection_manager_tracing_config_.has_value());
  return connection_manager_tracing_config_->verbose();
}

uint32_t ActiveStream::maxPathTagLength() const {
  ASSERT(connection_manager_tracing_config_.has_value());
  return connection_manager_tracing_config_->maxPathTagLength();
}

Envoy::Event::Dispatcher& ActiveStream::dispatcher() { return parent_.connection().dispatcher(); }
const CodecFactory& ActiveStream::downstreamCodec() { return parent_.config_->codecFactory(); }
void ActiveStream::resetStream() {
  if (active_stream_reset_) {
    return;
  }
  active_stream_reset_ = true;
  parent_.deferredStream(*this);
}

void ActiveStream::sendLocalReply(Status status, ResponseUpdateFunction&& func) {
  ASSERT(parent_.creator_ != nullptr);
  local_or_upstream_response_stream_ =
      parent_.creator_->response(status, *downstream_request_stream_);

  ASSERT(local_or_upstream_response_stream_ != nullptr);

  if (func != nullptr) {
    func(*local_or_upstream_response_stream_);
  }

  parent_.sendReplyDownstream(*local_or_upstream_response_stream_, *this);
}

void ActiveStream::continueDecoding() {
  if (active_stream_reset_ || downstream_request_stream_ == nullptr) {
    return;
  }

  if (cached_route_entry_ == nullptr) {
    cached_route_entry_ = parent_.config_->routeEntry(*downstream_request_stream_);
  }

  ASSERT(downstream_request_stream_ != nullptr);
  for (; next_decoder_filter_index_ < decoder_filters_.size();) {
    auto status = decoder_filters_[next_decoder_filter_index_]->filter_->onStreamDecoded(
        *downstream_request_stream_);
    next_decoder_filter_index_++;
    if (status == FilterStatus::StopIteration) {
      break;
    }
  }
  if (next_decoder_filter_index_ == decoder_filters_.size()) {
    ENVOY_LOG(debug, "Complete decoder filters");
  }
}

void ActiveStream::upstreamResponse(ResponsePtr response) {
  local_or_upstream_response_stream_ = std::move(response);
  continueEncoding();
}

void ActiveStream::completeDirectly() { parent_.deferredStream(*this); };

void ActiveStream::continueEncoding() {
  if (active_stream_reset_ || local_or_upstream_response_stream_ == nullptr) {
    return;
  }

  ASSERT(local_or_upstream_response_stream_ != nullptr);
  for (; next_encoder_filter_index_ < encoder_filters_.size();) {
    auto status = encoder_filters_[next_encoder_filter_index_]->filter_->onStreamEncoded(
        *local_or_upstream_response_stream_);
    next_encoder_filter_index_++;
    if (status == FilterStatus::StopIteration) {
      break;
    }
  }

  if (next_encoder_filter_index_ == encoder_filters_.size()) {
    ENVOY_LOG(debug, "Complete decoder filters");
    parent_.sendReplyDownstream(*local_or_upstream_response_stream_, *this);
  }
}

void ActiveStream::onEncodingSuccess(Buffer::Instance& buffer, bool close_connection) {
  ASSERT(parent_.connection().state() == Network::Connection::State::Open);
  parent_.connection().write(buffer, close_connection);
  parent_.deferredStream(*this);
}

void ActiveStream::initializeFilterChain(FilterChainFactory& factory) {
  factory.createFilterChain(*this);
  // Reverse the encoder filter chain so that the first encoder filter is the last filter in the
  // chain.
  std::reverse(encoder_filters_.begin(), encoder_filters_.end());
}

void ActiveStream::completeRequest() {
  stream_info_.onRequestComplete();

  if (active_span_) {
    Tracing::TracerUtility::finalizeSpan(*active_span_, *downstream_request_stream_, stream_info_,
                                         *this, false);
  }

  for (auto& filter : decoder_filters_) {
    filter->filter_->onDestroy();
  }
  for (auto& filter : encoder_filters_) {
    if (filter->isDualFilter()) {
      continue;
    }
    filter->filter_->onDestroy();
  }
}

Envoy::Network::FilterStatus Filter::onData(Envoy::Buffer::Instance& data, bool) {
  if (downstream_connection_closed_) {
    return Envoy::Network::FilterStatus::StopIteration;
  }

  decoder_->decode(data);
  return Envoy::Network::FilterStatus::StopIteration;
}

void Filter::onDecodingSuccess(RequestPtr request) { newDownstreamRequest(std::move(request)); }

void Filter::onDecodingFailure() {
  resetStreamsForUnexpectedError();
  connection().close(Network::ConnectionCloseType::FlushWrite);
}

void Filter::sendReplyDownstream(Response& response, ResponseEncoderCallback& callback) {
  response_encoder_->encode(response, callback);
}

void Filter::newDownstreamRequest(RequestPtr request) {
  auto stream = std::make_unique<ActiveStream>(*this, std::move(request));
  auto raw_stream = stream.get();
  LinkedList::moveIntoList(std::move(stream), active_streams_);

  // Initialize filter chian.
  raw_stream->initializeFilterChain(*config_);
  // Start request.
  raw_stream->continueDecoding();
}

void Filter::deferredStream(ActiveStream& stream) {
  stream.completeRequest();

  if (!stream.inserted()) {
    return;
  }
  callbacks_->connection().dispatcher().deferredDelete(stream.removeFromList(active_streams_));
  mayBeDrainClose();
}

void Filter::resetStreamsForUnexpectedError() {
  while (!active_streams_.empty()) {
    active_streams_.front()->resetStream();
  }
}

void Filter::mayBeDrainClose() {
  if (drain_decision_.drainClose() && active_streams_.empty()) {
    onDrainCloseAndNoActiveStreams();
  }
}

// Default implementation for connection draining.
void Filter::onDrainCloseAndNoActiveStreams() {
  connection().close(Network::ConnectionCloseType::FlushWrite);
}

} // namespace GenericProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
