#!/bin/bash

# This script runs in a container built from Dockerfile-centos7.
# It assumes the workspace directory is mounted on /etc/envoy-serverless and the build directory is optionally mount on /build

set -euo pipefail
set -x

# Use MongoDB toolchain installed in the docker image.
export LD_LIBRARY_PATH="/opt/mongodbtoolchain/v4/lib"
export CC=/opt/mongodbtoolchain/v4/bin/clang
export CXX=/opt/mongodbtoolchain/v4/bin/clang++

# Test targets listed in repo's .bazelci config
export bazel_tests="//test/common/common/... //test/integration/... //test/exe/..."
# Include serverless specific patches.
export bazel_tests="${bazel_tests} //test/extensions/filters/listener/proxy_protocol:proxy_protocol_test //test/extensions/common/proxy_protocol:proxy_protocol_header_test //test/extensions/transport_sockets/proxy_protocol:proxy_protocol_integration_test"
# This test started failing in 1.17, but only when run by evergreen. not reproducible
# in any environment other than being executed by jasper/evergreen curator.
export bazel_tests="${bazel_tests} -//test/common/common:logger_speed_test_benchmark_test"
export bazel_flags="--config=linux --config=clang"
export bazel_flags="${bazel_flags} -c opt --action_env=PATH --action_env=CC=${CC} --action_env=CXX=${CXX} --action_env=LD_LIBRARY_PATH --verbose_failures -k"
export bazel_test_flags="--test_output=errors --flaky_test_attempts=//test/integration.*@3"

# Set the bazel repository cache and output base to /build, so that they are not on the docker overlay volume.
export BUILD_DIR=${BUILD_DIR:-/build}
if [[ ! -d "${BUILD_DIR}" ]]
then
  echo "${BUILD_DIR} mount missing - did you forget -v <something>:${BUILD_DIR}? Creating."
  mkdir -p "${BUILD_DIR}"
fi
export bazel_startup_flags="--output_base=${BUILD_DIR}"

# If this script can be executed, then /etc/envoy-serverless must be accessable.
cd /etc/envoy-serverless
bazel ${bazel_startup_flags} test ${bazel_flags} ${bazel_test_flags} -- ${bazel_tests}
bazel ${bazel_startup_flags} build ${bazel_flags} -- //:envoy

