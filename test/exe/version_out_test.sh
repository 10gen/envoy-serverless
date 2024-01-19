#!/bin/bash

set -e -o pipefail

# Undo LC_ALL=en_US.UTF-8, since it breaks sed.
export LC_ALL=C

ENVOY_BIN="${TEST_SRCDIR}/envoy/source/exe/envoy-static"

# Note: The output of the --version command differs based on the presence of a RELEASE_VERSION file during bazel workspace status generation.
# If the file is present, the --version command output will include the content of the file as a prefix to the text after the first '/'.
# For example:
# bazel-bin/envoy  version: 2f7ac770075de46ce960f4da95b40bd49834dcb1/20240104-1.26.3-65-g2f7ac77/Modified/DEBUG/BoringSSL
# where 20240104 is taken from the RELEASE_VERSION file.
# If RELEASE_VERSION was not present, the --version command output would be:
# bazel-bin/envoy  version: 2f7ac770075de46ce960f4da95b40bd49834dcb1/1.26.3-65-g2f7ac77/Modified/DEBUG/BoringSSL
# This prefix is optionally captured by the second regex capture group below.

# Example expected COMMIT: 2f7ac770075de46ce960f4da95b40bd49834dcb1

COMMIT=$(${ENVOY_BIN} --version | \
  sed -n -E 's/.*version: ([0-9a-f]{40})\/([0-9]+-)?([0-9]+\.[0-9]+\.[0-9]+)(-[a-zA-Z0-9_\-]+)?\/(Clean|Modified)\/(RELEASE|DEBUG)\/([a-zA-Z-]+)$/\1/p')

EXPECTED=$(cat "${TEST_SRCDIR}/envoy/bazel/raw_build_id.ldscript")

if [[ "${COMMIT}" != "${EXPECTED}" ]]; then
  echo "Commit mismatch, got: ${COMMIT}, expected: ${EXPECTED}".
  exit 1
fi

# Example expected VERSION: 20240104-1.26.3-65-g2f7ac77

VERSION=$(${ENVOY_BIN} --version | \
  sed -n -E 's/.*version: ([0-9a-f]{40})\/([0-9]+-)?([0-9]+\.[0-9]+\.[0-9]+)(-[a-zA-Z0-9_\-]+)?\/(Clean|Modified)\/(RELEASE|DEBUG)\/([a-zA-Z-]+)$/\2\3\4/p')

EXPECTED=$(cat "${TEST_SRCDIR}/envoy/bazel/git_version.txt")

if [[ "${VERSION}" != "${EXPECTED}" ]]; then
  echo "Version mismatch, got: ${VERSION}, expected: ${EXPECTED}".
  exit 1
fi
