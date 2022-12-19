#!/bin/bash

# This script runs in a container built from Dockerfile-centos7.
# It assumes the workspace directory is mounted on /etc/envoy-serverless and the build directory is optionally mount on /build

set -euo pipefail
set -x

source $(dirname "$0")/set-up-env-centos7.sh

# If this script can be executed, then /etc/envoy-serverless must be accessable.
cd /etc/envoy-serverless
bazel ${bazel_startup_flags} build ${bazel_flags} -- //:envoy

