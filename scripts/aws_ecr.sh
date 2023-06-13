#!/bin/bash

# We will set the -u flag after option parsing.
set -eo pipefail

function usage() {
  cat <<EOF
This script interacts with the AWS ECR repositories on mms-scratch associated with the serverless proxy. They include:
- serverless-proxy/centos7
- serverless-proxy/al2-x86_64
- serverless-proxy/al2-aarch64
- serverless-proxy/al2022-x86_64
- serverless-proxy/al2022-aarch64

Before running the script, ensure that 'aws configure' has been run on the machine. You may also opt to set the environment variables AWS_ACCESS_KEY_ID and AWS_SECRET_ACCESS_KEY instead.

The script:
1. Computes a Docker image tag from the appropriate Dockerfile's SHA-256 hash (see --distro below).
2. Checks AWS ECR for a Docker image with the computed tag.
3. If the Docker image doesn't exist, optionally builds and/or pushes to AWS ECR.
4. If the Docker image exists, optionally pulls from AWS ECR.

Usage:
  aws_ecr.sh --distro DISTRO [--build-docker-image] [--push-docker-image] [--pull-docker-image] [--run-script SCRIPT_PATH] [--mount-build-dir BUILD_DIR] [--mount-atlasproxy-dir ATLASPROXY_DIR] [--mount-ssh-dir SSH_DIR] [--interactive] [--detached] [--use-podman] [--print-trace]
  aws_ecr.sh -d DISTRO [-b] [-p] [-l] [-r SCRIPT_PATH] [-v BUILD_DIR] [-a ATLASPROXY_DIR] [-s SSH_DIR] [-i] [-t] [-u] [-x]

Options:
  -d, --distro                  Linux distribution that determines the Dockerfile used (rhel7, al2, al2022). Required argument.
  -b, --build-docker-image      Build new Docker image if it doesn't exist in AWS ECR. Optional argument.
  -p, --push-docker-image       Push new Docker image to AWS ECR. No-op if --build-docker-image is absent. Optional argument.
  -l, --pull-docker-image       Pull Docker image from AWS ECR if it exists. Optional argument.
  -r, --run-script              Run script on Docker image if it exists. No-op if --build-docker-image or --pull-docker-image is absent. Optional argument.
  -v, --mount-build-dir         Mount a Bazel build directory to the Docker container running the script. No-op if --run-script is absent. Optional argument.
  -a, --mount-atlasproxy-dir    Mount an Atlas Proxy repository to the Docker container running the script. No-op if --run-script is absent. Optional argument.
  -s, --mount-ssh-dir           Mount an SSH directory to the Docker container running the script. No-op if --run-script is absent. Optional argument.
  -i, --interactive             Run script in interactive mode. We attempt to re-use existing containers in interactive mode. No-op if --run-script is absent. Optional argument.
  -t, --detached                Run script in detached move. No-op if --run-script is absent. Optional argument.
  -u, --use-podman              Use Podman as a replacement for Docker. Optional argument.
  -x, --print-trace             Run 'set -x' to print a trace of simple commands. Optional argument.
EOF
}

function cleanup() {
  # Save the pre-cleanup exit code so we can tell if the script actually succeeded or failed.
  EXIT_CODE=$?
  echo "INFO: Running post-script cleanup."
  if ${USE_PODMAN}; then
    podman logout --all
  else
    docker logout "${ECR_REPO_URI:-}"
  fi
  exit "${EXIT_CODE}"
}

function login() {
  # Requesting the login password for AWS ECR differs between AWS CLI version 1 and 2+.
  if [ "$(aws --version 2>&1 | cut -d " " -f1 | cut -d "/" -f2 | cut -d "." -f1)" = "1" ]; then
    aws ecr get-login --region us-east-1 | cut -d " " -f6 | eval "${DOCKER_COMMAND}" login --username \
      AWS --password-stdin "${ECR_REPO_URI}" # AWS CLI version 1
  else
    aws ecr get-login-password --region us-east-1 | eval "${DOCKER_COMMAND}" login --username AWS \
      --password-stdin "${ECR_REPO_URI}" # AWS CLI version 2+
  fi
}

function handle_docker_image_doesnt_exist() {
  if ${BUILD_DOCKER_IMAGE}; then
    # Build the Docker image.
    echo "INFO: Building Docker image ${ECR_REPO_URI}:${TAG}."
    eval "${DOCKER_COMMAND}" build -t "${ECR_REPO_URI}:${TAG}" -f "${DOCKERFILE}" .
    if ${PUSH_DOCKER_IMAGE}; then
      # Push the Docker image.
      echo "INFO: Pushing Docker image ${ECR_REPO_URI}:${TAG}."
      login
      eval "${DOCKER_COMMAND}" push "${ECR_REPO_URI}:${TAG}"
    fi
    if ${RUN_SCRIPT}; then
      # Run script if requested.
      run_script
    fi
  else
    return 1
  fi
}

function handle_docker_image_exists() {
  if ${PULL_DOCKER_IMAGE}; then
    # Pull the Docker image.
    echo "INFO: Pulling Docker image ${ECR_REPO_URI}:${TAG}."
    login
    eval "${DOCKER_COMMAND}" pull "${ECR_REPO_URI}:${TAG}"
    # Run script if requested.
    ${RUN_SCRIPT} && run_script
  fi
}

function run_script() {
  echo "INFO: Running script ${SCRIPT_PATH} on Docker image."
  # Determine the script's path on the Docker container. We support both relative and absolute paths.
  if [[ "${SCRIPT_PATH}" = /* ]]; then
    CONTAINER_SCRIPT_PATH="${SCRIPT_PATH/${ROOT_DIR}/\/etc\/envoy-serverless}" # Handle absolute path.
  else
    SCRIPT_PATH="$(pwd)/${SCRIPT_PATH}"
    CONTAINER_SCRIPT_PATH="${SCRIPT_PATH/${ROOT_DIR}/\/etc\/envoy-serverless}" # Handle relative path. 
  fi
  if ${INTERACTIVE_MODE}; then
    if reuse_docker_container_if_possible; then
      return 0
    fi
  fi
  EXTRA_MOUNTS=""
  if ${MOUNT_BUILD_DIR}; then
    EXTRA_MOUNTS="${EXTRA_MOUNTS} -v \"${BUILD_DIR}:/build\""
  fi
  if ${MOUNT_ATLASPROXY_DIR}; then
    EXTRA_MOUNTS="${EXTRA_MOUNTS} -v \"${ATLASPROXY_DIR}:/etc/atlasproxy\""
  fi
  if ${MOUNT_SSH_DIR}; then
    EXTRA_MOUNTS="${EXTRA_MOUNTS} -v \"${SSH_DIR}:/root/.ssh\""
  elif [ -n "${SSH_AUTH_SOCK:-}" ]; then
    # If the SSH agent is running and no SSH directory is mounted, forward the agent socket.
    EXTRA_MOUNTS="${EXTRA_MOUNTS} -v \"${SSH_AUTH_SOCK}:/tmp/ssh-agent.socket\" -e \"SSH_AUTH_SOCK=/tmp/ssh-agent.socket\""
  fi
  eval "${DOCKER_COMMAND}" run "$(${INTERACTIVE_MODE} && echo "-it")" "$(${DETACHED_MODE} && \
    echo "-d")" -e "DISTRO=${DISTRO}" "--network=host" -v "${ROOT_DIR}:/etc/envoy-serverless" \
    "${EXTRA_MOUNTS}" "${ECR_REPO_URI}:${TAG}" "${CONTAINER_SCRIPT_PATH}"
  # If running in detached mode, tail the logs of the Docker container to see the script output.
  if ${DETACHED_MODE}; then
    DETACHED_CONTAINER_ID=$(eval "${DOCKER_COMMAND}" ps --format "{{.ID}}")
    eval "${DOCKER_COMMAND}" logs -f "${DETACHED_CONTAINER_ID}" >> "docker-${DETACHED_CONTAINER_ID}.log" 2>&1 &
  fi
}

function reuse_docker_container_if_possible() {
  # Compute the SHA-256 hash of the script.
  SCRIPT_PATH_DIGEST=$(sha256sum "${SCRIPT_PATH}" | cut -d " " -f1)
  # Iterate over all Docker containers using an image with the computed tag. We want to determine
  # whether or not a container already exists that is running the same script. If so, re-use this
  # container.
  for CONTAINER_WITH_TAG in $(eval "${DOCKER_COMMAND}" container ls --all --no-trunc | grep "$(eval \
    "${DOCKER_COMMAND}" image ls --no-trunc | grep "${TAG}" | awk '{print $1":"$2}')" | awk \
    '{print $1":"$3}'); do
    CONTAINER_WITH_TAG_ID=$(echo "${CONTAINER_WITH_TAG}" | cut -d ":" -f1 | tr -d '"')
    CONTAINER_WITH_TAG_SCRIPT_PATH=$(echo "${CONTAINER_WITH_TAG}" | cut -d ":" -f2 | tr -d '"')
    INFERRED_SCRIPT_PATH="${CONTAINER_WITH_TAG_SCRIPT_PATH/\/etc\/envoy-serverless/${ROOT_DIR}}"
    # Verify that the inferred script path is a valid file.
    if [ -f "${INFERRED_SCRIPT_PATH}" ]; then
      # Compute the SHA-256 hash of the inferred script.
      INFERRED_SCRIPT_PATH_DIGEST=$(sha256sum "${INFERRED_SCRIPT_PATH}" | cut -d " " -f1)
      if [ "${SCRIPT_PATH_DIGEST}" = "${INFERRED_SCRIPT_PATH_DIGEST}" ]; then
        # If the script and the inferred script are the same file, start the Docker container and
        # attach to it.
        echo "INFO: Found Docker container ${CONTAINER_WITH_TAG_ID} running script ${SCRIPT_PATH}."
        echo "INFO: Attaching to Docker container ${CONTAINER_WITH_TAG_ID}."
        eval "${DOCKER_COMMAND}" start --attach "${CONTAINER_WITH_TAG_ID}"
        return 0
      fi
    fi
  done
  return 1
}

# Perform option parsing.
OPTIONS=$(getopt -a -l "help,distro:,build-docker-image,push-docker-image,pull-docker-image,run-script:,mount-build-dir:,mount-atlasproxy-dir:,mount-ssh-dir:,interactive,detached,use-podman,print-trace" -o "hd:bplr:v:a:s:itux" -- "$@")
eval set -- "${OPTIONS}"

BUILD_DOCKER_IMAGE=false
PUSH_DOCKER_IMAGE=false
PULL_DOCKER_IMAGE=false
RUN_SCRIPT=false
MOUNT_BUILD_DIR=false
MOUNT_ATLASPROXY_DIR=false
MOUNT_SSH_DIR=false
INTERACTIVE_MODE=false
DETACHED_MODE=false
USE_PODMAN=false
PRINT_TRACE=false

while true; do
  case $1 in
  -h | --help)
    usage
    exit 0
    ;;
  -d | --distro)
    shift
    DISTRO=$1
    ;;
  -b | --build-docker-image)
    BUILD_DOCKER_IMAGE=true
    ;;
  -p | --push-docker-image)
    PUSH_DOCKER_IMAGE=true
    ;;
  -l | --pull-docker-image)
    PULL_DOCKER_IMAGE=true
    ;;
  -r | --run-script)
    shift
    RUN_SCRIPT=true
    SCRIPT_PATH=$1
    ;;
  -v | --mount-build-dir)
    shift
    MOUNT_BUILD_DIR=true
    BUILD_DIR=$1
    ;;
  -a | --mount-atlasproxy-dir)
    shift
    MOUNT_ATLASPROXY_DIR=true
    ATLASPROXY_DIR=$1
    ;;
  -s | --mount-ssh-dir)
    shift
    MOUNT_SSH_DIR=true
    SSH_DIR=$1
    ;;
  -i | --interactive)
    INTERACTIVE_MODE=true
    ;;
  -t | --detached)
    DETACHED_MODE=true
    ;;
  -u | --use-podman)
    USE_PODMAN=true
    ;;
  -x | --print-trace)
    PRINT_TRACE=true
    ;;
  --)
    shift
    break
    ;;
  esac
  shift
done

# Verify that DISTRO exists and is valid.
if [ -z "${DISTRO}" ]; then
  usage
  exit 1
fi

case "${DISTRO}" in
rhel7 | al2 | al2022)
  ;;
*)
  usage
  exit 1
  ;;
esac

# Verify that SCRIPT_PATH exists and is a valid file if requested.
if ${RUN_SCRIPT} && [ ! -f "${SCRIPT_PATH}" ]; then
  usage
  exit 1
fi

# Verify that BUILD_DIR exists and is a valid directory if requested.
if ${MOUNT_BUILD_DIR} && [ ! -d "${BUILD_DIR}" ]; then
  usage
  exit 1
fi

# Verify that ATLASPROXY_DIR exists and is a valid directory if requested.
if ${MOUNT_ATLASPROXY_DIR} && [ ! -d "${ATLASPROXY_DIR}" ]; then
  usage
  exit 1
fi

# Verify that SSH_DIR exists and is a valid directory if requested.
if ${MOUNT_SSH_DIR} && [ ! -d "${SSH_DIR}" ]; then
  usage
  exit 1
fi

# Treat unset variables and parameters as an error after option parsing.
set -u

# Print a trace of simple commands if requested.
${PRINT_TRACE} && set -x

# Ensure that we proactively clean up when the script exits.
trap cleanup EXIT

# Determine the root directory of the serverless proxy Git repository.
ROOT_DIR=$(git rev-parse --show-toplevel)

# Determine whether or not we are using Podman as a replacement for Docker.
DOCKER_COMMAND=$(${USE_PODMAN} && echo "podman" || echo "docker")

# Determine the AWS ECR repository name from DISTRO and the machine's architecture.
if [ "${DISTRO}" = "rhel7" ]; then
  ECR_REPO_NAME="serverless-proxy/centos7"
else
  ECR_REPO_NAME="serverless-proxy/${DISTRO}-$(uname -m)"
fi

# Verify that the prerequisites are installed.
DOCKER_OR_PODMAN_VERSION=$(eval "${DOCKER_COMMAND}" -v)
AWS_CLI_VERSION=$(aws --version)
echo "INFO: Using ${DOCKER_OR_PODMAN_VERSION}."
echo "INFO: Using ${AWS_CLI_VERSION}."

# Compute the SHA-256 hash of the distro's Dockerfile, which will be used as the Docker image tag.
# This ensures that we only build new Docker images when the distro's Dockerfile changes.
DOCKERFILE="${ROOT_DIR}/evergreen/Dockerfile-$([ "${DISTRO}" = "rhel7" ] && echo "centos7" || echo \
  "${DISTRO}")"
TAG=$(sha256sum "${DOCKERFILE}" | cut -d " " -f1)

# Determine the ECR repository URI.
ECR_REPO_URI=$(aws ecr describe-repositories --region us-east-1 --query \
  "repositories[?repositoryName=='${ECR_REPO_NAME}'].repositoryUri | [0]" | tr -d '"')

# Determine whether or not a Docker image already exists with the computed tag. If so, this script
# is a no-op.
EXISTING_IMAGE=$(aws ecr list-images --repository-name "${ECR_REPO_NAME}" --region us-east-1 \
  --query "imageIds[?imageTag=='${TAG}'].imageDigest | [0]")
if [ "${EXISTING_IMAGE}" = "null" ]; then
  echo "INFO: Docker image ${ECR_REPO_URI}:${TAG} doesn't exist."
  handle_docker_image_doesnt_exist
else
  echo "INFO: Docker image ${ECR_REPO_URI}:${TAG} exists."
  handle_docker_image_exists
fi
