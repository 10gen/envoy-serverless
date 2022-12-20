 #!/bin/bash
set -xeuo pipefail

sudo yum install -y podman fuse-overlayfs conmon
export REPODIR=/etc/envoy-serverless
export SRCDIR=$(pwd)/src
mkdir -p ./build
sudo podman --root="$(pwd)/containers" run \
    -v "$SRCDIR:$REPODIR" \
    -v "$(pwd)/build:/build" \
    docker.io/siyuanzhou/envoy-serverless-centos7 \
    $REPODIR/evergreen/compile-centos7.sh
