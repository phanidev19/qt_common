#!/bin/sh
set -e

# Builds and deploys current branch of the current repo to PEL
# (Debug/RelWithDebInfo/Release configurations for multiple compilers or architectures)
#
# Usage: deploy_to_pel.sh <extra_pmideploy_options>
#
# See PEL/tools/deploy_to_pel.sh for more info.

SCRIPT_PATH=$(realpath "$0")
SCRIPT_DIR=$(dirname "$SCRIPT_PATH")
SOURCEDIR=$(cd "$SCRIPT_DIR" && git rev-parse --show-toplevel)
PELDIR=$(realpath "$SOURCEDIR/../pmi_externals_libs_new")

"$SCRIPT_DIR/deploy_to_pel_2015_32.sh" "$@"

# Now we should have above deployment in PEL, get PEL branch name and use it as
# a base branch for another deploy (-b option).
BASE_BRANCH=$(cd "$PELDIR" && git symbolic-ref --short HEAD)

"$SCRIPT_DIR/deploy_to_pel_2015_64.sh" "$@" -b "$BASE_BRANCH" # note: important, -b has to be after $@)
