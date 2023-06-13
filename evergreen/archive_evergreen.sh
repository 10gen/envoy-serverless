#!/bin/bash

# Assume DISTRO and ARCH environment variables are set.

set -xeuo pipefail

SRCDIR=$(pwd)/src
cd $SRCDIR

# Generate the version from the most recent base tag.
#
# For example, 0.0.1-4-gdbdcfa34cf is based on the most recent tag "v0.0.1" and has "4" commits
# on top of that, then it appends the "g" prefix (for Git) and an abbreviated object name for the
# commit. The leading "v" is then omitted.
# --dirty: If the working tree has a local modification, "-dirty" is appended to it.
# --always: Show uniquely abbreviated commit object as a fallback when no tag is found.
VERSION=$(git describe --tags --dirty --always --match 'v[0-9]*' --abbrev=7 | cut -c 2-)
cd - 

# The tarball needs to be placed in another directory because Evergreen's s3.put will scan the
# working directory to find files specified by local_files_include_filter. Unfortunately, the
# build files created by Docker containers are inaccessible due to permissions issues. We change
# the working directory to ARCHIVE_PATH as a workaround.
ARCHIVE_PATH="$(pwd)/archive"

# Example archive file name: envoy-serverless-0.0.1-4-gdbdcfa34cf.rhel7.amd64.tar.gz
ARCHIVE_DIR="envoy-serverless-${VERSION}.${DISTRO}.${ARCH}"
mkdir -p "${ARCHIVE_PATH}/${ARCHIVE_DIR}"

OUTPUT_DIR=$([ "${ARCH}" = "aarch64" ] && echo "aarch64" || echo "k8")

# Only include the Envoy binary and the hot restarter script in the tarball. The startup script and
# static Envoy config will be prepared by the automation agent.

cp "${SRCDIR}/build/execroot/envoy/bazel-out/${OUTPUT_DIR}-opt/bin/source/exe/envoy-static" "${ARCHIVE_PATH}/${ARCHIVE_DIR}/envoy-serverless"
cp "${SRCDIR}/restarter/hot-restarter.py" "${ARCHIVE_PATH}/${ARCHIVE_DIR}/"
cd "${ARCHIVE_PATH}" && tar -cvzf "${ARCHIVE_DIR}.tar.gz" "${ARCHIVE_DIR}"
