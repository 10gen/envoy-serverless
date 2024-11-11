#!/bin/bash

# This script runs in a container built from the Dockerfiles.
# It assumes the workspace directory is mounted on /etc/envoy-serverless and the build directory is optionally mounted on /build.

set -xeuo pipefail

# Set the bazel repository cache and output base to /build, so that they are not on the docker overlay volume.
export BUILD_DIR=${BUILD_DIR:-/build}
if [[ ! -d "${BUILD_DIR}" ]]
then
  echo "${BUILD_DIR} mount missing - did you forget -v <something>:${BUILD_DIR}? Creating..."
  mkdir -p "${BUILD_DIR}"
fi

export BAZEL_FLAGS="--config=linux --config=clang --compilation_mode=opt --verbose_failures --subcommands --keep_going"
export BAZEL_STARTUP_FLAGS="--output_base=${BUILD_DIR}"
