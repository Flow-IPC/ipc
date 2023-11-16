#!/bin/sh

# Flow-IPC
# Copyright 2023 Akamai Technologies, Inc.
#
# Licensed under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in
# compliance with the License.  You may obtain a copy
# of the License at
#
#   https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in
# writing, software distributed under the License is
# distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing
# permissions and limitations under the License.

# Author note: I (ygoldfel) personally detest shell scripts; would much rather use a real
# programming language; maybe Perl or Python.  However to avoid adding further dependencies this
# very straightforward script is a shell script.

# Maintenance note: This almost exactly mirrors Flow's similar script used to stage its own
# docs.  TODO: Reuse Flow's which can take an argument, 'flow' or 'ipc', and substitute it in a few
# obvious places below.  It's a simple enough script where we took this copy/pasty shortcut for the time being;
# it's not great but not criminal, I (ygoldfel) think.

# Exit immediately if any command has a non-zero exit status.
set -e

# Ensure(ish) the current directory is `ipc_doc`.
if [ ! -f index.html ] || [ ! -f ../ipc_doc/index.html ]; then
  echo "Please run this from the relative directory doc/ipc_doc/ off the project root." >&2
  exit 1
fi

# Check for the mandatory argument.
if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <the build dir where you invoked CMake, thus generating the build system there>" >&2
  exit 1
fi

BUILD_DIR="$1"

# Ensure specified directories exist, meaning the docs have been generated using `make`.
if [ ! -d "${BUILD_DIR}/html_ipc_doc_public" ] || [ ! -d "${BUILD_DIR}/html_ipc_doc_full" ]; then
  echo "Please run 'make ipc_doc_public ipc_doc_full' from [${BUILD_DIR}] before invoking me." >&2
  exit 1
fi

# Prepare the target directories.
rm -rf ./generated/html_public
rm -rf ./generated/html_full
mkdir -p ./generated

# Move/rename directories.
mv -v "${BUILD_DIR}/html_ipc_doc_public" ./generated/html_public
mv -v "${BUILD_DIR}/html_ipc_doc_full" ./generated/html_full

cd ..
rm -f ipc_doc.tgz
tar czf ipc_doc.tgz ipc_doc

echo 'Peruse the docs by opening ./index.html with browser.'
echo 'If satisfied you may check the resulting ../ipc_doc.tgz into source control.'
