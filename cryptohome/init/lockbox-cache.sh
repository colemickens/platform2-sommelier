# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

bootstat lockbox-cache-start
umask 022
mkdir -p -m 0711 $LOCKBOX_CACHE_DIR
# /sbin/mount-encrypted emits the TPM NVRAM contents, if they exist, to a
# file on tmpfs which is used to authenticate the lockbox during cache
# creation.
if [ -O $LOCKBOX_NVRAM_FILE ]; then
  bootstat lockbox-cache-exec
  lockbox-cache --cache=$INSTALL_ATTRS_CACHE \
                --nvram=$LOCKBOX_NVRAM_FILE \
                --lockbox=$INSTALL_ATTRS_FILE
  # There are no other consumers; remove the nvram data
  rm $LOCKBOX_NVRAM_FILE
# For VMs and legacy firmware devices, pretend like lockbox is supported.
elif crossystem "mainfw_type?nonchrome"; then
  cp $INSTALL_ATTRS_FILE $INSTALL_ATTRS_CACHE
fi
bootstat lockbox-cache-end
