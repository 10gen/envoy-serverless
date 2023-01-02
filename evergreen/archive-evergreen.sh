#!/bin/bash
set -xeuo pipefail

SRCDIR=$(pwd)/src
cd $SRCDIR
# Generate version from the most recent base tag, e.g. v1.21.3-4-gdbdcfa34cf
# This example is based on the most recent tag "v1.21.3" and has "4" commits on top of that,
# then it appends the "g" prefix (for git) and an abbreviated object name for the commit.
# --dirty: If the working tree has local modification "-dirty" is appended to it.
# --always: Show uniquely abbreviated commit object as fallback when no tag is found.
VERSION=$(git describe --tags --dirty --always --match 'v[0-9]*')
cd -

ARCHIVE_PATH=archive
ARCHIVE_DIR=$ARCHIVE_PATH/envoy-serverless-${VERSION}.centos7.amd64
mkdir -p ${ARCHIVE_DIR}
# Only include the envoy binary and the hot-restarter. The start script and the static config should
# be prepared by the agent.
cp build/execroot/envoy/bazel-out/k8-opt/bin/source/exe/envoy-static ${ARCHIVE_DIR}/envoy-serverless
cp ${SRCDIR}/restarter/hot-restarter.py ${ARCHIVE_DIR}
tar -zcvf ${ARCHIVE_DIR}.tar.gz ${ARCHIVE_DIR}
