#!/bin/bash
set -ex

SHORT_TAG="cbc-engine-amd64:latest"
REPO="s3.excelsior.internal:5000"
TAG="$REPO/$SHORT_TAG"

podman build --network=host --pull --no-cache -f Dockerfile -t "$TAG"
podman push $TAG
