#import "library/objective-c/EnvoyEngine.h"

#import "library/common/main_interface.h"

@implementation EnvoyConfiguration

- (instancetype)initWithAdminInterfaceEnabled:(BOOL)adminInterfaceEnabled
                                  GrpcStatsDomain:(nullable NSString *)grpcStatsDomain
                            connectTimeoutSeconds:(UInt32)connectTimeoutSeconds
                                dnsRefreshSeconds:(UInt32)dnsRefreshSeconds
                     dnsFailureRefreshSecondsBase:(UInt32)dnsFailureRefreshSecondsBase
                      dnsFailureRefreshSecondsMax:(UInt32)dnsFailureRefreshSecondsMax
                           dnsQueryTimeoutSeconds:(UInt32)dnsQueryTimeoutSeconds
                             dnsMinRefreshSeconds:(UInt32)dnsMinRefreshSeconds
                           dnsPreresolveHostnames:(NSString *)dnsPreresolveHostnames
                                   enableDNSCache:(BOOL)enableDNSCache
                      dnsCacheSaveIntervalSeconds:(UInt32)dnsCacheSaveIntervalSeconds
                              enableHappyEyeballs:(BOOL)enableHappyEyeballs
                                      enableHttp3:(BOOL)enableHttp3
                          enableGzipDecompression:(BOOL)enableGzipDecompression
                            enableGzipCompression:(BOOL)enableGzipCompression
                        enableBrotliDecompression:(BOOL)enableBrotliDecompression
                          enableBrotliCompression:(BOOL)enableBrotliCompression
                           enableInterfaceBinding:(BOOL)enableInterfaceBinding
                        enableDrainPostDnsRefresh:(BOOL)enableDrainPostDnsRefresh
                    enforceTrustChainVerification:(BOOL)enforceTrustChainVerification
                                        forceIPv6:(BOOL)forceIPv6
              enablePlatformCertificateValidation:(BOOL)enablePlatformCertificateValidation
    h2ConnectionKeepaliveIdleIntervalMilliseconds:
        (UInt32)h2ConnectionKeepaliveIdleIntervalMilliseconds
              h2ConnectionKeepaliveTimeoutSeconds:(UInt32)h2ConnectionKeepaliveTimeoutSeconds
                            maxConnectionsPerHost:(UInt32)maxConnectionsPerHost
                                statsFlushSeconds:(UInt32)statsFlushSeconds
                         streamIdleTimeoutSeconds:(UInt32)streamIdleTimeoutSeconds
                         perTryIdleTimeoutSeconds:(UInt32)perTryIdleTimeoutSeconds
                                       appVersion:(NSString *)appVersion
                                            appId:(NSString *)appId
                                  virtualClusters:(NSString *)virtualClusters
                           directResponseMatchers:(NSString *)directResponseMatchers
                                  directResponses:(NSString *)directResponses
                                nativeFilterChain:
                                    (NSArray<EnvoyNativeFilterConfig *> *)nativeFilterChain
                              platformFilterChain:
                                  (NSArray<EnvoyHTTPFilterFactory *> *)httpPlatformFilterFactories
                                  stringAccessors:
                                      (NSDictionary<NSString *, EnvoyStringAccessor *> *)
                                          stringAccessors
                                   keyValueStores:
                                       (NSDictionary<NSString *, id<EnvoyKeyValueStore>> *)
                                           keyValueStores
                                       statsSinks:(NSArray<NSString *> *)statsSinks {
  self = [super init];
  if (!self) {
    return nil;
  }

  self.adminInterfaceEnabled = adminInterfaceEnabled;
  self.grpcStatsDomain = grpcStatsDomain;
  self.connectTimeoutSeconds = connectTimeoutSeconds;
  self.dnsRefreshSeconds = dnsRefreshSeconds;
  self.dnsFailureRefreshSecondsBase = dnsFailureRefreshSecondsBase;
  self.dnsFailureRefreshSecondsMax = dnsFailureRefreshSecondsMax;
  self.dnsQueryTimeoutSeconds = dnsQueryTimeoutSeconds;
  self.dnsMinRefreshSeconds = dnsMinRefreshSeconds;
  self.dnsPreresolveHostnames = dnsPreresolveHostnames;
  self.enableDNSCache = enableDNSCache;
  self.dnsCacheSaveIntervalSeconds = dnsCacheSaveIntervalSeconds;
  self.enableHappyEyeballs = enableHappyEyeballs;
  self.enableHttp3 = enableHttp3;
  self.enableGzipDecompression = enableGzipDecompression;
  self.enableGzipCompression = enableGzipCompression;
  self.enableBrotliDecompression = enableBrotliDecompression;
  self.enableBrotliCompression = enableBrotliCompression;
  self.enableInterfaceBinding = enableInterfaceBinding;
  self.enableDrainPostDnsRefresh = enableDrainPostDnsRefresh;
  self.enforceTrustChainVerification = enforceTrustChainVerification;
  self.forceIPv6 = forceIPv6;
  self.enablePlatformCertificateValidation = enablePlatformCertificateValidation;
  self.h2ConnectionKeepaliveIdleIntervalMilliseconds =
      h2ConnectionKeepaliveIdleIntervalMilliseconds;
  self.h2ConnectionKeepaliveTimeoutSeconds = h2ConnectionKeepaliveTimeoutSeconds;
  self.maxConnectionsPerHost = maxConnectionsPerHost;
  self.statsFlushSeconds = statsFlushSeconds;
  self.streamIdleTimeoutSeconds = streamIdleTimeoutSeconds;
  self.perTryIdleTimeoutSeconds = perTryIdleTimeoutSeconds;
  self.appVersion = appVersion;
  self.appId = appId;
  self.virtualClusters = virtualClusters;
  self.directResponseMatchers = directResponseMatchers;
  self.directResponses = directResponses;
  self.nativeFilterChain = nativeFilterChain;
  self.httpPlatformFilterFactories = httpPlatformFilterFactories;
  self.stringAccessors = stringAccessors;
  self.keyValueStores = keyValueStores;
  self.statsSinks = statsSinks;
  return self;
}

- (nullable NSString *)resolveTemplate:(NSString *)templateYAML {
  NSMutableString *customClusters = [[NSMutableString alloc] init];
  NSMutableString *customListeners = [[NSMutableString alloc] init];
  NSMutableString *customRoutes = [[NSMutableString alloc] init];
  NSMutableString *customFilters = [[NSMutableString alloc] init];

  NSString *platformFilterTemplate = [[NSString alloc] initWithUTF8String:platform_filter_template];
  for (EnvoyHTTPFilterFactory *filterFactory in self.httpPlatformFilterFactories) {
    NSString *filterConfig =
        [platformFilterTemplate stringByReplacingOccurrencesOfString:@"{{ platform_filter_name }}"
                                                          withString:filterFactory.filterName];
    [customFilters appendString:filterConfig];
  }

  NSString *nativeFilterTemplate = [[NSString alloc] initWithUTF8String:native_filter_template];
  for (EnvoyNativeFilterConfig *nativeFilterConfig in self.nativeFilterChain) {
    NSString *filterConfig =
        [[nativeFilterTemplate stringByReplacingOccurrencesOfString:@"{{ native_filter_name }}"
                                                         withString:nativeFilterConfig.name]
            stringByReplacingOccurrencesOfString:@"{{ native_filter_typed_config }}"
                                      withString:nativeFilterConfig.typedConfig];
    [customFilters appendString:filterConfig];
  }

  if (self.enableGzipDecompression) {
    NSString *insert = [[NSString alloc] initWithUTF8String:gzip_decompressor_config_insert];
    [customFilters appendString:insert];
  }

  if (self.enableGzipCompression) {
#if ENVOY_MOBILE_REQUEST_COMPRESSION
    NSString *insert = [[NSString alloc] initWithUTF8String:gzip_compressor_config_insert];
    [customFilters appendString:insert];
#else
    NSLog(@"[Envoy] error: request compression functionality was not compiled in this build of "
          @"Envoy Mobile");
    return nil;
#endif
  }

  if (self.enableBrotliDecompression) {
    NSString *insert = [[NSString alloc] initWithUTF8String:brotli_decompressor_config_insert];
    [customFilters appendString:insert];
  }

  if (self.enableBrotliCompression) {
#if ENVOY_MOBILE_REQUEST_COMPRESSION
    NSString *insert = [[NSString alloc] initWithUTF8String:brotli_compressor_config_insert];
    [customFilters appendString:insert];
#else
    NSLog(@"[Envoy] error: request compression functionality was not compiled in this build of "
          @"Envoy Mobile");
    return nil;
#endif
  }

  if (self.enableHttp3) {
    NSString *http3Insert =
        [[NSString alloc] initWithUTF8String:alternate_protocols_cache_filter_insert];
    [customFilters appendString:http3Insert];
  }

  BOOL hasDirectResponses = self.directResponses.length > 0;
  if (hasDirectResponses) {
    templateYAML = [templateYAML stringByReplacingOccurrencesOfString:@"#{fake_remote_responses}"
                                                           withString:self.directResponses];
    [customClusters appendString:[[NSString alloc] initWithUTF8String:fake_remote_cluster_insert]];
    [customListeners
        appendString:[[NSString alloc] initWithUTF8String:fake_remote_listener_insert]];
    [customRoutes appendString:self.directResponseMatchers];
    [customFilters
        appendString:[[NSString alloc] initWithUTF8String:route_cache_reset_filter_insert]];
  }

  templateYAML = [templateYAML stringByReplacingOccurrencesOfString:@"#{custom_clusters}"
                                                         withString:customClusters];
  templateYAML = [templateYAML stringByReplacingOccurrencesOfString:@"#{custom_listeners}"
                                                         withString:customListeners];
  templateYAML = [templateYAML stringByReplacingOccurrencesOfString:@"#{custom_routes}"
                                                         withString:customRoutes];
  templateYAML = [templateYAML stringByReplacingOccurrencesOfString:@"#{custom_filters}"
                                                         withString:customFilters];

  NSMutableString *definitions =
      [[NSMutableString alloc] initWithString:@"!ignore platform_defs:\n"];

  [definitions
      appendFormat:@"- &connect_timeout %lus\n", (unsigned long)self.connectTimeoutSeconds];
  [definitions appendFormat:@"- &dns_fail_base_interval %lus\n",
                            (unsigned long)self.dnsFailureRefreshSecondsBase];
  [definitions appendFormat:@"- &dns_fail_max_interval %lus\n",
                            (unsigned long)self.dnsFailureRefreshSecondsMax];
  [definitions
      appendFormat:@"- &dns_query_timeout %lus\n", (unsigned long)self.dnsQueryTimeoutSeconds];
  [definitions
      appendFormat:@"- &dns_min_refresh_rate %lus\n", (unsigned long)self.dnsMinRefreshSeconds];
  [definitions appendFormat:@"- &dns_preresolve_hostnames %@\n", self.dnsPreresolveHostnames];
  [definitions appendFormat:@"- &dns_lookup_family %@\n",
                            self.enableHappyEyeballs ? @"ALL" : @"V4_PREFERRED"];
  [definitions appendFormat:@"- &dns_refresh_rate %lus\n", (unsigned long)self.dnsRefreshSeconds];
  [definitions appendFormat:@"- &enable_drain_post_dns_refresh %@\n",
                            self.enableDrainPostDnsRefresh ? @"true" : @"false"];
  [definitions appendFormat:@"- &enable_interface_binding %@\n",
                            self.enableInterfaceBinding ? @"true" : @"false"];
  [definitions appendFormat:@"- &trust_chain_verification %@\n", self.enforceTrustChainVerification
                                                                     ? @"VERIFY_TRUST_CHAIN"
                                                                     : @"ACCEPT_UNTRUSTED"];
  [definitions appendFormat:@"- &force_ipv6 %@\n", self.forceIPv6 ? @"true" : @"false"];
  [definitions appendFormat:@"- &h2_connection_keepalive_idle_interval %.*fs\n", 3,
                            (double)self.h2ConnectionKeepaliveIdleIntervalMilliseconds / 1000.0];
  [definitions appendFormat:@"- &h2_connection_keepalive_timeout %lus\n",
                            (unsigned long)self.h2ConnectionKeepaliveTimeoutSeconds];
  [definitions
      appendFormat:@"- &max_connections_per_host %lu\n", (unsigned long)self.maxConnectionsPerHost];
  [definitions
      appendFormat:@"- &stream_idle_timeout %lus\n", (unsigned long)self.streamIdleTimeoutSeconds];
  [definitions
      appendFormat:@"- &per_try_idle_timeout %lus\n", (unsigned long)self.perTryIdleTimeoutSeconds];
  [definitions appendFormat:@"- &metadata { device_os: %@, app_version: %@, app_id: %@ }\n", @"iOS",
                            self.appVersion, self.appId];
  [definitions appendFormat:@"- &virtual_clusters %@\n", self.virtualClusters];

  [definitions
      appendFormat:@"- &stats_flush_interval %lus\n", (unsigned long)self.statsFlushSeconds];

  NSString *cert_validator_template = self.enablePlatformCertificateValidation
                                          ? @(platform_cert_validation_context_template)
                                          : @(default_cert_validation_context_template);

  [definitions appendFormat:@"%@\n", cert_validator_template];

  if (self.enableDNSCache) {
    [definitions appendFormat:@"- &persistent_dns_cache_save_interval %lu\n",
                              (unsigned long)self.dnsCacheSaveIntervalSeconds];
    NSString *persistent_dns_cache_config = @(persistent_dns_cache_config_insert);
    [definitions appendFormat:@"- &persistent_dns_cache_config %@\n", persistent_dns_cache_config];
  }

  NSMutableArray *stat_sinks_config = [self.statsSinks mutableCopy];

  if (self.grpcStatsDomain != nil) {
    [definitions appendFormat:@"- &stats_domain %@\n", self.grpcStatsDomain];
    [stat_sinks_config addObject:@"*base_metrics_service"];
  }

  if (stat_sinks_config.count > 0) {
    [definitions appendString:@"- &stats_sinks ["];
    [definitions appendString:[stat_sinks_config componentsJoinedByString:@","]];
    [definitions appendString:@"]\n"];
  }

  if (self.adminInterfaceEnabled) {
    [definitions appendString:@"admin: *admin_interface\n"];
  }

  [definitions appendString:templateYAML];

  if ([definitions containsString:@"{{"]) {
    NSLog(@"[Envoy] error: could not resolve all template keys in config:\n%@", definitions);
    return nil;
  }

  NSLog(@"[Envoy] debug: config:\n%@", definitions);
  return definitions;
}

@end
