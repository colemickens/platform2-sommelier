#!/usr/bin/env python
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper for running platform2 tests.

This handles the fun details like running against the right sysroot, via
qemu, bind mounts, etc...
"""

import argparse
import errno
import os
import subprocess
import sys

from platform2 import Platform2

from chromite.lib import cros_build_lib
from chromite.lib import namespaces
from chromite.lib import osutils
from chromite.lib import signals


# Env vars to let through to the test env when we do sudo.
ENV_PASSTHRU = (
)


class Qemu(object):
  """Framework for running tests via qemu"""

  # Map from the Gentoo $ARCH to the right qemu arch.
  ARCH_MAP = {
      'amd64': 'x86_64',
      'arm': 'arm',
      'x86': 'i386',
  }

  # The binfmt register format looks like:
  # :name:type:offset:magic:mask:interpreter:flags
  _REGISTER_FORMAT = r':%(name)s:M::%(magic)s:%(mask)s:%(interp)s:%(flags)s'

  # Tuples of (magic, mask) for an arch.
  _MAGIC_MASK = {
      'arm': (r'\x7fELF\x01\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00'
              r'\x02\x00\x28\x00',
              r'\xff\xff\xff\xff\xff\xff\xff\x00\xff\xff\xff\xff\xff\xff'
              r'\xff\xff\xfe\xff\xff\xff',),
  }

  _BINFMT_PATH = '/proc/sys/fs/binfmt_misc'
  _BINFMT_REGISTER_PATH = os.path.join(_BINFMT_PATH, 'register')

  def __init__(self, qemu_arch=None, gentoo_arch=None, sysroot=None):
    if qemu_arch is None:
      qemu_arch = self.ARCH_MAP[gentoo_arch]
    self.arch = qemu_arch

    self.sysroot = sysroot

    self.name = 'qemu-%s' % self.arch
    self.build_path = os.path.join('/build', 'bin', self.name)
    self.binfmt_path = os.path.join(self._BINFMT_PATH, self.name)

  @staticmethod
  def inode(path):
    """Return the inode for |path| (or -1 if it doesn't exist)"""
    try:
      return os.stat(path).st_ino
    except OSError as e:
      if e.errno == errno.ENOENT:
        return -1
      raise

  def install(self, sysroot=None):
    """Install qemu into |sysroot| safely"""
    if sysroot is None:
      sysroot = self.sysroot

    # Copying strategy:
    # Compare /usr/bin/qemu inode to /build/$board/build/bin/qemu; if
    # different, hard link to a temporary file, then rename temp to target.
    # This should ensure that once $QEMU_SYSROOT_PATH exists it will always
    # exist, regardless of simultaneous test setups.
    paths = (
        ('/usr/bin/%s' % self.name,
         sysroot + self.build_path),
        ('/usr/bin/qemu-binfmt-wrapper',
         sysroot + self.build_path + '-binfmt-wrapper'),
    )

    for src_path, sysroot_path in paths:
      src_path = os.path.normpath(src_path)
      sysroot_path = os.path.normpath(sysroot_path)
      if self.inode(sysroot_path) != self.inode(src_path):
        # Use hardlinks so that the process is atomic.
        temp_path = '%s.%s' % (sysroot_path, os.getpid())
        os.link(src_path, temp_path)
        os.rename(temp_path, sysroot_path)
        # Clear out the temp path in case it exists (another process already
        # swooped in and created the target link for us).
        try:
          os.unlink(temp_path)
        except OSError as e:
          if e.errno != errno.ENOENT:
            raise

  def register_binfmt(self):
    """Make sure qemu has been registered as a format handler

    Prep the binfmt handler. First mount if needed, then unregister any bad
    mappings, and then register our mapping.

    There may still be some race conditions here where one script
    de-registers and another script starts executing before it gets
    re-registered, however it should be rare.
    """
    if not os.path.exists(self._BINFMT_REGISTER_PATH):
      cmd = ['mount', '-n', '-t', 'binfmt_misc', 'binfmt_misc',
             self._BINFMT_PATH]
      cros_build_lib.SudoRunCommand(cmd, error_code_ok=True)

    if os.path.exists(self.binfmt_path):
      interp = 'interpreter %s\n' % self.build_path
      for line in osutils.ReadFile(self.binfmt_path):
        if line == interp:
          break
      else:
        osutils.WriteFile(self.binfmt_path, '-1')

    if not os.path.exists(self.binfmt_path):
      magic, mask = self._MAGIC_MASK[self.arch]
      register = self._REGISTER_FORMAT % {
          'name': self.name,
          'magic': magic,
          'mask': mask,
          'interp': '%s-binfmt-wrapper' % self.build_path,
          'flags': 'POC',
      }
      osutils.WriteFile(self._BINFMT_REGISTER_PATH, register)


class Platform2Test(object):
  """Framework for running platform2 tests"""

  _BIND_MOUNT_PATHS = ('dev', 'dev/pts', 'proc', 'mnt/host/source', 'sys')

  def __init__(self, test_bin, board, host, use_flags, package, framework,
               run_as_root, gtest_filter, user_gtest_filter, cache_dir):
    self.bin = test_bin
    self.board = board
    self.host = host
    self.package = package
    self.use_flags = use_flags
    self.run_as_root = run_as_root
    (self.gtest_filter, self.user_gtest_filter) = \
        self.generateGtestFilter(gtest_filter, user_gtest_filter)

    self.framework = framework
    for k in Qemu.ARCH_MAP.iterkeys():
      if self.use(k):
        gentoo_arch = k
        break
    if self.framework == 'auto':
      if gentoo_arch in ('x86', 'amd64'):
        self.framework = 'ldso'
      else:
        self.framework = 'qemu'

    p2 = Platform2(self.use_flags, self.board, self.host, cache_dir=cache_dir)
    self.sysroot = p2.sysroot
    self.lib_dir = os.path.join(p2.get_products_path(), 'lib')
    if self.framework == 'qemu':
      self.qemu = Qemu(gentoo_arch=gentoo_arch, sysroot=self.sysroot)

  @classmethod
  def generateGtestSubfilter(cls, gtest_filter):
    """Split a gtest_filter down into positive and negative filters.

    Args:
      gtest_filter: A filter string as normally passed to --gtest_filter.

    Returns:
      A tuple of format (positive_filters, negative_filters).
    """

    filters = gtest_filter.split('-', 1)
    positive_filters = [x for x in filters[0].split(':') if x]
    if len(filters) > 1:
      negative_filters = [x for x in filters[1].split(':') if x]
    else:
      negative_filters = []

    return (positive_filters, negative_filters)

  @classmethod
  def generateGtestFilter(cls, filters, user_filters):
    """Merge internal gtest filters and user-supplied gtest filters.

    Returns:
      A string that can be passed to --gtest_filter.
    """

    gtest_filter = cls.generateGtestSubfilter(filters)
    user_gtest_filter = {}

    pkg_filters = dict([x.split('::') for x in user_filters.split()])
    for pkg, pkg_filter in pkg_filters.items():
      user_gtest_filter[pkg] = cls.generateGtestSubfilter(pkg_filter)

    return (gtest_filter, user_gtest_filter)

  def removeSysrootPrefix(self, path):
    """Returns the given path with any sysroot prefix removed."""

    if path.startswith(self.sysroot):
      path = path.replace(self.sysroot, '', 1)

    return path

  def pre_test(self):
    """Runs pre-test environment setup.

    Sets up any required mounts and copying any required files to run tests
    (not those specific to tests) into the sysroot.
    """

    if self.framework == 'qemu':
      self.qemu.install()
      self.qemu.register_binfmt()

  def post_test(self):
    """Runs post-test teardown, removes mounts/files copied during pre-test."""

  def use(self, use_flag):
    return use_flag in self.use_flags

  def run(self):
    """Runs the test in a proper environment (e.g. qemu)."""

    if not self.use('cros_host'):
      for mount in self._BIND_MOUNT_PATHS:
        path = os.path.join(self.sysroot, mount)
        if not os.path.isdir(path):
          subprocess.check_call(['mkdir', '-p', path])
        subprocess.check_call(['mount', '-n', '--bind', '/' + mount, path])

    positive_filters = self.gtest_filter[0]
    negative_filters = self.gtest_filter[1]

    if self.user_gtest_filter:
      if self.package not in self.user_gtest_filter:
        return
      else:
        positive_filters += self.user_gtest_filter[self.package][0]
        negative_filters += self.user_gtest_filter[self.package][1]

    filters = (':'.join(positive_filters), ':'.join(negative_filters))
    gtest_filter = '%s-%s' % filters

    self.bin = self.removeSysrootPrefix(self.bin)
    cmd = [self.bin]
    if gtest_filter != '-':
      cmd.append('--gtest_filter=' + gtest_filter)

    # Some programs expect to find data files via $CWD, so doing a chroot
    # and dropping them into / would make them fail.
    cwd = self.removeSysrootPrefix(os.getcwd())

    # Fork off a child to run the test.  This way we can make tweaks to the
    # env that only affect the child (gid/uid/chroot/cwd/etc...).  We have
    # to fork anyways to run the test, so might as well do it all ourselves
    # to avoid (slow) chaining through programs like:
    #   sudo -u $SUDO_UID -g $SUDO_GID chroot $SYSROOT bash -c 'cd $CWD; $BIN'
    child = os.fork()
    if child == 0:
      print 'chroot: %s' % self.sysroot
      print 'cwd: %s' % cwd
      print 'cmd: %s' % (' '.join(map(repr, cmd)))
      os.chroot(self.sysroot)
      os.chdir(cwd)
      if not self.run_as_root:
        os.setgid(int(os.environ.get('SUDO_GID', '65534')))
        os.setuid(int(os.environ.get('SUDO_UID', '65534')))
      sys.exit(os.execv(cmd[0], cmd))

    _, status = os.waitpid(child, 0)
    if status:
      exit_status, sig = status >> 8, status & 0xff
      raise AssertionError('Error running test binary %s: exit:%i signal:%s(%i)'
                           % (self.bin, exit_status,
                              signals.StrSignal(sig & 0x7f), sig))


def _SudoCommand():
  """Get the 'sudo' command, along with all needed environment variables."""
  cmd = ['sudo']
  for key in ENV_PASSTHRU:
    value = os.environ.get(key)
    if value is not None:
      cmd += ['%s=%s' % (key, value)]
  return cmd


def _ReExecuteIfNeeded(argv):
  """Re-execute tests as root.

  We often need to do things as root, so make sure we're that.  Like chroot
  for proper library environment or do bind mounts.

  Also unshare the mount namespace so as to ensure that doing bind mounts for
  tests don't leak out to the normal chroot.  Also unshare the UTS namespace
  so changes to `hostname` do not impact the host.
  """
  if os.geteuid() != 0:
    cmd = _SudoCommand() + ['--'] + argv
    os.execvp(cmd[0], cmd)
  else:
    namespaces.Unshare(namespaces.CLONE_NEWNS | namespaces.CLONE_NEWUTS)


class _ParseStringSetAction(argparse.Action):
  """Support flags that store into a set (vs a list)."""

  def __call__(self, parser, namespace, values, option_string=None):
    setattr(namespace, self.dest, set(values.split()))


def main(argv):
  actions = ['pre_test', 'post_test', 'run']

  parser = argparse.ArgumentParser()
  parser.add_argument('--action', default='run',
                      choices=actions, help='action to perform')
  parser.add_argument('--bin',
                      help='test binary to run')
  parser.add_argument('--board', required=True,
                      help='board to build for')
  parser.add_argument('--cache_dir',
                      default='var/cache/portage/chromeos-base/platform2',
                      help='directory to use as cache for incremental build')
  parser.add_argument('--framework', default='auto',
                      choices=('auto', 'ldso', 'qemu'),
                      help='framework to be used to run tests')
  parser.add_argument('--gtest_filter', default='',
                      help='args to pass to gtest/test binary')
  parser.add_argument('--host', action='store_true',
                      help='specify that we\'re testing for the host')
  parser.add_argument('--package',
                      help='name of the package we\'re running tests for')
  parser.add_argument('--run_as_root', action='store_true',
                      help='should the test be run as root')
  parser.add_argument('--use_flags', default=set(),
                      action=_ParseStringSetAction,
                      help='USE flags to enable')
  parser.add_argument('--user_gtest_filter', default='',
                      help=argparse.SUPPRESS)

  options = parser.parse_args(argv)

  if options.action == 'run' and (not options.bin or len(options.bin) == 0):
    raise AssertionError('You must specify a binary for the "run" action')

  if options.user_gtest_filter and not options.package:
    raise AssertionError('You must specify a package with user gtest filters')

  if not (options.host ^ (options.board != None)):
    raise AssertionError('You must provide only one of --board or --host')

  # Once we've finished sanity checking args, make sure we're root.
  _ReExecuteIfNeeded([sys.argv[0]] + argv)

  p2test = Platform2Test(options.bin, options.board, options.host,
                         options.use_flags, options.package, options.framework,
                         options.run_as_root, options.gtest_filter,
                         options.user_gtest_filter, options.cache_dir)
  getattr(p2test, options.action)()


if __name__ == '__main__':
  main(sys.argv[1:])
