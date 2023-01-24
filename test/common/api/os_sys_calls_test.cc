#ifdef WIN32
#include <fcntl.h> // Needed for `O_CREAT` and `O_RDWR` on Win32.

#endif

#include "source/common/api/os_sys_calls_impl.h"

#include "test/test_common/environment.h"

#include "gtest/gtest.h"

namespace Envoy {

// Test happy path for `open`, `pwrite`, `pread`, `fstat`, `close`, `stat` and `unlink`.
TEST(OsSyscallsTest, OpenPwritePreadFstatCloseStatUnlink) {
  auto& os_syscalls = Api::OsSysCallsSingleton::get();
  std::string path{TestEnvironment::temporaryPath("envoy_test")};
  TestEnvironment::createPath(path);
  std::string file_path = path + "/file";
  absl::string_view file_contents = "12345";
// Test `open`
#ifdef WIN32
  int pmode = _S_IWRITE | _S_IREAD;
#else
  int pmode = S_IRUSR | S_IWUSR;
#endif
  Api::SysCallIntResult open_result = os_syscalls.open(file_path.c_str(), O_CREAT | O_RDWR, pmode);
  EXPECT_NE(open_result.return_value_, -1);
  EXPECT_EQ(open_result.errno_, 0);
  os_fd_t fd = open_result.return_value_;
#ifdef WIN32
  // `pwrite` and `pread` are not supported. Just write some bytes so we can still test stat.
  EXPECT_EQ(file_contents.size(), ::_write(fd, file_contents.begin(), file_contents.size()));
#else
  // Test `pwrite`
  Api::SysCallSizeResult write_result =
      os_syscalls.pwrite(fd, file_contents.begin(), file_contents.size(), 0);
  EXPECT_EQ(write_result.return_value_, file_contents.size());
  EXPECT_EQ(write_result.errno_, 0);
  // Test `pread`
  char read_buffer[5];
  Api::SysCallSizeResult read_result = os_syscalls.pread(fd, read_buffer, sizeof(read_buffer), 0);
  EXPECT_EQ(read_result.return_value_, sizeof(read_buffer));
  EXPECT_EQ(read_result.errno_, 0);
  absl::string_view read_buffer_view{read_buffer, sizeof(read_buffer)};
  EXPECT_EQ(file_contents, read_buffer_view);
#endif
  // Test `fstat`
  struct stat fstat_value;
  Api::SysCallIntResult fstat_result = os_syscalls.fstat(fd, &fstat_value);
  EXPECT_EQ(fstat_result.return_value_, 0);
  EXPECT_EQ(fstat_result.errno_, 0);
  EXPECT_EQ(fstat_value.st_size, file_contents.size());
  // Test `close`
  Api::SysCallIntResult close_result = os_syscalls.close(fd);
  EXPECT_EQ(close_result.return_value_, 0);
  EXPECT_EQ(close_result.errno_, 0);
  // Test `stat`
  struct stat stat_value;
  Api::SysCallIntResult stat_result = os_syscalls.stat(file_path.c_str(), &stat_value);
  EXPECT_EQ(stat_result.return_value_, 0);
  EXPECT_EQ(stat_result.errno_, 0);
  EXPECT_EQ(stat_value.st_size, file_contents.size());
  // Test `unlink`
  Api::SysCallIntResult unlink_result = os_syscalls.unlink(file_path.c_str());
  EXPECT_EQ(unlink_result.return_value_, 0);
  EXPECT_EQ(unlink_result.errno_, 0);
  TestEnvironment::removePath(path);
}

TEST(OsSyscallsTest, SetAlternateGetifaddrs) {
  auto& os_syscalls = Api::OsSysCallsSingleton::get();
  const bool pre_alternate_support = os_syscalls.supportsGetifaddrs();
  Api::InterfaceAddressVector interfaces{};
#if defined(WIN32) || (defined(__ANDROID_API__) && __ANDROID_API__ < 24)
  EXPECT_FALSE(pre_alternate_support);
  EXPECT_DEATH(os_syscalls.getifaddrs(interfaces), "not implemented");
#else
  EXPECT_TRUE(pre_alternate_support);
  const auto pre_alternate_rc = os_syscalls.getifaddrs(interfaces);
  EXPECT_EQ(0, pre_alternate_rc.return_value_);
  EXPECT_FALSE(interfaces.empty());
#endif

  os_syscalls.setAlternateGetifaddrs(
      [](Api::InterfaceAddressVector& interfaces) -> Api::SysCallIntResult {
        interfaces.emplace_back("made_up_if", 0, nullptr);
        return {0, 0};
      });
  interfaces.clear();

  const bool post_alternate_support = os_syscalls.supportsGetifaddrs();
  EXPECT_TRUE(post_alternate_support);
  EXPECT_EQ(0, os_syscalls.getifaddrs(interfaces).return_value_);
  EXPECT_EQ(1, interfaces.size());
  EXPECT_EQ("made_up_if", interfaces.front().interface_name_);
}
} // namespace Envoy
