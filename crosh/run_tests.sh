#!/bin/bash
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Be lazy about locale sorting and such.
export LC_ALL=C

show_diff() {
  local tmp1=$(mktemp)
  local tmp2=$(mktemp)
  echo "$1" > "${tmp1}"
  echo "$2" > "${tmp2}"
  diff -U 1 "${tmp1}" "${tmp2}" | sed -e 1d -e 2d -e 's:^:\t:'
  rm -f "${tmp1}" "${tmp2}"
}

ret=0

for s in crosh*; do
  echo "Checking ${s}"

  #
  # No trailing whitespace.
  #
  egrep -hn '[[:space:]]+$' ${s} && ret=1

  #
  # Keep HELP sorted.
  #
  for help in $(sed -n "/^[A-Z_]*HELP[A-Z_]*='$/s:='::p" "${s}"); do
    cmds=$(sed -n "/^${help}=/,/'$/p" "${s}" | egrep -o '^ [^ ]+')
    sorted=$(echo "${cmds}" | sort)
    if [[ ${cmds} != "${sorted}" ]]; then
      echo "ERROR: ${help} is not sorted:"
      show_diff "${cmds}" "${sorted}"
      ret=1
    fi
  done
done

exit ${ret}
