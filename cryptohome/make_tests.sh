#!/bin/bash
# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Packages up tests for that they can run outside of the build tree.

# Load common constants.  This should be the first executable line.
# The path to common.sh should be relative to your script's location.
COMMON_SH="$(dirname "$0")/../../scripts/common.sh"
. "$COMMON_SH"

mkdir -p ${OUT_DIR}
TARGET=${OUT_DIR}/cryptohome_test

# package_tests target initfile runfile
function package_tests() {
  local package="$1"
  local initfile="$2"
  local testfile="$3"
  shift; shift; shift

  local test_dir="test_image_dir"

  cat <<-EOF > $package
	#!/bin/sh
	# Self extracting archive - reqs bash,test,rm,pwd,tar,gzip,tail,basename
	# Generated from "$0"
	# export PKG_LEAVE_RUNFILES=1 to keep the exploded archive.
	PREV=\`pwd\`
	test \$? -eq 0 || exit 1
	BASE=\`basename \$0\`
	test \$? -eq 0 || exit 1
	export RUNFILES=\`mktemp -d \$PREV/\$BASE.runfiles_XXXXXX\`
	test \$? -eq 0 || exit 1
	# delete the runfiles on exit using a trap
	trap "test \$PKG_LEAVE_RUNFILES || rm -rf \$RUNFILES" EXIT
	# extract starting at the last line (21)
	tail -n +21 \$0 | base64 -d |tar xvf - -C \$RUNFILES
	test \$? -eq 0 || exit 1
	# execute the package but keep the current directory
	/bin/bash --noprofile --norc -c "cd \$RUNFILES;. $initfile $test_dir"
	/bin/bash --noprofile --norc -c "cd \$RUNFILES; ./$testfile" \$0 "\$@"
	exit \$?
	__PACKAGER_TARBALL__
	EOF
  tar cvf - $initfile $testfile |base64 >> $package
  chmod +x $package
}


SCRIPT_DIR=`dirname "$0"`
SCRIPT_DIR=`readlink -f "$SCRIPT_DIR"`

BUILD_ROOT=${BUILD_ROOT:-${SCRIPT_DIR}/../../../src/build}
mkdir -p $OUT_DIR

pushd $SCRIPT_DIR

scons cryptohome_testrunner
package_tests "$TARGET" init_cryptohome_data.sh cryptohome_testrunner

popd
