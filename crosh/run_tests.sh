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
  # Make sure we can at least parse the file and catch glaringly
  # obvious errors.
  #
  bash -n ${s} || ret=1

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

  #
  # Make sure cmd_* use () for function bodies.
  # People often forget to use `local` in their functions and end up polluting
  # the environment.  Forcing all commands into a subshell prevents that.
  #
  if grep -hn '^cmd_.*() *{' ${s}; then
    cat <<EOF
ERROR: The above commands need to use () for their bodies, not {}:
 cmd_foo() (
   ...
 )

EOF
    ret=1
  fi

  #
  # Check for common style mistakes.
  #
  if grep -hn '^[a-z0-9_]*()[{(]' ${s}; then
    cat <<EOF
ERROR: The above commands need a space after the ()

EOF
    ret=1
  fi

  #
  # Check for common bashisms.  We don't use `checkbashisms` as that script
  # throws too many false positives, and we do actually use some bash.
  #
  if grep -hn '&>' ${s}; then
    cat <<EOF
ERROR: The &> construct is a bashism.  Please fix it like so:
       before:   some_command &> /dev/null
       after :   some_command >/dev/null 2>&1
       Note: Some commands (like grep) have options to silence
             their output.  Use that rather than redirection.

EOF
    ret=1
  fi

  if grep -hn '[[:space:]]\[\[[[:space:]]' ${s}; then
    cat <<EOF
ERROR: The [[...]] construct is a bashism.  Please stick to [...].

EOF
    ret=1
  fi
done

exit ${ret}
