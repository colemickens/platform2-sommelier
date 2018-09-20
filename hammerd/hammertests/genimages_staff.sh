#!/bin/sh
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# gendevimage.sh IMAGE
# Generate dev image from any premp/mp signed image. Appends ".dev" to the version string

set -e

IMAGE=$(readlink -f $1)
KEY=~/trunk/src/platform/ec/board/hammer/dev_key.pem
# Increment to different rollback versions
ROLLBACK0=00000000
ROLLBACK1=01000000
ROLLBACK9=09000000

rm -rf images
mkdir images
cd images

# Use original image for some tests.
cp $IMAGE staff.bin

# Generate dev key set
futility create --desc="Hammer dev key" $KEY key

# Pick up RO and RW version (only take up to 27 bytes, to leave an extra
# 4 bytes for .dev/.rbX tag, and terminating \0.
ro_version_offset=`dump_fmap $IMAGE RO_FRID | sed -n 's/area_offset: *//p'`
ro_version=`dd if=$IMAGE bs=1 skip=$((ro_version_offset)) count=27`
rw_version_offset=`dump_fmap $IMAGE RW_FWID | sed -n 's/area_offset: *//p'`
rw_version=`dd if=$IMAGE bs=1 skip=$((rw_version_offset)) count=27`

# Hack the version string
cp $IMAGE staff.dev
printf "${ro_version}.dev" | dd of=staff.dev bs=1 seek=$((ro_version_offset)) count=32 conv=notrunc
printf "${rw_version}.dev" | dd of=staff.dev bs=1 seek=$((rw_version_offset)) count=32 conv=notrunc

# Resign the image with dev key
echo "Generating image signed with dev keys:"
~/trunk/src/platform/vboot_reference/scripts/image_signing/sign_official_build.sh accessory_rwsig staff.dev .  staff.dev.out
mv staff.dev.out staff.dev

# Show signature
futility show staff.dev

echo "Generating image with rollback = 0:"

printf "Current rollback version: "
rb_offset=`dump_fmap staff.dev RW_RBVER | sed -n 's/area_offset: *//p'`
dd if=staff.dev bs=1 skip=$((rb_offset)) count=4 2>/dev/null | xxd -l 4 -p

cp staff.dev staff.dev.rb0
# Decrement rollback to 0
echo $ROLLBACK0 | xxd -g 4 -p -r | dd of=staff.dev.rb0 bs=1 seek=$((rb_offset)) count=4 conv=notrunc
# Hack the version string
printf "${rw_version}.rb0" | dd of=staff.dev.rb0 bs=1 seek=$((rw_version_offset)) count=32 conv=notrunc
# Resign the image with dev key
~/trunk/src/platform/vboot_reference/scripts/image_signing/sign_official_build.sh accessory_rwsig staff.dev.rb0 . staff.dev.rb0.out
mv staff.dev.rb0.out staff.dev.rb0


echo "Generating image with rollback = 1:"

printf "Current rollback version: "
rb_offset=`dump_fmap staff.dev RW_RBVER | sed -n 's/area_offset: *//p'`
dd if=staff.dev bs=1 skip=$((rb_offset)) count=4 2>/dev/null | xxd -l 4 -p

cp staff.dev staff.dev.rb1
# Increment rollback to 1
echo $ROLLBACK1 | xxd -g 4 -p -r | dd of=staff.dev.rb1 bs=1 seek=$((rb_offset)) count=4 conv=notrunc
# Hack the version string
printf "${rw_version}.rb1" | dd of=staff.dev.rb1 bs=1 seek=$((rw_version_offset)) count=32 conv=notrunc
# Resign the image with dev key
~/trunk/src/platform/vboot_reference/scripts/image_signing/sign_official_build.sh accessory_rwsig staff.dev.rb1 . staff.dev.rb1.out
mv staff.dev.rb1.out staff.dev.rb1

echo "Generating image with rollback = 9:"

printf "Current rollback version: "
rb_offset=`dump_fmap staff.dev RW_RBVER | sed -n 's/area_offset: *//p'`
dd if=staff.dev bs=1 skip=$((rb_offset)) count=4 2>/dev/null | xxd -l 4 -p

cp staff.dev staff.dev.rb9
# Increment rollback to 9
echo $ROLLBACK9 | xxd -g 4 -p -r | dd of=staff.dev.rb9 bs=1 seek=$((rb_offset)) count=4 conv=notrunc
# Hack the version string
printf "${rw_version}.rb9" | dd of=staff.dev.rb9 bs=1 seek=$((rw_version_offset)) count=32 conv=notrunc
# Resign the image with dev key
~/trunk/src/platform/vboot_reference/scripts/image_signing/sign_official_build.sh accessory_rwsig staff.dev.rb9 . staff.dev.rb9.out
mv staff.dev.rb9.out staff.dev.rb9


echo "Generating image with bits corrupted at start of image:"
cp $IMAGE staff_corrupt_first_byte.bin
offset=`dump_fmap staff_corrupt_first_byte.bin EC_RW | sed -n 's/area_offset: *//p'`
dd if=/dev/random of=staff_corrupt_first_byte.bin bs=1 seek=$((offset+100)) count=1 conv=notrunc

echo "Generating image with bits corrupted at end of image:"
cp $IMAGE staff_corrupt_last_byte.bin
offset=`dump_fmap staff_corrupt_last_byte.bin SIG_RW | sed -n 's/area_offset: *//p'`
dd if=/dev/zero of=staff_corrupt_last_byte.bin bs=1 seek=$((offset-100)) count=1 conv=notrunc

# hexdumps are always nice to have to do diffs
for image in staff.bin staff_corrupt_first_byte.bin staff_corrupt_last_byte.bin \
		       staff.dev staff.dev.rb0 staff.dev.rb1 staff.dev.rb9; do
        hexdump -C $image > $image.hex
done

