#!/usr/bin/env bash
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generates initial corpus for the cryptolib_rsa_oaep_decrypt_fuzzer.cc fuzzer.
# Execute it if you need to re-generate the corpus, which is normally committed
# to the source tree.

set -e

CORPUS_DIR=cryptolib_rsa_oaep_decrypt_corpus

cd "$(dirname "$0")"

append_big_endian_uint32()
{
  local VALUE=$1
  local OUTPUT_FILE_PATH=$2
  printf "0: %.8x" "${VALUE}" | xxd -r -g0 >> "${OUTPUT_FILE_PATH}"
}

append_ascii_string_with_size()
{
  local STRING=$1
  local OUTPUT_FILE_PATH=$2
  local STRING_LENGTH="${#STRING}"
  append_big_endian_uint32 "${STRING_LENGTH}" "${OUTPUT_FILE_PATH}"
  echo -n "${STRING}" >> "${OUTPUT_FILE_PATH}"
}

append_file_with_size()
{
  local INPUT_FILE_PATH=$1
  local OUTPUT_FILE_PATH=$2
  local INPUT_FILE_SIZE=$(wc -c < "${INPUT_FILE_PATH}")
  append_big_endian_uint32 "${INPUT_FILE_SIZE}" "${OUTPUT_FILE_PATH}"
  cat "${INPUT_FILE_PATH}" >> "${OUTPUT_FILE_PATH}"
}

generate_corpus_entry()
{
  local OUTPUT_FILE_NAME=$1
  local KEY_SIZE_BITS=$2
  local PLAINTEXT=$3
  local OAEP_LABEL=$4

  # Generate an RSA private key
  openssl genrsa -out key.pem "${KEY_SIZE_BITS}"
  openssl rsa -in key.pem -outform der -out key.der

  # Encrypt the plaintext
  local ENCRYPTION_COMMAND="openssl pkeyutl -encrypt -inkey key.pem \
      -pkeyopt rsa_padding_mode:oaep"
  if [[ "${OAEP_LABEL}" ]]; then
    local OAEP_LABEL_HEX=$(echo -n "${OAEP_LABEL}" | od -A n -t x1 | \
        sed "s/ *//g")
    ENCRYPTION_COMMAND="${ENCRYPTION_COMMAND} \
        -pkeyopt rsa_oaep_label:${OAEP_LABEL_HEX}"
  fi
  echo -n "${PLAINTEXT}" | ${ENCRYPTION_COMMAND} > ciphertext.dat

  # Generate the output file
  local OUTPUT_FILE_PATH="${CORPUS_DIR}/${OUTPUT_FILE_NAME}"
  > "${OUTPUT_FILE_PATH}"
  append_file_with_size ciphertext.dat "${OUTPUT_FILE_PATH}"
  append_ascii_string_with_size "${OAEP_LABEL}" "${OUTPUT_FILE_PATH}"
  append_file_with_size key.der "${OUTPUT_FILE_PATH}"
  echo "Successfully generated \"${OUTPUT_FILE_PATH}\""

  # Cleanup
  rm ciphertext.dat
  rm key.pem
  rm key.der
}

for KEY_SIZE_BITS in 512 1024 2048 4096; do
  for PLAINTEXT in "" "foobar"; do
    for OAEP_LABEL in "" "bazlabel"; do
      generate_corpus_entry \
          "valid.key_${KEY_SIZE_BITS}.text_${PLAINTEXT}.label_${OAEP_LABEL}" \
          "${KEY_SIZE_BITS}" "${PLAINTEXT}" "${OAEP_LABEL}"
    done
  done
done
