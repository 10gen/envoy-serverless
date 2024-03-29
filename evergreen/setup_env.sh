#!/bin/bash

# This script runs in a container built from the Dockerfiles.
# It assumes the workspace directory is mounted on /etc/envoy-serverless and the build directory is optionally mounted on /build

set -xeuo pipefail

export bazel_flags="--config=linux --config=clang --copt=-fsized-deallocation -c opt --action_env=PATH --verbose_failures -k"

if grep -q "CentOS Linux 7" /etc/os-release; then
  echo "Using MongoDB toolchain on CentOS Linux 7"

  # Use MongoDB toolchain installed in the docker image.
  export LD_LIBRARY_PATH="/opt/mongodbtoolchain/v4/lib"
  export CC=/opt/mongodbtoolchain/v4/bin/clang
  export CXX=/opt/mongodbtoolchain/v4/bin/clang++
  export bazel_flags="${bazel_flags} --action_env=CC=${CC} --action_env=CXX=${CXX} --action_env=LD_LIBRARY_PATH"
fi

# Set the bazel repository cache and output base to /build, so that they are not on the docker overlay volume.
export BUILD_DIR=${BUILD_DIR:-/build}
if [[ ! -d "${BUILD_DIR}" ]]
then
  echo "${BUILD_DIR} mount missing - did you forget -v <something>:${BUILD_DIR}? Creating."
  mkdir -p "${BUILD_DIR}"
fi
export bazel_startup_flags="--output_base=${BUILD_DIR}"
