#!/bin/sh

# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

# Default product ID in crash report (used if GOOGLE_CRASH_* is undefined).
CHROMEOS_PRODUCT=ChromeOS

# The base directory where we keep various state flags.
CRASH_RUN_STATE_DIR="/run/crash_reporter"

# The directory where crash_reporter stores files.
CRASH_LIB_STATE_DIR="/var/lib/crash_reporter"

# Path to file that indicates a crash test is currently running.
CRASH_TEST_IN_PROGRESS_FILE="${CRASH_RUN_STATE_DIR}/crash-test-in-progress"

# Path to find which is required for computing the crash rate.
FIND="/usr/bin/find"

# Path to hardware class description.
HWCLASS_PATH="/sys/devices/platform/chromeos_acpi/HWID"

# Path to file that indicates this is a developer image.
LEAVE_CORE_FILE="/root/.leave_core"

# Path to list_proxies.
LIST_PROXIES="/usr/bin/list_proxies"

# Path to metrics_client.
METRICS_CLIENT="/usr/bin/metrics_client"

# File whose existence mocks crash sending.  If empty we pretend the
# crash sending was successful, otherwise unsuccessful.
MOCK_CRASH_SENDING="${CRASH_RUN_STATE_DIR}/mock-crash-sending"

# URL to send official build crash reports to.
REPORT_UPLOAD_PROD_URL="https://clients2.google.com/cr/report"

# Path to a directory of restricted certificates which includes
# a certificate for ${REPORT_UPLOAD_PROD_URL}.
RESTRICTED_CERTIFICATES_PATH="/usr/share/chromeos-ca-certificates"

# The syslog tag for all logging we emit.
TAG="$(basename $0)[$$]"

# Temp directory for this process.
TMP_DIR=""

# Chrome's crash report log file.
CHROME_CRASH_LOG="/var/log/chrome/Crash Reports/uploads.log"

lecho() {
  logger -t "${TAG}" "$@"
}

# Returns true if mock is enabled.
is_mock() {
  [ -f "${MOCK_CRASH_SENDING}" ] && return 0
  return 1
}

is_mock_successful() {
  local mock_in=$(cat "${MOCK_CRASH_SENDING}")
  [ "${mock_in}" = "" ] && return 0  # empty file means success
  return 1
}

# Returns 0 if we should consider ourselves to be running on an official image.
# NOTE: NOTE: Mirrors util.cc:IsOfficialImage().
is_official_image() {
  [ ${FORCE_OFFICIAL} -ne 0 ] && return 0
  grep ^CHROMEOS_RELEASE_DESCRIPTION /etc/lsb-release | grep -q Official
}

# Returns 0 if the a crash test is currently running.  NOTE: Mirrors
# util.cc:IsCrashTestInProgress().
is_crash_test_in_progress() {
  [ -f "${CRASH_TEST_IN_PROGRESS_FILE}" ] && return 0
  return 1
}

# Returns 0 if we should consider ourselves to be running on a developer
# image.  NOTE: Mirrors util.cc:IsDeveloperImage().
is_developer_image() {
  # If we're testing crash reporter itself, we don't want to special-case
  # for developer images.
  is_crash_test_in_progress && return 1
  [ -f "${LEAVE_CORE_FILE}" ] && return 0
  return 1
}

# Returns 0 if we should consider ourselves to be running on a test image.
# NOTE: Mirrors util.cc:IsTestImage().
is_test_image() {
  # If we're testing crash reporter itself, we don't want to special-case
  # for test images.
  is_crash_test_in_progress && return 1
  case $(get_channel) in
  test*) return 0;;
  esac
  return 1
}

# Returns 0 if the machine booted up in developer mode.
is_developer_mode() {
  [ ${MOCK_DEVELOPER_MODE} -ne 0 ] && return 0
  # If we're testing crash reporter itself, we don't want to special-case
  # for developer mode.
  is_crash_test_in_progress && return 1
  crossystem "devsw_boot?1"  # exit status will be accurate
}

get_extension() {
  local extension="${1##*.}"
  local filename="${1%.*}"
  # For gzipped file, we ignore .gz and get the real extension
  if [ "${extension}" = "gz" ]; then
    echo "${filename##*.}"
  else
    echo "${extension}"
  fi
}

# Return which kind of report the given metadata file relates to
# NOTE: Similar to crash_sender_util.cc:GetKindFromPayloadPath().
get_kind() {
  local meta_path="$1"
  local payload="$(get_file_path "${meta_path}" "payload")"

  if [ "${payload}" = "undefined" -o ! -r "${payload}" ]; then
    lecho "Missing payload: ${payload}"
    echo "undefined"
    return
  fi
  local kind="$(get_extension "${payload}")"
  if [ "${kind}" = "dmp" ]; then
    echo "minidump"
    return
  fi
  echo "${kind}"
}

get_key_value() {
  local file="$1" key="$2" value

  if [ -f "${file}" ]; then
    # Return the first entry.  There shouldn't be more than one anyways.
    # Substr at length($1) + 2 skips past the key and following = sign (awk
    # uses 1-based indexes), but preserves embedded = characters.
    value=$(sed -n "/^${key}[[:space:]]*=/{s:^[^=]*=::p;q}" "${file}")
  fi

  echo "${value:-undefined}"
}

# Get the full path to the file pointed at by the meta data.
# NOTE: Similar to crash_sender_util.cc:GetRelativePathFromMetadata().
get_file_path() {
  local file="$1"
  local key="$2"
  local value="$(get_key_value "${file}" "${key}")"

  if [ "${value}" = "undefined" ]; then
    echo "undefined"
    return
  fi

  # Paths are relative to the metadata file.
  # TODO: Reject reports w/absolute paths in M72+.
  echo "$(dirname "${file}")/$(basename "${value}")"
}

# Try to find $key in a cached version of $file in this order:
#   1. /var/lib/crash_reporter
#   2. /var/spool/crash
#   3. /etc
# Return "undefined" if $key is not found in any path.
# NOTE: Mirrors util.cc:GetCachedKeyValueDefault().
get_cached_key_value() {
  local file="$1" key="$2"

  local d
  for d in "${CRASH_LIB_STATE_DIR}" "/etc"; do
    local value=$(get_key_value "${d}/${file}" "${key}")
    if [ "${value}" != "undefined" ]; then
      echo "${value}"
      return
    fi
  done

  echo "undefined"
}

# Try to find a value in a cached version of os-release in the same search
# order as get_cached_key_value.
# Try each key in the arguments in order (looking in each file for
# each one), and return "undefined" if no file contains a value for any
# key.
get_os_key_value() {
  # TODO(bmgordon): This should also look in os-release.d.  See
  # libbrillo/brillo/osrelease_reader.cc for an example.
  local key value
  for key in "$@"; do
    value="$(get_cached_key_value "os-release" "${key}")"
    if [ "${value}" != "undefined" ]; then
      echo "${value}"
      return
    fi
  done

  echo "undefined"
}

get_keys() {
  local file="$1" regex="$2"

  awk -F'[[:space:]=]' -vregex="${regex}" \
      'match($1, regex) { print $1 }' "${file}"
}

# Return the board name.
get_board() {
  get_cached_key_value "lsb-release" "CHROMEOS_RELEASE_BOARD"
}

# Return the channel name (sans "-channel" suffix).
get_channel() {
  get_cached_key_value "lsb-release" "CHROMEOS_RELEASE_TRACK" |
      sed 's:-channel$::'
}

# Return the hardware class or "undefined".
get_hardware_class() {
  if [ -r "${HWCLASS_PATH}" ]; then
    cat "${HWCLASS_PATH}"
  elif crossystem hwid > /dev/null 2>&1; then
    echo "$(crossystem hwid)"
  else
    echo "undefined"
  fi
}

send_crash() {
  local meta_path="$1"
  local report_payload="$2"
  local kind="$3"
  local exec_name="$4"

  local url="${REPORT_UPLOAD_PROD_URL}"
  local chromeos_version="$(get_key_value "${meta_path}" "ver")"
  local board="$(get_board)"
  local hwclass="$(get_hardware_class)"
  local write_payload_size="$(get_key_value "${meta_path}" "payload_size")"
  local log="$(get_file_path "${meta_path}" "log")"
  local sig="$(get_key_value "${meta_path}" "sig")"
  local send_payload_size="$(stat --printf=%s "${report_payload}" 2>/dev/null)"
  local product="$(get_key_value "${meta_path}" "upload_var_prod")"
  local version="$(get_key_value "${meta_path}" "upload_var_ver")"
  local upload_prefix="$(get_key_value "${meta_path}" "upload_prefix")"
  local guid

  set -- \
    -F "write_payload_size=${write_payload_size}" \
    -F "send_payload_size=${send_payload_size}"
  if [ "${sig}" != "undefined" ]; then
    set -- "$@" \
      -F "sig=${sig}" \
      -F "sig2=${sig}"
  fi
  set -- "$@" \
    -F "upload_file_${kind}=@${report_payload}"
  if [ "${log}" != "undefined" -a -r "${log}" ]; then
    set -- "$@" \
      -F "log=@${log}"
  fi

  if [ "${upload_prefix}" = "undefined" ]; then
    upload_prefix=""
  fi

  # Grab any variable that begins with upload_.
  local v
  for k in $(get_keys "${meta_path}" "^upload_"); do
    v="$(get_key_value "${meta_path}" "${k}")"
    case ${k} in
      # Product & version are handled separately.
      upload_var_prod) ;;
      upload_var_ver) ;;
      upload_var_*)
        set -- "$@" -F "${upload_prefix}${k#upload_var_}=${v}"
        ;;
      upload_text_*)
        if [ -r "${v}" ]; then
          set -- "$@" -F "${upload_prefix}${k#upload_text_}=<${v}"
        fi
        ;;
      upload_file_*)
        if [ -r "${v}" ]; then
          set -- "$@" -F "${upload_prefix}${k#upload_file_}=@${v}"
        fi
        ;;
    esac
  done

  # When uploading Chrome reports we need to report the right product and
  # version. If the meta file does not specify it we try to examine os-release
  # content. If not available there product gets assigned default product name
  # and version is derived from CHROMEOS_RELEASE_VERSION in /etc/lsb-release.

  if [ "${product}" = "undefined" ]; then
    product=$(get_os_key_value GOOGLE_CRASH_ID ID)
  fi
  if [ "${product}" = "undefined" ]; then
    product="${CHROMEOS_PRODUCT}"
  fi

  if [ "${version}" = "undefined" ]; then
    version="${chromeos_version}"
  fi
  if [ "${version}" = "undefined" ]; then
    version=$(get_os_key_value GOOGLE_CRASH_VERSION_ID BUILD_ID VERSION_ID)
  fi

  local image_type
  if is_test_image; then
    image_type="test"
  elif is_developer_image; then
    image_type="dev"
  elif [ ${FORCE_OFFICIAL} -ne 0 ]; then
    image_type="force-official"
  elif is_mock && ! is_mock_successful; then
    image_type="mock-fail"
  fi

  local boot_mode
  if ! crossystem "cros_debug" > /dev/null 2>&1; then
    # Sanity-check failed that makes sure crossystem exists.
    lecho "Cannot determine boot mode due to error running crossystem command"
    boot_mode="missing-crossystem"
  elif is_developer_mode; then
    boot_mode="dev"
  fi

  # Need to strip dashes ourselves as Chrome preserves it in the file
  # nowadays.  This is also what the Chrome breakpad client does:
  # src/components/crash/core/common/crash_keys.cc:SetMetricsClientIdFromGUID
  # Note: We ignore errors from metrics_client here reading the id.  It was
  # already sanity checked earlier via the consent check.  If it did fail,
  # then we'll end up passing a blank guid below which isn't a big deal.
  guid=$(${METRICS_CLIENT} -i | tr -d '-')

  local error_type="$(get_key_value "${meta_path}" "error_type")"
  [ "${error_type}" = "undefined" ] && error_type=

  lecho "Sending crash:"
  if [ "${product}" != "${CHROMEOS_PRODUCT}" ]; then
    lecho "  Sending crash report on behalf of ${product}"
  fi
  lecho "  Metadata: ${meta_path} (${kind})"
  lecho "  Payload: ${report_payload}"
  lecho "  Version: ${version}"
  [ -n "${image_type}" ] && lecho "  Image type: ${image_type}"
  [ -n "${boot_mode}" ] && lecho "  Boot mode: ${boot_mode}"
  if is_mock; then
    lecho "  Product: ${product}"
    lecho "  URL: ${url}"
    lecho "  Board: ${board}"
    lecho "  HWClass: ${hwclass}"
    lecho "  write_payload_size: ${write_payload_size}"
    lecho "  send_payload_size: ${send_payload_size}"
    if [ "${log}" != "undefined" ]; then
      lecho "  log: @${log}"
    fi
    if [ "${sig}" != "undefined" ]; then
      lecho "  sig: ${sig}"
    fi
  fi
  lecho "  Exec name: ${exec_name}"
  [ -n "${error_type}" ] && lecho "  Error type: ${error_type}"
  if is_mock; then
    if ! is_mock_successful; then
      lecho "Mocking unsuccessful send"
      return 1
    fi
    lecho "Mocking successful send"
    return 0
  fi

  # Read in the first proxy, if any, for a given URL.  NOTE: The
  # double-quotes are necessary due to a bug in dash with the "local"
  # builtin command and values that have spaces in them (see
  # "https://bugs.launchpad.net/ubuntu/+source/dash/+bug/139097").
  if [ -f "${LIST_PROXIES}" ]; then
    local proxy ret
    proxy=$("${LIST_PROXIES}" "${url}")
    ret=$?
    if [ ${ret} -ne 0 ]; then
      proxy=''
      lecho -psyslog.warn \
        "Listing proxies failed with exit code ${ret}"
    else
      proxy=$(echo "${proxy}" | head -1)
    fi
  fi
  # if a direct connection should be used, unset the proxy variable.
  [ "${proxy}" = "direct://" ] && proxy=
  local report_id="${TMP_DIR}/report_id"
  local curl_stderr="${TMP_DIR}/curl_stderr"

  set +e
  curl "${url}" -f -v ${proxy:+--proxy "$proxy"} \
    --capath "${RESTRICTED_CERTIFICATES_PATH}" --ciphers HIGH \
    -F "prod=${product}" \
    -F "ver=${version}" \
    -F "board=${board}" \
    -F "hwclass=${hwclass}" \
    -F "exec_name=${exec_name}" \
    ${image_type:+-F "image_type=${image_type}"} \
    ${boot_mode:+-F "boot_mode=${boot_mode}"} \
    ${error_type:+-F "error_type=${error_type}"} \
    -F "guid=${guid}" \
    -o "${report_id}" \
    "$@" \
    2>"${curl_stderr}"
  curl_result=$?
  set -e

  if [ ${curl_result} -eq 0 ]; then
    local id="$(cat "${report_id}")"
    local product_name
    local timestamp="$(date +%s)"
    case ${product} in
    Chrome_ChromeOS)
      product_name="Chrome"
      ;;
    *)
      product_name=${product}
      ;;
    esac
    if ! is_official_image; then
      product_name=$(echo "${product_name}" | sed 's/Chrome/Chromium/')
    fi
    local silent="$(get_key_value "${meta_path}" "silent")"
    if [ "${IS_CHROMELESS_TTY}" != "true" ] && [ "${silent}" != "true" ]; then
      printf '%s,%s,%s\n' \
        "${timestamp}" "${id}" "${product_name}" >> "${CHROME_CRASH_LOG}"
    fi
    lecho "Crash report receipt ID ${id}"
  else
    lecho "Crash sending failed with exit code ${curl_result}: " \
      "$(cat "${curl_stderr}")"
  fi

  rm -f "${report_id}"

  return ${curl_result}
}

main () {
  if [ $# -ne 5 ]; then
    lecho "Wrong number of command line flags: $*"
    exit 1
  fi
  TMP_DIR="$1"

  shift
  send_crash "$@"
}

main "$@"
