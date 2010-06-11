#!/bin/bash
# Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
#
# This uses the cryptohome script to initialize an "IMAGE_DIR".  This directory
# will contain the system salt, and three master keys for a user called
# testuser@invalid.domain.
#
# The three keys will have the passwords "zero", "one" and "two".  You can use
# the check_cryptohome_data.sh script to verify that cryptohome can
# successfully decrypt these keys.  The authenticator_unittest.cc testcases
# call this script to create their test data.
#

USERNAME="testuser@invalid.domain"
PASSWORDS="zero one two"

function usage {
  echo "Usage: $0 [-q] <image-dir>"
  echo
  echo "Initialize a directory of sample cryptohome data containing "
  echo "system salt, and a single user named $USERNAME."
  echo "The user will have three master keys, encrypted with the "
  echo "passwords: $PASSWORDS."
  echo
  echo "         -q   Quiet mode"
  echo " <image-dir>  Directory to store cryptohome data"
  echo
  exit 1
}

QUIET=0
IMAGE_DIR=""

while [ ! -z "$1" ]; do
  if [ "$1" == "-q" ]; then
    QUIET=1; shift
  elif [ -z "$IMAGE_DIR" ]; then
    IMAGE_DIR="$1"; shift
  else
    # we only take two arguments, one of which is optional
    usage
  fi
done

if [[ -z "$IMAGE_DIR" || ${IMAGE_DIR:0:1} == "-" ]]; then
  usage
fi

if [ "$QUIET" == "0" ]; then
  info="echo"
else
  function no_echo {
    echo -n
  }

  info="no_echo"
fi

if [ -d "$IMAGE_DIR" ]; then
  $info "Image directory '$IMAGE_DIR' exists.  Remove it if you would like to"
  $info "re-initialize the test data."
  exit 0
fi

$info "Initializing sample crpytohome image root: $IMAGE_DIR"
mkdir -p "$IMAGE_DIR"

$info "Creating system salt."
SYSTEM_SALT_FILE="$IMAGE_DIR/salt"
head -c 16 /dev/urandom > $SYSTEM_SALT_FILE

$info "Creating user directory"

USERID=$(cat "$SYSTEM_SALT_FILE" <(echo -n $USERNAME) \
  | openssl sha1)

$info "USERNAME: $USERNAME"
$info "USERID: $USERID"

mkdir -p "$IMAGE_DIR/skel/sub_path"
echo -n "testfile" > "$IMAGE_DIR/skel/sub_path/.testfile"

mkdir -p "$IMAGE_DIR/$USERID"

$info "Creating master keys..."
INDEX=0
for PASSWORD in $PASSWORDS; do
  $info "PASSWORD: $PASSWORD"

  ASCII_SALT=$(cat "$SYSTEM_SALT_FILE" | xxd -p)

  echo -n "${ASCII_SALT}${PASSWORD}" | sha256sum | head -c 32 \
    > "$IMAGE_DIR/$USERID/pwhash.$INDEX"

  READABLE=$(cat "$IMAGE_DIR/$USERID/pwhash.$INDEX")
  $info "HASHED_PASSWORD: $READABLE"

  openssl rand -rand /dev/urandom \
    -out "$IMAGE_DIR/$USERID/master.$INDEX.salt" 16

  READABLE=$(cat "$IMAGE_DIR/$USERID/master.$INDEX.salt" |xxd -p)
  $info "SALT: $READABLE"

  cat "$IMAGE_DIR/$USERID/pwhash.$INDEX" \
    | cat "$IMAGE_DIR/$USERID/master.$INDEX.salt" - \
    | openssl sha1 > "$IMAGE_DIR/$USERID/pwwrapper.$INDEX"

  READABLE=$(cat "$IMAGE_DIR/$USERID/pwwrapper.$INDEX")
  $info "WRAPPER: $READABLE"

  openssl rand -rand /dev/urandom \
    -out "$IMAGE_DIR/$USERID/rawkey.$INDEX" 160

  echo -n -e 'ch\0001\0001' | cat "$IMAGE_DIR/$USERID/rawkey.$INDEX" - \
    > "$IMAGE_DIR/$USERID/keyvault.$INDEX"

  cat "$IMAGE_DIR/$USERID/pwwrapper.$INDEX" | openssl aes-256-ecb \
    -p \
    -in "$IMAGE_DIR/$USERID/keyvault.$INDEX" \
    -out "$IMAGE_DIR/$USERID/master.$INDEX" \
    -pass fd:0 -md sha1 -e

  rm -f "$IMAGE_DIR/$USERID/pwhash.$INDEX"
  rm -f "$IMAGE_DIR/$USERID/pwwrapper.$INDEX"
  rm -f "$IMAGE_DIR/$USERID/rawkey.$INDEX"
  rm -f "$IMAGE_DIR/$USERID/keyvault.$INDEX"

  EXIT=$?
  if [ $EXIT != 0 ]; then
    exit $EXIT
  fi

  READABLE=$(cat "$IMAGE_DIR/$USERID/master.$INDEX" |xxd -p)
  $info "MASTER_KEY: $READABLE"

  INDEX=$(($INDEX + 1))
done
