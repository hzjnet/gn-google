#!/bin/bash -eu

FLATBUFFERS_VERSION="v25.12.19"

if [ ! -d /tmp/flatbuffers ]; then
    git clone https://github.com/google/flatbuffers.git /tmp/flatbuffers
fi

ROOT_DIR="$(realpath "$(dirname "${BASH_SOURCE[0]}")/..")"
OUT_DIR="${ROOT_DIR}/src/third_party/flatbuffers"
SCHEMA_DIR="${ROOT_DIR}/src/gn/binja"

cd /tmp/flatbuffers
git checkout "${FLATBUFFERS_VERSION}"

bazel run //:flatc -- --cpp -o "${SCHEMA_DIR}" "${SCHEMA_DIR}"/project.fbs

rm -rf "${OUT_DIR}"
for f in $(bazel cquery --output=files //:public_headers | cut -f3- -d/); do
  mkdir -p "$(dirname "${OUT_DIR}/$f")"
  cp "include/flatbuffers/$f" "${OUT_DIR}/$f"
done
