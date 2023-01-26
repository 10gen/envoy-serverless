#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "envoy/config/bootstrap/v3/bootstrap.pb.h"

#include "absl/container/flat_hash_map.h"
#include "engine.h"
#include "engine_callbacks.h"
#include "key_value_store.h"
#include "log_level.h"
#include "string_accessor.h"

namespace Envoy {
namespace Platform {

// The C++ Engine builder supports 2 ways of building Envoy Mobile config, the 'legacy mode'
// which uses a yaml config header, blocks of well known yaml configs, and uses string manipulation
// to glue them together, and the 'bootstrap mode' which creates a structured bootstrap proto and
// modifies it to produce the same config. We need to retain the legacy mode even if all functions
// become bootstrap compatible to be able to regression test that changes to the config yaml are
// reflected in generateBootstrap, until all languages use the C++ bootstrap builder.
//
// Currently the default is legacy mode but worth noting bootstrap mode is 2 orders of magnitude
// faster.
class EngineBuilder {
public:
  // This constructor is not compatible with bootstrap mode.
  EngineBuilder(std::string config_template);
  EngineBuilder();

  // Use the experimental non-YAML config mode which uses the bootstrap proto directly.
  EngineBuilder& setUseBootstrap();

  EngineBuilder& addLogLevel(LogLevel log_level);
  EngineBuilder& setOnEngineRunning(std::function<void()> closure);

  EngineBuilder& addGrpcStatsDomain(std::string stats_domain);
  EngineBuilder& addConnectTimeoutSeconds(int connect_timeout_seconds);
  EngineBuilder& addDnsRefreshSeconds(int dns_refresh_seconds);
  EngineBuilder& addDnsFailureRefreshSeconds(int base, int max);
  EngineBuilder& addDnsQueryTimeoutSeconds(int dns_query_timeout_seconds);
  EngineBuilder& addDnsMinRefreshSeconds(int dns_min_refresh_seconds);
  EngineBuilder& addMaxConnectionsPerHost(int max_connections_per_host);
  EngineBuilder& useDnsSystemResolver(bool use_system_resolver);
  EngineBuilder& addH2ConnectionKeepaliveIdleIntervalMilliseconds(
      int h2_connection_keepalive_idle_interval_milliseconds);
  EngineBuilder&
  addH2ConnectionKeepaliveTimeoutSeconds(int h2_connection_keepalive_timeout_seconds);
  EngineBuilder& addStatsFlushSeconds(int stats_flush_seconds);
  // Configures Envoy to use the PlatformBridge filter named `name`. An instance of
  // envoy_http_filter must be registered as a platform API with the same name.
  EngineBuilder& setAppVersion(std::string app_version);
  EngineBuilder& setAppId(std::string app_id);
  EngineBuilder& setDeviceOs(std::string app_id);
  EngineBuilder& setStreamIdleTimeoutSeconds(int stream_idle_timeout_seconds);
  EngineBuilder& setPerTryIdleTimeoutSeconds(int per_try_idle_timeout_seconds);
  EngineBuilder& enableGzip(bool gzip_on);
  EngineBuilder& enableBrotli(bool brotli_on);
  EngineBuilder& enableSocketTagging(bool socket_tagging_on);
  EngineBuilder& enableHappyEyeballs(bool happy_eyeballs_on);
  EngineBuilder& enableHttp3(bool http3_on);
  EngineBuilder& enableInterfaceBinding(bool interface_binding_on);
  EngineBuilder& enableDrainPostDnsRefresh(bool drain_post_dns_refresh_on);
  EngineBuilder& enforceTrustChainVerification(bool trust_chain_verification_on);
  EngineBuilder& enablePlatformCertificatesValidation(bool platform_certificates_validation_on);

  // These functions are not compatible with boostrap mode, see class definition for details.
  EngineBuilder& addStatsSinks(const std::vector<std::string>& stat_sinks);
  EngineBuilder& addDnsPreresolveHostnames(std::string dns_preresolve_hostnames);
  EngineBuilder& addVirtualClusters(std::string virtual_clusters);
  EngineBuilder& addKeyValueStore(std::string name, KeyValueStoreSharedPtr key_value_store);
  EngineBuilder& addStringAccessor(std::string name, StringAccessorSharedPtr accessor);
  EngineBuilder& addNativeFilter(std::string name, std::string typed_config);
  EngineBuilder& addPlatformFilter(std::string name);
  EngineBuilder& enableAdminInterface(bool admin_interface_on);

  // this is separated from build() for the sake of testability
  std::string generateConfigStr() const;
  // If trim_whitespace_for_tests is set true, it will prunt whitespace from the
  // certificates file to allow for proto equivalence testing. This is expensive
  // and should only be done for testing purposes (in explicit unit tests or
  // debug mode)
  std::unique_ptr<envoy::config::bootstrap::v3::Bootstrap>
  generateBootstrap(bool trim_whitespace_for_tests = false) const;

  EngineSharedPtr build();

protected:
  void setOverrideConfigForTests(std::string config) { config_override_for_tests_ = config; }
  void setAdminAddressPathForTests(std::string admin) { admin_address_path_for_tests_ = admin; }

private:
  friend BaseClientIntegrationTest;
  struct NativeFilterConfig {
    NativeFilterConfig(std::string name, std::string typed_config)
        : name_(std::move(name)), typed_config_(std::move(typed_config)) {}

    std::string name_;
    std::string typed_config_;
  };

  // Verifies use_bootstrap_ is not true and sets config_bootstrap_incompatible_ true.
  void bootstrapIncompatible();

  LogLevel log_level_ = LogLevel::info;
  EngineCallbacksSharedPtr callbacks_;

  std::string config_template_;
  std::string stats_domain_;
  int connect_timeout_seconds_ = 30;
  int dns_refresh_seconds_ = 60;
  int dns_failure_refresh_seconds_base_ = 2;
  int dns_failure_refresh_seconds_max_ = 10;
  int dns_query_timeout_seconds_ = 25;
  std::string dns_preresolve_hostnames_ = "[]";
  bool use_system_resolver_ = true;
  int h2_connection_keepalive_idle_interval_milliseconds_ = 100000000;
  int h2_connection_keepalive_timeout_seconds_ = 10;
  int stats_flush_seconds_ = 60;
  std::string app_version_ = "unspecified";
  std::string app_id_ = "unspecified";
  std::string device_os_ = "unspecified";
  std::string virtual_clusters_ = "[]";
  std::string config_override_for_tests_ = "";
  std::string admin_address_path_for_tests_ = "";
  int stream_idle_timeout_seconds_ = 15;
  int per_try_idle_timeout_seconds_ = 15;
  bool gzip_filter_ = true;
  bool brotli_filter_ = false;
  bool socket_tagging_filter_ = false;
  bool platform_certificates_validation_on_ = false;

  absl::flat_hash_map<std::string, KeyValueStoreSharedPtr> key_value_stores_{};

  bool admin_interface_enabled_ = false;
  bool enable_happy_eyeballs_ = true;
  bool enable_interface_binding_ = false;
  bool enable_drain_post_dns_refresh_ = false;
  bool enforce_trust_chain_verification_ = true;
  bool h2_extend_keepalive_timeout_ = false;
  bool enable_http3_ = true;
  int dns_min_refresh_seconds_ = 60;
  int max_connections_per_host_ = 7;
  std::vector<std::string> stat_sinks_;

  std::vector<NativeFilterConfig> native_filter_chain_;
  std::vector<std::string> platform_filters_;
  absl::flat_hash_map<std::string, StringAccessorSharedPtr> string_accessors_;
  bool config_bootstrap_incompatible_ = false;
  bool use_bootstrap_ = false;
};

using EngineBuilderSharedPtr = std::shared_ptr<EngineBuilder>;

} // namespace Platform
} // namespace Envoy
