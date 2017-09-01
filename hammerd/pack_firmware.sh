#!/bin/bash
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is used to generate the firmware tarball. After generating the
# tarball, please upload the tarball by running:
#   $ gsutil cp <tarball>.tbz2 gs://chromeos-localmirror/distfiles/
# And then update chromeos-firmware-<base> ebuild file.

CURRENT_DIR="$(dirname "$(readlink -f "$0")")"
SCRIPT_ROOT="${CURRENT_DIR}/../../scripts"
. "${SCRIPT_ROOT}/common.sh" || exit 1

# The mapping from the board name and the base name. We only support the boards
# listed here.
BOARD_LOOKUP_TABLE="\
poppy hammer
soraka staff"

DEFINE_string board "" "The board name. e.g. poppy" b
DEFINE_string version "" "The version of the target file. e.g. 9794.0.0" v
DEFINE_string channel "dev" \
  "The channel of the target file. One of canary, dev, beta, or stable" c
DEFINE_string signed_key "" \
  "The signed key of the target file. e.g. none, mp or premp" s

FLAGS "$@" || exit 1
eval set -- "${FLAGS_ARGV}"

get_base_name() {
  local board_name="${1}"
  local key
  local value
  echo "${BOARD_LOOKUP_TABLE}" | while read key value; do
    if [[ "${key}" == "${board_name}" ]]; then
      echo "${value}"
    fi
  done
}

init() {
  TMP="$(mktemp -d)"
  echo "Create temp work directory: ${TMP}"
  cd "${TMP}"

  if [[ -z "${FLAGS_board}" ]]; then
    die_notrace "Please specify the board name using -b"
  fi

  base_name="$(get_base_name "${FLAGS_board}")"
  if [[ -z "${base_name}" ]]; then
    die_notrace "The board name is not supported."
  fi
  echo "The base name: ${base_name}"

  if [[ -z "${FLAGS_version}" ]]; then
    die_notrace "Please specify a firmware version using -v"
  fi

  if [[ -z "${FLAGS_channel}" ]]; then
    die_notrace "Please specify a channel using -c"
  fi

  if [[ -n "${FLAGS_signed_key}" ]] &&
    [[ "${FLAGS_signed_key}" != "premp" ]] &&
    [[ "${FLAGS_signed_key}" != "mp" ]]; then
    die_notrace "Please specify a correct key name (e.g. mp, premp) using -s"
  fi
}

cleanup() {
  cd "${CURRENT_DIR}"
  rm -rf "${TMP}"
}

prepare_file_url() {
  local signed_key="${1}"
  local gs_file="ChromeOS-firmware-*.tar.bz2"
  if [[ -n ${signed_key} ]]; then
    gs_file="chromeos_${FLAGS_version}_${base_name}_${FLAGS_signed_key}.bin"
  fi
  echo "gs://chromeos-releases/${FLAGS_channel}-channel/${FLAGS_board}/\
${FLAGS_version}/${gs_file}"
}

process_downloaded_file() {
  EC_PATH="${base_name}/ec.bin"
  local file_name="${1}"
  local modified_name="${base_name}.fw"

  if [[ "${file_name: -4}" == ".bin" ]]; then
    mv "${file_name}" "${modified_name}"
  else
    tar jxf "${file_name}" "${EC_PATH}"
    mv "${EC_PATH}" "${modified_name}"
  fi

  echo "${modified_name}"
}

main() {
  TMP=""
  trap cleanup EXIT
  init

  # Download firmware.
  local gs_url="$(prepare_file_url "${FLAGS_signed_key}")"
  local fw_path="$(gsutil ls "${gs_url}")"
  if [[ -z "${fw_path}" ]]; then
    die "Please ensure your gsutil works and the firmware version is correct."
  fi
  gsutil cp "${fw_path}" .

  # Extract ec firmware image from tar or just rename signed file to target
  # name.
  local downloaded_file_name="$(basename "${fw_path}")"
  local image_name="$(process_downloaded_file "${downloaded_file_name}")"

  # Pack firmware with the version as the file name.
  local fw_version="$(strings "${image_name}" | grep "${base_name}" | head -n1)"
  local output_file="${fw_version}.tbz2"
  tar jcf "${output_file}" "${image_name}"
  mv "${output_file}" "${CURRENT_DIR}"

  echo ""
  echo "Successfully generated the firmware tarball: ${output_file}"
  echo "Please upload the tarball to localmirror by running:"
  echo "  $ gsutil cp ${output_file} gs://chromeos-localmirror/distfiles/"
  echo "And then update chromeos-firmware-${base_name} ebuild file."
}

main "$@"
