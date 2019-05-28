#!/bin/sh
set -e

# Builds and deploys current branch of the current repo to PEL
# (Debug/RelWithDebInfo/Release configurations)
#
# Usage: deploy_to_pel_2015_64.sh <extra_pmideploy_options>
#
# See PEL/tools/deploy_to_pel.sh for more info.

SCRIPT_PATH=$(realpath "$0")
SCRIPT_DIR=$(dirname "$SCRIPT_PATH")
SOURCEDIR=$(cd "$SCRIPT_DIR" && git rev-parse --show-toplevel)
PELDIR=$(realpath "$SOURCEDIR/../pmi_externals_libs_new")

"$PELDIR/tools/deploy_to_pel.sh" "$SOURCEDIR" -o -Q 2015_64 "$@"
