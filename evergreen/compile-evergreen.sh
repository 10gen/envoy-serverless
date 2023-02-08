 #!/bin/bash
set -xeuo pipefail

# Assume $DOCKER_IMAGE is set.

sudo yum install -y podman fuse-overlayfs conmon
export REPODIR=/etc/envoy-serverless
export SRCDIR=$(pwd)/src
mkdir -p ./build
sudo podman --root="$(pwd)/containers" run \
    -v "$SRCDIR:$REPODIR" \
    -v "$(pwd)/build:/build" \
    $DOCKER_IMAGE \
    $REPODIR/evergreen/compile.sh
