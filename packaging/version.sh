#!/bin/sh
set -eu

case "${GITHUB_REF_TYPE:-}:${GITHUB_REF_NAME:-}" in
    tag:v*) echo "${GITHUB_REF_NAME#v}" ;;
    *) echo "0.0.0-dev+$(git rev-parse --short=7 HEAD)" ;;
esac
