#import <Foundation/Foundation.h>

@class EnvoyHTTPFilterFactory;
@class EnvoyNativeFilterConfig;
@class EnvoyStringAccessor;
@protocol EnvoyKeyValueStore;

NS_ASSUME_NONNULL_BEGIN

/// Typed configuration that may be used for starting Envoy.
@interface EnvoyConfiguration : NSObject

@property (nonatomic, assign) BOOL adminInterfaceEnabled;
@property (nonatomic, strong, nullable) NSString *grpcStatsDomain;
@property (nonatomic, assign) UInt32 connectTimeoutSeconds;
@property (nonatomic, assign) UInt32 dnsFailureRefreshSecondsBase;
@property (nonatomic, assign) UInt32 dnsFailureRefreshSecondsMax;
@property (nonatomic, assign) UInt32 dnsQueryTimeoutSeconds;
@property (nonatomic, assign) UInt32 dnsMinRefreshSeconds;
@property (nonatomic, strong) NSArray<NSString *> *dnsPreresolveHostnames;
@property (nonatomic, assign) UInt32 dnsRefreshSeconds;
@property (nonatomic, assign) BOOL enableDNSCache;
@property (nonatomic, assign) UInt32 dnsCacheSaveIntervalSeconds;
@property (nonatomic, assign) BOOL enableHappyEyeballs;
@property (nonatomic, assign) BOOL enableHttp3;
@property (nonatomic, assign) BOOL enableGzipDecompression;
@property (nonatomic, assign) BOOL enableGzipCompression;
@property (nonatomic, assign) BOOL enableBrotliDecompression;
@property (nonatomic, assign) BOOL enableBrotliCompression;
@property (nonatomic, assign) BOOL enableInterfaceBinding;
@property (nonatomic, assign) BOOL enableDrainPostDnsRefresh;
@property (nonatomic, assign) BOOL enforceTrustChainVerification;
@property (nonatomic, assign) BOOL forceIPv6;
@property (nonatomic, assign) BOOL enablePlatformCertificateValidation;
@property (nonatomic, assign) BOOL useLegacyBuilder;
@property (nonatomic, assign) UInt32 h2ConnectionKeepaliveIdleIntervalMilliseconds;
@property (nonatomic, assign) UInt32 h2ConnectionKeepaliveTimeoutSeconds;
@property (nonatomic, assign) UInt32 maxConnectionsPerHost;
@property (nonatomic, assign) UInt32 statsFlushSeconds;
@property (nonatomic, assign) UInt32 streamIdleTimeoutSeconds;
@property (nonatomic, assign) UInt32 perTryIdleTimeoutSeconds;
@property (nonatomic, strong) NSString *appVersion;
@property (nonatomic, strong) NSString *appId;
@property (nonatomic, strong) NSArray<NSString *> *virtualClusters;
@property (nonatomic, strong) NSString *directResponseMatchers;
@property (nonatomic, strong) NSString *directResponses;
@property (nonatomic, strong) NSArray<EnvoyNativeFilterConfig *> *nativeFilterChain;
@property (nonatomic, strong) NSArray<EnvoyHTTPFilterFactory *> *httpPlatformFilterFactories;
@property (nonatomic, strong) NSDictionary<NSString *, EnvoyStringAccessor *> *stringAccessors;
@property (nonatomic, strong) NSDictionary<NSString *, id<EnvoyKeyValueStore>> *keyValueStores;
@property (nonatomic, strong) NSArray<NSString *> *statsSinks;
@property (nonatomic, copy, nullable) void (^experimentalValidateYAMLCallback)(BOOL);

/**
 Create a new instance of the configuration.
 */
- (instancetype)initWithAdminInterfaceEnabled:(BOOL)adminInterfaceEnabled
                                  grpcStatsDomain:(nullable NSString *)grpcStatsDomain
                            connectTimeoutSeconds:(UInt32)connectTimeoutSeconds
                                dnsRefreshSeconds:(UInt32)dnsRefreshSeconds
                     dnsFailureRefreshSecondsBase:(UInt32)dnsFailureRefreshSecondsBase
                      dnsFailureRefreshSecondsMax:(UInt32)dnsFailureRefreshSecondsMax
                           dnsQueryTimeoutSeconds:(UInt32)dnsQueryTimeoutSeconds
                             dnsMinRefreshSeconds:(UInt32)dnsMinRefreshSeconds
                           dnsPreresolveHostnames:(NSArray<NSString *> *)dnsPreresolveHostnames
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
                                  virtualClusters:(NSArray<NSString *> *)virtualClusters
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
                                       statsSinks:(NSArray<NSString *> *)statsSinks
                 experimentalValidateYAMLCallback:
                     (nullable void (^)(BOOL))experimentalValidateYAMLCallback
                                 useLegacyBuilder:(BOOL)useLegacyBuilder;

/**
 Resolves the provided configuration template using properties on this configuration.

 @param templateYAML The template configuration to resolve.
 @return The resolved template. Nil if the template fails to fully resolve.
 */
- (nullable NSString *)resolveTemplate:(NSString *)templateYAML;

/**
 * Takes the various fields from EnvoyConfiguration, and the YAML it generates
 * and compares the YAML generated by the C++ builder to the YAML generated by this
 * `EnvoyConfiguration`.
 *
 * @param yaml The yaml generated by this `EnvoyConfiguration`
 *
 * @return True if the YAML matched, false if a difference was found.
 */
- (BOOL)compareYAMLWithProtoBuilder:(NSString *)yaml;

@end

NS_ASSUME_NONNULL_END
