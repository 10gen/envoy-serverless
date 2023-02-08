#!/bin/bash

# This script runs in a container built from the Dockerfiles.
# It assumes the workspace directory is mounted on /etc/envoy-serverless and the build directory is optionally mounted on /build

set -xeuo pipefail

source $(dirname "$0")/set-up-env.sh

# If this script can be executed, then /etc/envoy-serverless must be accessable.
cd /etc/envoy-serverless
bazel ${bazel_startup_flags} build ${bazel_flags} -- //:envoy

