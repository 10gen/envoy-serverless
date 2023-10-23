#include <chrono>
#include <memory>

#include "envoy/config/core/v3/grpc_service.pb.h"
#include "envoy/grpc/async_client.h"

#include "source/common/api/api_impl.h"
#include "source/common/event/dispatcher_impl.h"
#include "source/common/grpc/async_client_manager_impl.h"

#include "test/mocks/stats/mocks.h"
#include "test/mocks/thread_local/mocks.h"
#include "test/mocks/upstream/cluster_manager.h"
#include "test/mocks/upstream/cluster_priority_set.h"
#include "test/test_common/test_runtime.h"
#include "test/test_common/test_time.h"
#include "test/test_common/utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::Return;

namespace Envoy {
namespace Grpc {
namespace {

class RawAsyncClientCacheTest : public testing::Test {
public:
  RawAsyncClientCacheTest()
      : api_(Api::createApiForTest(time_system_)),
        dispatcher_(api_->allocateDispatcher("test_thread")), client_cache_(*dispatcher_) {}

  // advanceTimeAndRun moves the current time as requested, and then executes
  // all executable timers in a non-deterministic order. This mimics real-time behavior in
  // libevent if there is a long delay between libevent regaining control. Here we want to
  // test behavior with a specific sequence of events, where each timer fires within a
  // simulated second of what was programmed.
  void waitForSeconds(int seconds) {
    for (int i = 0; i < seconds; i++) {
      time_system_.advanceTimeAndRun(std::chrono::seconds(1), *dispatcher_,
                                     Event::Dispatcher::RunType::NonBlock);
    }
  }

protected:
  Event::SimulatedTimeSystem time_system_;
  Api::ApiPtr api_;
  Event::DispatcherPtr dispatcher_;
  AsyncClientManagerImpl::RawAsyncClientCache client_cache_;
};

TEST_F(RawAsyncClientCacheTest, CacheEviction) {
  envoy::config::core::v3::GrpcService foo_service;
  foo_service.mutable_envoy_grpc()->set_cluster_name("foo");
  GrpcServiceConfigWithHashKey config_with_hash_key(foo_service);
  RawAsyncClientSharedPtr foo_client = std::make_shared<MockAsyncClient>();
  client_cache_.setCache(config_with_hash_key, foo_client);
  waitForSeconds(49);
  // Cache entry hasn't been evicted because it was created 49s ago.
  EXPECT_EQ(client_cache_.getCache(config_with_hash_key).get(), foo_client.get());
  waitForSeconds(49);
  // Cache entry hasn't been evicted because it was accessed 49s ago.
  EXPECT_EQ(client_cache_.getCache(config_with_hash_key).get(), foo_client.get());
  waitForSeconds(51);
  EXPECT_EQ(client_cache_.getCache(config_with_hash_key).get(), nullptr);
}

TEST_F(RawAsyncClientCacheTest, MultipleCacheEntriesEviction) {
  envoy::config::core::v3::GrpcService grpc_service;
  RawAsyncClientSharedPtr foo_client = std::make_shared<MockAsyncClient>();
  for (int i = 1; i <= 50; i++) {
    grpc_service.mutable_envoy_grpc()->set_cluster_name(std::to_string(i));
    GrpcServiceConfigWithHashKey config_with_hash_key(grpc_service);
    client_cache_.setCache(config_with_hash_key, foo_client);
  }
  waitForSeconds(20);
  for (int i = 51; i <= 100; i++) {
    grpc_service.mutable_envoy_grpc()->set_cluster_name(std::to_string(i));
    GrpcServiceConfigWithHashKey config_with_hash_key(grpc_service);
    client_cache_.setCache(config_with_hash_key, foo_client);
  }
  waitForSeconds(30);
  // Cache entries created 50s before have expired.
  for (int i = 1; i <= 50; i++) {
    grpc_service.mutable_envoy_grpc()->set_cluster_name(std::to_string(i));
    GrpcServiceConfigWithHashKey config_with_hash_key(grpc_service);
    EXPECT_EQ(client_cache_.getCache(config_with_hash_key).get(), nullptr);
  }
  // Cache entries 30s before haven't expired.
  for (int i = 51; i <= 100; i++) {
    grpc_service.mutable_envoy_grpc()->set_cluster_name(std::to_string(i));
    GrpcServiceConfigWithHashKey config_with_hash_key(grpc_service);
    EXPECT_EQ(client_cache_.getCache(config_with_hash_key).get(), foo_client.get());
  }
}

// Test the case when the eviction timer doesn't fire on time, getting the oldest entry that has
// already expired but hasn't been evicted should succeed.
TEST_F(RawAsyncClientCacheTest, GetExpiredButNotEvictedCacheEntry) {
  envoy::config::core::v3::GrpcService foo_service;
  foo_service.mutable_envoy_grpc()->set_cluster_name("foo");
  RawAsyncClientSharedPtr foo_client = std::make_shared<MockAsyncClient>();
  GrpcServiceConfigWithHashKey config_with_hash_key(foo_service);
  client_cache_.setCache(config_with_hash_key, foo_client);
  time_system_.advanceTimeAsyncImpl(std::chrono::seconds(50));
  // Cache entry hasn't been evicted because it is accessed before timer fire.
  EXPECT_EQ(client_cache_.getCache(config_with_hash_key).get(), foo_client.get());
  time_system_.advanceTimeAndRun(std::chrono::seconds(50), *dispatcher_,
                                 Event::Dispatcher::RunType::NonBlock);
  // Cache entry has been evicted because it is accessed after timer fire.
  EXPECT_EQ(client_cache_.getCache(config_with_hash_key).get(), nullptr);
}

class RawAsyncClientCacheTestBusyLoop : public testing::Test {
public:
  RawAsyncClientCacheTestBusyLoop() {
    timer_ = new Event::MockTimer();
    EXPECT_CALL(dispatcher_, createTimer_(_)).WillOnce(Invoke([this](Event::TimerCb) {
      return timer_;
    }));
    client_cache_ = std::make_unique<AsyncClientManagerImpl::RawAsyncClientCache>(dispatcher_);
    EXPECT_CALL(*timer_, enableTimer(testing::Not(std::chrono::milliseconds(0)), _))
        .Times(testing::AtLeast(1));
  }

  void waitForMilliSeconds(int ms) {
    for (int i = 0; i < ms; i++) {
      time_system_.advanceTimeAndRun(std::chrono::milliseconds(1), dispatcher_,
                                     Event::Dispatcher::RunType::NonBlock);
    }
  }

protected:
  Event::SimulatedTimeSystem time_system_;
  NiceMock<Event::MockDispatcher> dispatcher_;
  Event::MockTimer* timer_;
  std::unique_ptr<AsyncClientManagerImpl::RawAsyncClientCache> client_cache_;
};

TEST_F(RawAsyncClientCacheTestBusyLoop, MultipleCacheEntriesEvictionBusyLoop) {
  envoy::config::core::v3::GrpcService grpc_service;
  RawAsyncClientSharedPtr foo_client = std::make_shared<MockAsyncClient>();
  // two entries are added to the cache
  for (int i = 1; i <= 2; i++) {
    grpc_service.mutable_envoy_grpc()->set_cluster_name(std::to_string(i));
    GrpcServiceConfigWithHashKey config_with_hash_key(grpc_service);
    client_cache_->setCache(config_with_hash_key, foo_client);
  }
  // waiting for 49.2 secs to make sure that for the entry which is not accessed, time to expire is
  // less than 1 second, ~0.8 secs
  waitForMilliSeconds(49200);

  // Access first cache entry to so that evictEntriesAndResetEvictionTimer() gets called.
  // Since we are getting first entry, access time of first entry will be updated to current time.
  grpc_service.mutable_envoy_grpc()->set_cluster_name(std::to_string(1));
  GrpcServiceConfigWithHashKey config_with_hash_key_1(grpc_service);
  EXPECT_EQ(client_cache_->getCache(config_with_hash_key_1).get(), foo_client.get());

  // Verifying that though the time to expire for second entry ~0.8 sec, it is considered as expired
  // to avoid the busy loop which could happen if timer gets enabled with 0(0.8 rounded off to 0)
  // duration.
  grpc_service.mutable_envoy_grpc()->set_cluster_name(std::to_string(2));
  GrpcServiceConfigWithHashKey config_with_hash_key_2(grpc_service);
  EXPECT_EQ(client_cache_->getCache(config_with_hash_key_2).get(), nullptr);
}

class AsyncClientManagerImplTest : public testing::Test {
public:
  AsyncClientManagerImplTest()
      : api_(Api::createApiForTest()), stat_names_(scope_.symbolTable()),
        async_client_manager_(cm_, tls_, test_time_.timeSystem(), *api_, stat_names_) {}

  Upstream::MockClusterManager cm_;
  NiceMock<ThreadLocal::MockInstance> tls_;
  Stats::MockStore store_;
  Stats::MockScope& scope_{store_.mockScope()};
  DangerousDeprecatedTestTime test_time_;
  Api::ApiPtr api_;
  StatNames stat_names_;
  AsyncClientManagerImpl async_client_manager_;
};

TEST_F(AsyncClientManagerImplTest, EnvoyGrpcOk) {
  envoy::config::core::v3::GrpcService grpc_service;
  grpc_service.mutable_envoy_grpc()->set_cluster_name("foo");
  EXPECT_CALL(cm_, checkActiveStaticCluster("foo")).WillOnce(Return());
  async_client_manager_.factoryForGrpcService(grpc_service, scope_, false);
}

TEST_F(AsyncClientManagerImplTest, GrpcServiceConfigWithHashKeyTest) {
  envoy::config::core::v3::GrpcService grpc_service;
  grpc_service.mutable_envoy_grpc()->set_cluster_name("foo");
  envoy::config::core::v3::GrpcService grpc_service_c;
  grpc_service.mutable_envoy_grpc()->set_cluster_name("bar");

  GrpcServiceConfigWithHashKey config_with_hash_key_a = GrpcServiceConfigWithHashKey(grpc_service);
  GrpcServiceConfigWithHashKey config_with_hash_key_b = GrpcServiceConfigWithHashKey(grpc_service);
  GrpcServiceConfigWithHashKey config_with_hash_key_c =
      GrpcServiceConfigWithHashKey(grpc_service_c);
  EXPECT_TRUE(config_with_hash_key_a == config_with_hash_key_b);
  EXPECT_FALSE(config_with_hash_key_a == config_with_hash_key_c);

  EXPECT_EQ(config_with_hash_key_a.getPreComputedHash(),
            config_with_hash_key_b.getPreComputedHash());
  EXPECT_NE(config_with_hash_key_a.getPreComputedHash(),
            config_with_hash_key_c.getPreComputedHash());
}

TEST_F(AsyncClientManagerImplTest, RawAsyncClientCache) {
  envoy::config::core::v3::GrpcService grpc_service;
  grpc_service.mutable_envoy_grpc()->set_cluster_name("foo");

  // Use cache when runtime is enabled.
  RawAsyncClientSharedPtr foo_client0 =
      async_client_manager_.getOrCreateRawAsyncClient(grpc_service, scope_, true);
  RawAsyncClientSharedPtr foo_client1 =
      async_client_manager_.getOrCreateRawAsyncClient(grpc_service, scope_, true);
  EXPECT_EQ(foo_client0.get(), foo_client1.get());

  // Get a different raw async client with different cluster config.
  grpc_service.mutable_envoy_grpc()->set_cluster_name("bar");
  RawAsyncClientSharedPtr bar_client =
      async_client_manager_.getOrCreateRawAsyncClient(grpc_service, scope_, true);
  EXPECT_NE(foo_client1.get(), bar_client.get());
}

TEST_F(AsyncClientManagerImplTest, EnvoyGrpcInvalid) {
  envoy::config::core::v3::GrpcService grpc_service;
  grpc_service.mutable_envoy_grpc()->set_cluster_name("foo");
  EXPECT_CALL(cm_, checkActiveStaticCluster("foo")).WillOnce(Invoke([](const std::string&) {
    throw EnvoyException("fake exception");
  }));
  EXPECT_THROW_WITH_MESSAGE(
      async_client_manager_.factoryForGrpcService(grpc_service, scope_, false), EnvoyException,
      "fake exception");
}

TEST_F(AsyncClientManagerImplTest, GoogleGrpc) {
  EXPECT_CALL(scope_, createScope_("grpc.foo."));
  envoy::config::core::v3::GrpcService grpc_service;
  grpc_service.mutable_google_grpc()->set_stat_prefix("foo");

#ifdef ENVOY_GOOGLE_GRPC
  EXPECT_NE(nullptr, async_client_manager_.factoryForGrpcService(grpc_service, scope_, false));
#else
  EXPECT_THROW_WITH_MESSAGE(
      async_client_manager_.factoryForGrpcService(grpc_service, scope_, false), EnvoyException,
      "Google C++ gRPC client is not linked");
#endif
}

TEST_F(AsyncClientManagerImplTest, GoogleGrpcIllegalCharsInKey) {
  EXPECT_CALL(scope_, createScope_("grpc.foo."));
  envoy::config::core::v3::GrpcService grpc_service;
  grpc_service.mutable_google_grpc()->set_stat_prefix("foo");

  auto& metadata = *grpc_service.mutable_initial_metadata()->Add();
  metadata.set_key("illegalcharacter;");
  metadata.set_value("value");

#ifdef ENVOY_GOOGLE_GRPC
  EXPECT_THROW_WITH_MESSAGE(
      async_client_manager_.factoryForGrpcService(grpc_service, scope_, false), EnvoyException,
      "Illegal characters in gRPC initial metadata header key: illegalcharacter;.");
#else
  EXPECT_THROW_WITH_MESSAGE(
      async_client_manager_.factoryForGrpcService(grpc_service, scope_, false), EnvoyException,
      "Google C++ gRPC client is not linked");
#endif
}

TEST_F(AsyncClientManagerImplTest, LegalGoogleGrpcChar) {
  EXPECT_CALL(scope_, createScope_("grpc.foo."));
  envoy::config::core::v3::GrpcService grpc_service;
  grpc_service.mutable_google_grpc()->set_stat_prefix("foo");

  auto& metadata = *grpc_service.mutable_initial_metadata()->Add();
  metadata.set_key("_legal-character.");
  metadata.set_value("value");

#ifdef ENVOY_GOOGLE_GRPC
  EXPECT_NE(nullptr, async_client_manager_.factoryForGrpcService(grpc_service, scope_, false));
#else
  EXPECT_THROW_WITH_MESSAGE(
      async_client_manager_.factoryForGrpcService(grpc_service, scope_, false), EnvoyException,
      "Google C++ gRPC client is not linked");
#endif
}

TEST_F(AsyncClientManagerImplTest, GoogleGrpcIllegalCharsInValue) {
  EXPECT_CALL(scope_, createScope_("grpc.foo."));
  envoy::config::core::v3::GrpcService grpc_service;
  grpc_service.mutable_google_grpc()->set_stat_prefix("foo");

  auto& metadata = *grpc_service.mutable_initial_metadata()->Add();
  metadata.set_key("legal-key");
  metadata.set_value("NonAsciValue.भारत");

#ifdef ENVOY_GOOGLE_GRPC
  EXPECT_THROW_WITH_MESSAGE(
      async_client_manager_.factoryForGrpcService(grpc_service, scope_, false), EnvoyException,
      "Illegal ASCII value for gRPC initial metadata header key: legal-key.");
#else
  EXPECT_THROW_WITH_MESSAGE(
      async_client_manager_.factoryForGrpcService(grpc_service, scope_, false), EnvoyException,
      "Google C++ gRPC client is not linked");
#endif
}

TEST_F(AsyncClientManagerImplTest, EnvoyGrpcUnknownSkipClusterCheck) {
  envoy::config::core::v3::GrpcService grpc_service;
  grpc_service.mutable_envoy_grpc()->set_cluster_name("foo");

  EXPECT_CALL(cm_, checkActiveStaticCluster(_)).Times(0);
  ASSERT_NO_THROW(async_client_manager_.factoryForGrpcService(grpc_service, scope_, true));
}

} // namespace
} // namespace Grpc
} // namespace Envoy
