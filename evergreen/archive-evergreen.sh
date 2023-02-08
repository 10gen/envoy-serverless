#!/bin/bash

# Assume $DISTRO and $ARCH are set.

set -xeuo pipefail

SRCDIR=$(pwd)/src
cd $SRCDIR
# Generate version from the most recent base tag.
# For example, 1.21.3-4-gdbdcfa34cf is based on the most recent tag "v1.21.3" and
# has "4" commits on top of that, then it appends the "g" prefix (for git) and
# an abbreviated object name for the commit. The leading "v" is then omitted.
# --dirty: If the working tree has local modification "-dirty" is appended to it.
# --always: Show uniquely abbreviated commit object as fallback when no tag is found.
VERSION=$(git describe --tags --dirty --always --match 'v[0-9]*' | cut -c 2-)
cd -

# The archives have to be put in a directory because evergreen s3.put will scan the working directory
# to find files specified by "local_files_include_filter". However, the build files created by
# the docker containers cannot be accessed due to permissions, leading to a failure. Changing the
# working directory to "ARCHIVE_PATH" is a workaround.
ARCHIVE_PATH=archive
# Example archive file name: envoy-serverless-1.21.3-4-gdbdcfa3.rhel7.amd64.tar.gz
ARCHIVE_DIR=envoy-serverless-${VERSION}.${DISTRO}.${ARCH}
mkdir -p ${ARCHIVE_PATH}/${ARCHIVE_DIR}

if [ $ARCH == "aarch64" ]; then
	OUTPUT_DIR="aarch64-opt"
else
	OUTPUT_DIR="k8-opt"
fi
# Only include the envoy binary and the hot-restarter. The start script and the static config should
# be prepared by the agent.
cp build/execroot/envoy/bazel-out/$OUTPUT_DIR/bin/source/exe/envoy-static ${ARCHIVE_PATH}/${ARCHIVE_DIR}/envoy-serverless
cp ${SRCDIR}/restarter/hot-restarter.py ${ARCHIVE_PATH}/${ARCHIVE_DIR}/
cd ${ARCHIVE_PATH}
tar -zcvf ${ARCHIVE_DIR}.tar.gz ${ARCHIVE_DIR}
