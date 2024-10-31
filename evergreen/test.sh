#!/bin/bash

# This script runs in a container built from the Dockerfile.
# It assumes the workspace directory is mounted on /etc/envoy-serverless and the build directory is optionally mounted on /build.

set -xeuo pipefail

source $(dirname "$0")/setup_env.sh

# Create a symlink to the location of the python3 binary. Overwrite existing system python3 binary available on al2023 (and there's no python3 binary in /usr/bin on al2).
ln -sf /opt/mongodbtoolchain/v4/bin/python3 /usr/bin/python3

# Test targets listed in repo's .bazelci config and includes serverless specific patches.
export BAZEL_TESTS="//test/common/common/... \
    //test/integration/... //test/exe/... \
    //test/extensions/filters/listener/proxy_protocol:proxy_protocol_test \
    //test/extensions/common/proxy_protocol:proxy_protocol_header_test \
    //test/extensions/transport_sockets/proxy_protocol:proxy_protocol_integration_test \
    -//test/common/common:logger_speed_test_benchmark_test \
    -//test/integration/admin_html:test_server_test"

# CLOUDP-157636: The following test targets require a glibc version at/above 2.27 when running Envoy versions >= 1.22.
#   Because our testing distros other than al2023 do not have this glibc version, we won't run them for now. 
#       //test/exe:main_common_test
#       //test/exe:win32_scm_test
#       //test/integration:hotrestart_test
#       //test/integration:run_envoy_test
# Note that in CLOUDP-157636, we only enabled submodule tests on al2023 distros. This check remains in case we 
# want to enable tests on al2.
if [[ "${DISTRO}" != "al2023" ]]; then
    export BAZEL_TESTS="${BAZEL_TESTS} -//test/exe:main_common_test -//test/exe:win32_scm_test -//test/integration:hotrestart_test -//test/integration:run_envoy_small_test"
fi

export BAZEL_TEST_FLAGS="--test_output=errors --flaky_test_attempts=//test/integration.*@3"

# If this script can be executed, then /etc/envoy-serverless must be accessable.
cd /etc/envoy-serverless
bazel ${BAZEL_STARTUP_FLAGS} test ${BAZEL_FLAGS} ${BAZEL_TEST_FLAGS} -- ${BAZEL_TESTS}
