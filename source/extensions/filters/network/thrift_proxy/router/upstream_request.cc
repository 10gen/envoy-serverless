#include "source/extensions/filters/network/thrift_proxy/router/upstream_request.h"

#include "source/extensions/filters/network/thrift_proxy/app_exception_impl.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace ThriftProxy {
namespace Router {

UpstreamRequest::UpstreamRequest(RequestOwner& parent, Upstream::TcpPoolData& pool_data,
                                 MessageMetadataSharedPtr& metadata, TransportType transport_type,
                                 ProtocolType protocol_type, bool close_downstream_on_error)
    : parent_(parent), stats_(parent.stats()), conn_pool_data_(pool_data), metadata_(metadata),
      transport_(NamedTransportConfigFactory::getFactory(transport_type).createTransport()),
      protocol_(NamedProtocolConfigFactory::getFactory(protocol_type).createProtocol()),
      request_complete_(false), response_started_(false), response_complete_(false),
      draining_(false), response_underflow_(false), charged_response_timing_(false),
      close_downstream_on_error_(close_downstream_on_error) {}

UpstreamRequest::~UpstreamRequest() {
  if (conn_pool_handle_) {
    conn_pool_handle_->cancel(Tcp::ConnectionPool::CancelPolicy::Default);
  }
}

FilterStatus UpstreamRequest::start() {
  Tcp::ConnectionPool::Cancellable* handle = conn_pool_data_.newConnection(*this);
  if (handle) {
    // Pause while we wait for a connection.
    conn_pool_handle_ = handle;
    return FilterStatus::StopIteration;
  }

  if (upgrade_response_ != nullptr) {
    // Pause while we wait for an upgrade response.
    return FilterStatus::StopIteration;
  }

  if (upstream_host_ == nullptr) {
    return FilterStatus::StopIteration;
  }

  return FilterStatus::Continue;
}

void UpstreamRequest::releaseConnection(const bool close) {
  ENVOY_LOG(debug, "releasing connection, close: {}", close);
  if (conn_pool_handle_) {
    conn_pool_handle_->cancel(Tcp::ConnectionPool::CancelPolicy::Default);
    conn_pool_handle_ = nullptr;
  }

  conn_state_ = nullptr;

  // The event triggered by close will also release this connection so clear conn_data_ before
  // closing.
  auto conn_data = std::move(conn_data_);
  if (close && conn_data != nullptr) {
    conn_data->connection().close(Network::ConnectionCloseType::NoFlush);
  }
}

void UpstreamRequest::resetStream() {
  ENVOY_LOG(debug, "reset stream");
  releaseConnection(true);
}

void UpstreamRequest::onPoolFailure(ConnectionPool::PoolFailureReason reason, absl::string_view,
                                    Upstream::HostDescriptionConstSharedPtr host) {
  ENVOY_LOG(debug, "on pool failure");
  conn_pool_handle_ = nullptr;

  // Mimic an upstream reset.
  onUpstreamHostSelected(host);
  if (!onResetStream(reason)) {
    parent_.continueDecoding();
  }
}

void UpstreamRequest::onPoolReady(Tcp::ConnectionPool::ConnectionDataPtr&& conn_data,
                                  Upstream::HostDescriptionConstSharedPtr host) {
  // Only invoke continueDecoding if we'd previously stopped the filter chain.
  bool continue_decoding = conn_pool_handle_ != nullptr;

  onUpstreamHostSelected(host);
  host->outlierDetector().putResult(Upstream::Outlier::Result::LocalOriginConnectSuccess);

  conn_data_ = std::move(conn_data);
  conn_data_->addUpstreamCallbacks(parent_.upstreamCallbacks());
  conn_pool_handle_ = nullptr;

  conn_state_ = conn_data_->connectionStateTyped<ThriftConnectionState>();
  if (conn_state_ == nullptr) {
    conn_data_->setConnectionState(std::make_unique<ThriftConnectionState>());
    conn_state_ = conn_data_->connectionStateTyped<ThriftConnectionState>();
  }

  if (protocol_->supportsUpgrade()) {
    auto& buffer = parent_.buffer();
    upgrade_response_ = protocol_->attemptUpgrade(*transport_, *conn_state_, buffer);
    if (upgrade_response_ != nullptr) {
      parent_.addSize(buffer.length());
      conn_data_->connection().write(buffer, false);
      return;
    }
  }

  onRequestStart(continue_decoding);
}

void UpstreamRequest::handleUpgradeResponse(Buffer::Instance& data) {
  ENVOY_LOG(trace, "reading upgrade response: {} bytes", data.length());
  if (!upgrade_response_->onData(data)) {
    // Wait for more data.
    return;
  }

  ENVOY_LOG(debug, "upgrade response complete");
  protocol_->completeUpgrade(*conn_state_, *upgrade_response_);
  upgrade_response_.reset();
  onRequestStart(true);
}

ThriftFilters::ResponseStatus
UpstreamRequest::handleRegularResponse(Buffer::Instance& data,
                                       UpstreamResponseCallbacks& callbacks) {
  ENVOY_LOG(trace, "reading response: {} bytes", data.length());

  if (!response_started_) {
    callbacks.startUpstreamResponse(*transport_, *protocol_);
    response_started_ = true;
  }

  const auto& cluster = parent_.cluster();

  const auto status = callbacks.upstreamData(data);
  if (status == ThriftFilters::ResponseStatus::Complete) {

    stats_.recordUpstreamResponseSize(cluster, response_size_);

    switch (callbacks.responseMetadata()->messageType()) {
    case MessageType::Reply:
      if (callbacks.responseSuccess()) {
        upstream_host_->outlierDetector().putResult(
            Upstream::Outlier::Result::ExtOriginRequestSuccess);
        stats_.incResponseReplySuccess(cluster, upstream_host_);
      } else {
        upstream_host_->outlierDetector().putResult(
            Upstream::Outlier::Result::ExtOriginRequestFailed);
        stats_.incResponseReplyError(cluster, upstream_host_);
      }
      break;

    case MessageType::Exception:
      upstream_host_->outlierDetector().putResult(
          Upstream::Outlier::Result::ExtOriginRequestFailed);
      stats_.incResponseRemoteException(cluster, upstream_host_);
      break;

    default:
      stats_.incResponseInvalidType(cluster, upstream_host_);
      break;
    }

    if (callbacks.responseMetadata()->isDraining()) {
      ENVOY_LOG(debug, "got draining signal");
      stats_.incCloseDrain(cluster);
      // ResetStream triggers a local connection failure. However, we want to
      // keep the downstream connection after the upstream connection, i.e.,
      // `conn_data->connection()`, is closed. Therefore, introduce a new flag
      // `draining_` to hint that we got a draining signal and not to close the
      //  downstream connection.
      draining_ = true;
      resetStream();
    }
    onResponseComplete();
  } else if (status == ThriftFilters::ResponseStatus::Reset) {
    // Note: invalid responses are not accounted in the response size histogram.
    ENVOY_LOG(debug, "upstream reset");
    upstream_host_->outlierDetector().putResult(Upstream::Outlier::Result::ExtOriginRequestFailed);
    stats_.incResponseDecodingError(cluster, upstream_host_);
    resetStream();
  }

  return status;
}

bool UpstreamRequest::handleUpstreamData(Buffer::Instance& data, bool end_stream,
                                         UpstreamResponseCallbacks& callbacks) {
  ASSERT(!response_complete_);

  response_size_ += data.length();

  if (upgrade_response_ != nullptr) {
    handleUpgradeResponse(data);
  } else {
    const auto status = handleRegularResponse(data, callbacks);
    if (status != ThriftFilters::ResponseStatus::MoreData) {
      return true;
    }
  }

  if (end_stream) {
    // Response is incomplete, but no more data is coming.
    ENVOY_LOG(debug, "response underflow");
    onResponseComplete();
    response_underflow_ = true;
    onResetStream(ConnectionPool::PoolFailureReason::RemoteConnectionFailure);
    return true;
  }

  return false;
}

void UpstreamRequest::onEvent(Network::ConnectionEvent event) {
  ASSERT(!response_complete_);
  bool end_downstream = true;

  switch (event) {
  case Network::ConnectionEvent::RemoteClose:
    ENVOY_LOG(debug, "upstream remote close");
    end_downstream = onResetStream(ConnectionPool::PoolFailureReason::RemoteConnectionFailure);
    break;
  case Network::ConnectionEvent::LocalClose:
    ENVOY_LOG(debug, "upstream local close");
    end_downstream = onResetStream(ConnectionPool::PoolFailureReason::LocalConnectionFailure);
    break;
  case Network::ConnectionEvent::Connected:
  case Network::ConnectionEvent::ConnectedZeroRtt:
    // Connected is consumed by the connection pool.
    IS_ENVOY_BUG("reached unexpectedly");
  }

  releaseConnection(false);
  if (!end_downstream && request_complete_) {
    parent_.onReset();
  }
}

uint64_t UpstreamRequest::encodeAndWrite(Buffer::OwnedImpl& request_buffer) {
  Buffer::OwnedImpl transport_buffer;

  metadata_->setProtocol(protocol_->type());
  transport_->encodeFrame(transport_buffer, *metadata_, request_buffer);

  uint64_t size = transport_buffer.length();

  conn_data_->connection().write(transport_buffer, false);

  return size;
}

void UpstreamRequest::onRequestStart(bool continue_decoding) {
  auto& buffer = parent_.buffer();
  parent_.initProtocolConverter(*protocol_, buffer);

  metadata_->setSequenceId(conn_state_->nextSequenceId());
  parent_.convertMessageBegin(metadata_);

  if (continue_decoding) {
    parent_.continueDecoding();
  }
}

void UpstreamRequest::onRequestComplete() {
  Event::Dispatcher& dispatcher = parent_.dispatcher();
  downstream_request_complete_time_ = dispatcher.timeSource().monotonicTime();
  request_complete_ = true;
}

void UpstreamRequest::onResponseComplete() {
  ENVOY_LOG(debug, "response complete");
  chargeResponseTiming();
  response_complete_ = true;
  conn_state_ = nullptr;
  conn_data_.reset();
}

void UpstreamRequest::onUpstreamHostSelected(Upstream::HostDescriptionConstSharedPtr host) {
  upstream_host_ = host;
}

static Upstream::Outlier::Result
poolFailureReasonToResult(ConnectionPool::PoolFailureReason reason) {
  switch (reason) {
  case ConnectionPool::PoolFailureReason::Overflow:
    FALLTHRU;
  case ConnectionPool::PoolFailureReason::LocalConnectionFailure:
    FALLTHRU;
  case ConnectionPool::PoolFailureReason::RemoteConnectionFailure:
    return Upstream::Outlier::Result::LocalOriginConnectFailed;
  case ConnectionPool::PoolFailureReason::Timeout:
    return Upstream::Outlier::Result::LocalOriginTimeout;
  }
  PANIC_DUE_TO_CORRUPT_ENUM;
}

bool UpstreamRequest::onResetStream(ConnectionPool::PoolFailureReason reason) {
  bool close_downstream = true;

  chargeResponseTiming();

  switch (reason) {
  case ConnectionPool::PoolFailureReason::Overflow:
    stats_.incResponseLocalException(parent_.cluster());
    parent_.sendLocalReply(AppException(AppExceptionType::InternalError,
                                        "thrift upstream request: too many connections"),
                           false /* Don't close the downstream connection. */);
    close_downstream = false;
    break;
  case ConnectionPool::PoolFailureReason::LocalConnectionFailure:
    FALLTHRU;
  case ConnectionPool::PoolFailureReason::RemoteConnectionFailure:
    FALLTHRU;
  case ConnectionPool::PoolFailureReason::Timeout:
    upstream_host_->outlierDetector().putResult(poolFailureReasonToResult(reason));

    // Error occurred after a partial or underflow response, propagate the reset to the
    // downstream.
    if (response_underflow_ || (response_started_ && !draining_ && !response_complete_)) {
      ENVOY_LOG(debug, "reset downstream connection for a partial or underflow response");
      parent_.resetDownstreamConnection();
    } else if (!draining_ && !response_complete_) {
      close_downstream = close_downstream_on_error_;
      stats_.incResponseLocalException(parent_.cluster());
      parent_.sendLocalReply(
          AppException(AppExceptionType::InternalError,
                       fmt::format("connection failure: {} '{}'",
                                   PoolFailureReasonNames::get().fromReason(reason),
                                   (upstream_host_ && upstream_host_->address())
                                       ? upstream_host_->address()->asString()
                                       : "to upstream")),
          close_downstream);
    }
    break;
  }

  ENVOY_LOG(debug,
            "upstream reset complete. reason={}, close_downstream={}, response_started={}, "
            "response_complete={}, draining={}, response_underflow={}",
            PoolFailureReasonNames::get().fromReason(reason), close_downstream,
            static_cast<bool>(response_started_), static_cast<bool>(response_complete_),
            static_cast<bool>(draining_), static_cast<bool>(response_underflow_));
  return close_downstream;
}

void UpstreamRequest::chargeResponseTiming() {
  if (charged_response_timing_ || !request_complete_) {
    return;
  }
  charged_response_timing_ = true;
  Event::Dispatcher& dispatcher = parent_.dispatcher();
  const std::chrono::milliseconds response_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          dispatcher.timeSource().monotonicTime() - downstream_request_complete_time_);
  stats_.recordUpstreamResponseTime(parent_.cluster(), upstream_host_, response_time.count());
}

} // namespace Router
} // namespace ThriftProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
