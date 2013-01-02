#!/usr/bin/env python
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import glob
import os
import shutil
import subprocess
import sys

from platform2 import Platform2


class Platform2Test(object):
  _BIND_MOUNT_PATHS = ('dev', 'proc', 'sys')

  def __init__(self, bin, board, host, use_flags, framework, run_as_root,
               gtest_filter):
    self.bin = bin
    self.board = board
    self.host = host
    self.use_flags = use_flags
    self.run_as_root = run_as_root
    self.gtest_filter = gtest_filter

    self.framework = framework
    if self.framework == 'auto':
      self.framework = 'qemu' if self.use('arm') else 'ldso'

    p2 = Platform2(self.use_flags, self.board, self.host)
    self.sysroot = p2.sysroot
    self.qemu_path = os.path.join(p2.get_buildroot(), 'qemu-arm')
    self.lib_dir = os.path.join(p2.get_products_path(), 'lib')

  def remove_sysroot_prefix(self, path):
    """Returns the given path with any sysroot prefix removed."""

    if path.startswith(self.sysroot):
      path = path.replace(self.sysroot, '', 1)

    return path

  def pre_test(self):
    """Runs pre-test environment setup.

    Sets up any required mounts and copying any required files to run tests
    (not those specific to tests) into the sysroot."""

    if not self.use('cros_host'):
      for mount in self._BIND_MOUNT_PATHS:
        path = os.path.join(self.sysroot, mount)
        if not os.path.isdir(path):
          subprocess.check_call(['sudo', 'mkdir', '-p', path])
        subprocess.check_call(['sudo', 'mount', '--bind', '/' + mount, path])

    if self.framework == 'qemu':
      shutil.copy('/usr/bin/qemu-arm', self.qemu_path)

  def post_test(self):
    """Runs post-test teardown, removes mounts/files copied during pre-test."""

    if not self.use('cros_host'):
      for mount in self._BIND_MOUNT_PATHS:
        path = os.path.join(self.sysroot, mount)
        subprocess.check_call(['sudo', 'umount', path])

    if self.framework == 'qemu':
      os.remove(self.qemu_path)

  def use(self, use_flag):
    return use_flag in self.use_flags

  def run(self):
    """Runs the test in a proper environment (e.g. qemu)."""

    cmd = []
    env = {}

    if self.framework == 'qemu':
      self.lib_dir = self.remove_sysroot_prefix(self.lib_dir)
      self.bin = self.remove_sysroot_prefix(self.bin)

    # TODO(lmcloughlin): This code is fundamentally "broken" in the non-QEMU
    # case: it uses the SDK ldso, not the board ldso.
    ld_paths = [self.lib_dir]
    ld_paths += glob.glob(self.sysroot + '/lib*/')
    ld_paths += glob.glob(self.sysroot + '/usr/lib*/')

    if self.framework == 'qemu':
      ld_paths = [self.remove_sysroot_prefix(path) for path in ld_paths]

    env['LD_LIBRARY_PATH'] = ':'.join(ld_paths)

    if self.run_as_root or self.framework == 'qemu':
      cmd.append('sudo')
      for var, val in env.items():
        cmd += ['-E', '%s=%s' % (var, val)]

    if self.framework == 'qemu':
      cmd.append('chroot')
      cmd.append(self.sysroot)
      cmd.append(self.remove_sysroot_prefix(self.qemu_path))
      cmd.append('-drop-ld-preload')

    cmd.append(self.bin)

    if len(self.gtest_filter) > 0:
      cmd.append('--gtest_filter=' + self.gtest_filter)

    try:
      subprocess.check_call(cmd, env=env)
    except subprocess.CalledProcessError:
      raise AssertionError('Error running test binary ' + self.bin)


class _ParseStringSetAction(argparse.Action):
  def __call__(self, parser, namespace, values, option_string=None):
    setattr(namespace, self.dest, set(values.split()))


def main(argv):
  actions = ['pre_test', 'post_test', 'run']

  parser = argparse.ArgumentParser()
  parser.add_argument('--action', required=True,
                      choices=actions, help='action to run')
  parser.add_argument('--bin',
                      help='test binary to run')
  parser.add_argument('--board', required=True,
                      help='board to build for')
  parser.add_argument('--framework', default='auto',
                      choices=('auto', 'ldso', 'qemu'),
                      help='framework to be used to run tests')
  parser.add_argument('--gtest_filter', default='',
                      help='args to pass to gtest/test binary')
  parser.add_argument('--host', action='store_true',
                      help='specify that we\'re testing for the host')
  parser.add_argument('--run_as_root', action='store_true',
                      help='should the test be run as root')
  parser.add_argument('--use_flags', default=set(),
                      action=_ParseStringSetAction,
                      help='USE flags to enable')

  options = parser.parse_args(argv)

  if options.action == 'run' and (not options.bin or len(options.bin) == 0):
    raise AssertionError('You must specify a binary for the "run" action')


  if not (options.host ^ (options.board != None)):
    raise AssertionError('You must provide only one of --board or --host')

  p2test = Platform2Test(options.bin, options.board, options.host,
                         options.use_flags, options.framework,
                         options.run_as_root, options.gtest_filter)
  getattr(p2test, options.action)()


if __name__ == '__main__':
  main(sys.argv[1:])
