#!/usr/bin/env python
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper for running platform2 tests.

This handles the fun details like running against the right sysroot, via
qemu, bind mounts, etc...
"""

import argparse
import array
import errno
import os
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

  # The binfmt register format looks like:
  # :name:type:offset:magic:mask:interpreter:flags
  _REGISTER_FORMAT = r':%(name)s:M::%(magic)s:%(mask)s:%(interp)s:%(flags)s'

  # Require enough data to read the Ehdr of the ELF.
  _MIN_ELF_LEN = 64

  # Tuples of (magic, mask) for an arch.  Most only need to identify by the Ehdr
  # fields: e_ident (16 bytes), e_type (2 bytes), e_machine (2 bytes).
  _MAGIC_MASK = {
      'aarch64':
          (r'\x7f\x45\x4c\x46\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00'
           r'\x02\x00\xb7\x00',
           r'\xff\xff\xff\xff\xff\xff\xff\x00\xff\xff\xff\xff\xff\xff\xff\xff'
           r'\xfe\xff\xff\xff'),
      'alpha':
          (r'\x7f\x45\x4c\x46\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00'
           r'\x02\x00\x26\x90',
           r'\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff'
           r'\xfe\xff\xff\xff'),
      'arm':
          (r'\x7f\x45\x4c\x46\x01\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00'
           r'\x02\x00\x28\x00',
           r'\xff\xff\xff\xff\xff\xff\xff\x00\xff\xff\xff\xff\xff\xff\xff\xff'
           r'\xfe\xff\xff\xff'),
      'armeb':
          (r'\x7f\x45\x4c\x46\x01\x02\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00'
           r'\x00\x02\x00\x28',
           r'\xff\xff\xff\xff\xff\xff\xff\x00\xff\xff\xff\xff\xff\xff\xff\xff'
           r'\xff\xfe\xff\xff'),
      'm68k':
          (r'\x7f\x45\x4c\x46\x01\x02\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00'
           r'\x00\x02\x00\x04',
           r'\xff\xff\xff\xff\xff\xff\xff\x00\xff\xff\xff\xff\xff\xff\xff\xff'
           r'\xff\xfe\xff\xff'),
      # For mips targets, we need to scan e_flags.  But first we have to skip:
      # e_version (4 bytes), e_entry/e_phoff/e_shoff (4 or 8 bytes).
      'mips':
          (r'\x7f\x45\x4c\x46\x01\x02\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00'
           r'\x00\x02\x00\x08\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
           r'\x00\x00\x00\x00\x00\x00\x10\x00',
           r'\xff\xff\xff\xff\xff\xff\xff\x00\xff\xff\xff\xff\xff\xff\xff\xff'
           r'\xff\xfe\xff\xff\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
           r'\x00\x00\x00\x00\x00\x00\xf0\x20'),
      'mipsel':
          (r'\x7f\x45\x4c\x46\x01\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00'
           r'\x02\x00\x08\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
           r'\x00\x00\x00\x00\x00\x10\x00\x00',
           r'\xff\xff\xff\xff\xff\xff\xff\x00\xff\xff\xff\xff\xff\xff\xff\xff'
           r'\xfe\xff\xff\xff\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
           r'\x00\x00\x00\x00\x20\xf0\x00\x00'),
      'mipsn32':
          (r'\x7f\x45\x4c\x46\x01\x02\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00'
           r'\x00\x02\x00\x08\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
           r'\x00\x00\x00\x00\x00\x00\x00\x20',
           r'\xff\xff\xff\xff\xff\xff\xff\x00\xff\xff\xff\xff\xff\xff\xff\xff'
           r'\xff\xfe\xff\xff\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
           r'\x00\x00\x00\x00\x00\x00\xf0\x20'),
      'mipsn32el':
          (r'\x7f\x45\x4c\x46\x01\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00'
           r'\x02\x00\x08\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
           r'\x00\x00\x00\x00\x20\x00\x00\x00',
           r'\xff\xff\xff\xff\xff\xff\xff\x00\xff\xff\xff\xff\xff\xff\xff\xff'
           r'\xfe\xff\xff\xff\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
           r'\x00\x00\x00\x00\x20\xf0\x00\x00'),
      'ppc':
          (r'\x7f\x45\x4c\x46\x01\x02\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00'
           r'\x00\x02\x00\x14',
           r'\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff'
           r'\xff\xfe\xff\xff'),
      'sparc':
          (r'\x7f\x45\x4c\x46\x01\x02\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00'
           r'\x00\x02\x00\x12',
           r'\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff'
           r'\xff\xfe\xff\xff'),
      'sparc64':
          (r'\x7f\x45\x4c\x46\x02\x02\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00'
           r'\x00\x02\x00\x2b',
           r'\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff'
           r'\xff\xfe\xff\xff'),
      's390x':
          (r'\x7f\x45\x4c\x46\x02\x02\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00'
           r'\x00\x02\x00\x16',
           r'\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff'
           r'\xff\xfe\xff\xff'),
      'sh4':
          (r'\x7f\x45\x4c\x46\x01\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00'
           r'\x02\x00\x2a\x00',
           r'\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff'
           r'\xfe\xff\xff\xff'),
      'sh4eb':
          (r'\x7f\x45\x4c\x46\x01\x02\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00'
           r'\x00\x02\x00\x2a',
           r'\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff'
           r'\xff\xfe\xff\xff'),
  }

  _BINFMT_PATH = '/proc/sys/fs/binfmt_misc'
  _BINFMT_REGISTER_PATH = os.path.join(_BINFMT_PATH, 'register')

  def __init__(self, sysroot, arch=None):
    if arch is None:
      arch = self.DetectArch(None, sysroot)
    self.arch = arch

    self.sysroot = sysroot

    self.name = 'qemu-%s' % self.arch
    self.build_path = os.path.join('/build', 'bin', self.name)
    self.binfmt_path = os.path.join(self._BINFMT_PATH, self.name)

  @classmethod
  def DetectArch(cls, prog, sysroot):
    """Figure out which qemu wrapper is best for this target"""
    def MaskMatches(bheader, bmagic, bmask):
      """Apply |bmask| to |bheader| and see if it matches |bmagic|

      The |bheader| array may be longer than the |bmask|; in which case we
      will only compare the number of bytes that |bmask| takes up.
      """
      return all((header_byte & mask_byte) == magic_byte
                 for header_byte, magic_byte, mask_byte in
                 zip(bheader[0:len(bmask)], bmagic, bmask))

    if prog is None:
      # Common when doing a global setup.
      prog = '/'

    for path in (prog, '/sbin/ldconfig', '/bin/sh', '/bin/dash', '/bin/bash'):
      path = os.path.join(sysroot, path.lstrip('/'))
      if os.path.islink(path) or not os.path.isfile(path):
        continue

      # Read the header of the ELF first.
      matched_arch = None
      with open(path, 'rb') as f:
        header = f.read(cls._MIN_ELF_LEN)
        if len(header) == cls._MIN_ELF_LEN:
          bheader = array.array('B', header)

          # Walk all the magics and see if any of them match this ELF.
          for arch, magic_mask in cls._MAGIC_MASK.items():
            magic = magic_mask[0].decode('string_escape')
            bmagic = array.array('B', magic)
            mask = magic_mask[1].decode('string_escape')
            bmask = array.array('B', mask)

            if MaskMatches(bheader, bmagic, bmask):
              # Make sure we do not have ambiguous magics as this will
              # also confuse the kernel when it tries to find a match.
              if not matched_arch is None:
                raise ValueError('internal error: multiple masks matched '
                                 '(%s & %s)' % (matched_arch, arch))
              matched_arch = arch

      if not matched_arch is None:
        return matched_arch

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
      try:
        # We need to decode the escape sequences as the kernel has a limit on
        # the register string (255 bytes!).
        osutils.WriteFile(self._BINFMT_REGISTER_PATH,
                          register.decode('string_escape'))
      except IOError:
        print('error: attempted to register: %s' % register)
        raise


class Platform2Test(object):
  """Framework for running platform2 tests"""

  _BIND_MOUNT_PATHS = ('dev', 'dev/pts', 'proc', 'mnt/host/source', 'sys')

  def __init__(self, test_bin, board, host, use_flags, package, framework,
               run_as_root, gtest_filter, user_gtest_filter, cache_dir,
               sysroot):
    self.bin = test_bin
    self.board = board
    self.host = host
    self.package = package
    self.use_flags = use_flags
    self.run_as_root = run_as_root
    (self.gtest_filter, self.user_gtest_filter) = \
        self.generateGtestFilter(gtest_filter, user_gtest_filter)

    p2 = Platform2(self.use_flags, self.board, self.host, cache_dir=cache_dir)
    if sysroot:
      self.sysroot = sysroot
    else:
      self.sysroot = p2.sysroot
    self.lib_dir = os.path.join(p2.get_products_path(), 'lib')

    self.framework = framework
    if self.framework == 'auto':
      qemu_arch = Qemu.DetectArch(self.bin, self.sysroot)
      if qemu_arch is None:
        self.framework = 'ldso'
      else:
        self.framework = 'qemu'

    if self.framework == 'qemu':
      self.qemu = Qemu(self.sysroot, arch=qemu_arch)

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
        osutils.SafeMakedirs(path)
        osutils.Mount('/' + mount, path, 'none', osutils.MS_BIND)

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
      # Some progs want this like bash else they get super confused.
      os.environ['PWD'] = cwd
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
  # Disable the Gentoo sandbox if it's active to avoid warnings/errors.
  if os.environ.get('SANDBOX_ON') == '1':
    os.environ['SANDBOX_ON'] = '0'
    os.execvp(argv[0], argv)
  elif os.geteuid() != 0:
    # Clear the LD_PRELOAD var since it won't be usable w/sudo (and the Gentoo
    # sandbox normally sets it for us).
    os.environ.pop('LD_PRELOAD', None)
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
  parser.add_argument('--board', default=None,
                      help='board to build for')
  parser.add_argument('--sysroot', default=None,
                      help='sysroot to run tests inside')
  parser.add_argument('--cache_dir',
                      default='var/cache/portage/chromeos-base/platform2',
                      help='directory to use as cache for incremental build')
  parser.add_argument('--framework', default='auto',
                      choices=('auto', 'ldso', 'qemu'),
                      help='framework to be used to run tests')
  parser.add_argument('--gtest_filter', default='',
                      help='args to pass to gtest/test binary')
  parser.add_argument('--host', action='store_true', default=False,
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

  if options.host and options.board:
    raise AssertionError('You must provide only one of --board or --host')
  elif not options.host and not options.board and not options.sysroot:
    raise AssertionError('You must provide --board or --host or --sysroot')

  # Once we've finished sanity checking args, make sure we're root.
  _ReExecuteIfNeeded([sys.argv[0]] + argv)

  p2test = Platform2Test(options.bin, options.board, options.host,
                         options.use_flags, options.package, options.framework,
                         options.run_as_root, options.gtest_filter,
                         options.user_gtest_filter, options.cache_dir,
                         options.sysroot)
  getattr(p2test, options.action)()


if __name__ == '__main__':
  main(sys.argv[1:])
