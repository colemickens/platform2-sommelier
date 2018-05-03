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
poppy wand
soraka staff
meowth whiskers"

DEFINE_string board "" "The board name. e.g. poppy" b
DEFINE_string version "" "The version of the target file. e.g. 9794.0.0" v
DEFINE_string channel "dev" \
  "The channel of the target file. One of canary, dev, beta, or stable" c
DEFINE_string signed_key "dev" \
  "The signed key of the target file. e.g. dev, premp, premp-v2, mp, mp-v2" s

FLAGS "$@" || exit 1
eval set -- "${FLAGS_ARGV}"

# Global variables that are assigned in init() function.
# The temporary working directory.
TMP=""
# The base URL of the downloaded files.
GS_URL_BASE=""
# The detachable base code name.
BASE_NAME=""

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

  BASE_NAME="$(get_base_name "${FLAGS_board}")"
  if [[ -z "${BASE_NAME}" ]]; then
    die_notrace "The board name is not supported."
  fi
  echo "The base name: ${BASE_NAME}"

  if [[ -z "${FLAGS_version}" ]]; then
    die_notrace "Please specify a firmware version using -v, e.g. 9794.0.0"
  fi

  if [[ -z "${FLAGS_channel}" ]]; then
    die_notrace "Please specify a channel using -c, e.g. canary"
  fi

  if [[ "${FLAGS_signed_key}" != "dev" ]] &&
     [[ "${FLAGS_signed_key}" != premp* ]] &&
     [[ "${FLAGS_signed_key}" != mp* ]]; then
    die_notrace "Please specify a signed key name using -s, e.g. dev, premp, mp"
  fi

  GS_URL_BASE="gs://chromeos-releases/${FLAGS_channel}-channel/${FLAGS_board}/${FLAGS_version}"
  if ! gsutil ls "${GS_URL_BASE}" > /dev/null; then
    die_notrace "${GS_URL_BASE} is not a valid URL. Please check the argument."
  fi
}

cleanup() {
  cd "${CURRENT_DIR}"
  rm -rf "${TMP}"
}

get_ec_file_name() {
  if [[ "${FLAGS_signed_key}" == "dev" ]]; then
    echo "ChromeOS-firmware-*.tar.bz2"
  else
    echo "chromeos_${FLAGS_version}_${BASE_NAME}_${FLAGS_signed_key}.bin"
  fi
}

get_tp_tarball_name() {
  echo "ChromeOS-accessory_rwsig-*-${FLAGS_version}-${FLAGS_board}.tar.bz2"
}

# Download a file that may have wildcard, and return the exact file name.
download_file() {
  local gs_url="${GS_URL_BASE}/${1}"
  local fw_path="$(gsutil ls "${gs_url}")"
  if [[ -z "${fw_path}" ]]; then
    die "Please ensure your gsutil works and the firmware version is correct."
  fi
  gsutil cp "${fw_path}" . > /dev/null

  echo "$(basename "${fw_path}")"
}

process_ec_file() {
  local downloaded_file="$(download_file "$(get_ec_file_name)")"
  local ec_path="${BASE_NAME}/ec.bin"
  local old_file=

  # If the firmware is signed, then the downloaded file is the binary blob,
  # instead of a tarball.
  if [[ "${downloaded_file}" == *.bin ]]; then
    old_file="${downloaded_file}"
  else
    tar xf "${downloaded_file}" "${ec_path}"
    old_file="${ec_path}"
  fi

  # Modify file name to the firmware version.
  local fw_version="$(strings "${old_file}" | grep "${BASE_NAME}" | head -n1)"
  local new_file="${fw_version}.fw"
  mv "${old_file}" "${new_file}"
  echo "${new_file}"
}

process_tp_file() {
  local downloaded_file="$(download_file "$(get_tp_tarball_name)")"

  # Extract the symbolic link first, then extract the target file.
  local sym_file_name="touchpad.bin"
  tar xf "${downloaded_file}" "${BASE_NAME}/${sym_file_name}"
  local real_file_name="$(readlink "${BASE_NAME}/${sym_file_name}")"
  tar xf "${downloaded_file}" "${BASE_NAME}/${real_file_name}"
  mv "${BASE_NAME}/${real_file_name}" "${real_file_name}"

  echo "${real_file_name}"
}

main() {
  TMP=""
  trap cleanup EXIT
  init

  # Download and extract EC firmware and touchpad firmware.
  local ec_file="$(process_ec_file)"
  local tp_file="$(process_tp_file)"
  # Pack EC and touchpad firmware and move to the current directory.
  local output_tar="${BASE_NAME}_${FLAGS_version}_${FLAGS_signed_key}.tbz2"
  tar jcf "${output_tar}" "${ec_file}" "${tp_file}"
  mv "${output_tar}" "${CURRENT_DIR}"

  # Print out the update instruction.
  cat <<EOF
${V_BOLD_GREEN}
Successfully generated the EC and touchpad firmware tarball! ${V_VIDOFF}

Steps to upload the EC firmware tarball:
1. Go to CPFE and Click "Uploads - Private"
2. Select the tarball file at "Select Component File"
3. Select "overlay-${FLAGS_board}-private" overlay
4. Enter "chromeos-base/chromeos-firmware-${BASE_NAME}" in \
"Relative path to file"
5. Update the variables of "chromeos-firmware-${BASE_NAME}" ebuild file.

  FW_TARBALL="${output_tar}"
  EC_FW_NAME="${ec_file}"
  TP_FW_NAME="${tp_file}"
EOF
}

main "$@"
