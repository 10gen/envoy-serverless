#!/bin/bash

# This script runs in a container built from the Dockerfiles.
# It assumes the workspace directory is mounted on /etc/envoy-serverless and the build directory is optionally mounted on /build.

set -xeuo pipefail

source $(dirname "$0")/setup_env.sh

# If this script can be executed, then /etc/envoy-serverless must be accessable.
cd /etc/envoy-serverless
bazel ${BAZEL_STARTUP_FLAGS} build ${BAZEL_FLAGS} -- //:envoy
