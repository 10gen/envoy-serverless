#!/bin/bash

# This script runs in a container built from Dockerfile-centos7.
# It assumes the workspace directory is mounted on /etc/envoy-serverless and the build directory is optionally mount on /build

set -euo pipefail
set -x

source $(dirname "$0")/set-up-env-centos7.sh

# Test targets listed in repo's .bazelci config
export bazel_tests="//test/common/common/... //test/integration/... //test/exe/..."
# Include serverless specific patches.
export bazel_tests="${bazel_tests} //test/extensions/filters/listener/proxy_protocol:proxy_protocol_test //test/extensions/common/proxy_protocol:proxy_protocol_header_test //test/extensions/transport_sockets/proxy_protocol:proxy_protocol_integration_test"
# This test started failing in 1.17, but only when run by evergreen. not reproducible
# in any environment other than being executed by jasper/evergreen curator.
export bazel_tests="${bazel_tests} -//test/common/common:logger_speed_test_benchmark_test"
export bazel_test_flags="--test_output=errors --flaky_test_attempts=//test/integration.*@3"

# If this script can be executed, then /etc/envoy-serverless must be accessable.
cd /etc/envoy-serverless
bazel ${bazel_startup_flags} test ${bazel_flags} ${bazel_test_flags} -- ${bazel_tests}

