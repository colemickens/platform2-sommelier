#!/usr/bin/env python2
# -*- coding: utf-8 -*-"
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to build a component artifact."""

from __future__ import print_function

import json
import os
import re
import zipfile

from distutils.version import LooseVersion

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import gs
from chromite.lib import osutils

logger = logging.getLogger(__name__)

COMPONENT_ZIP = 'files.zip'
MANIFEST_FILE_NAME = 'manifest.json'
MANIFEST_VERSION_FIELD = u'version'
MANIFEST_PACKAGE_VERSION_FIELD = u'package_version'

def GetParser():
  parser = commandline.ArgumentParser(description=__doc__)
  # Optional arguments:
  parser.add_argument('--gsbucket', default=None, metavar='GS_BUCKET_URI',
                      help='Override the gsbucket field (Google Cloud Storage '
                           'bucket where component is uploaded to) in config '
                           'file.')
  parser.add_argument('--upload', dest='upload', action='store_true',
                      default=False,
                      help='Upload to Omaha gsbucket.')
  # Required arguments:
  required = parser.add_argument_group('required arguments')
  required.add_argument('--board', metavar='BOARD',
                        help='Board to build the component for.',
                        required=True)
  required.add_argument('--config_path', metavar='CONFIG',
                        help='Path to the config file.', required=True)
  required.add_argument('--platform', metavar='PLATFORM',
                        help='Name for the platform folder in Omaha.',
                        choices=['chromeos_arm32-archive',
                                 'chromeos_intel64-archive',
                                 'chromeos_arm32',
                                 'chromeos_intel64'], required=True)
  # Positional arguments:
  parser.add_argument('component', metavar='COMPONENT',
                      help='The component to build (key inside the config '
                           'file).')
  return parser


def ParseVersion(version_str):
  """Parse version string into a list with components.

  Args:
    version_str: (str) version string.

  Returns:
    [int]: a list with version components.
  """
  pattern = re.compile("[0-9]+(\\.[0-9]+){2,3}")
  m = pattern.match(version_str)
  if m:
    return [int(x) for x in m.group().split('.')]
  return []

def CheckGsBucket(gsbucket):
  """Return list of folders in a gs bucket.

  Args:
    gsbucket: (str) gs bucket url.

  Returns:
    [str]: a list of folder paths.
  """
  ctx = gs.GSContext()
  dirs = ctx.LS(gsbucket)

  return [x for x in dirs if x != gsbucket]


def GetCurrentVersion(paths, platform):
  """Find the current component version by iterating gsbucket root folder.

  Args:
    paths: ([str]) a list of folder paths strings.
    platform: (str) the platform for which the component is being built

  Returns:
    str: current component version.
    str: gs path for current component version.
  """
  current_version = LooseVersion('0.0.0.0')
  current_version_path = None

  for version_path in paths:
    if version_path[-1] != '/':
      logger.fatal("version_path (%s) needs to end with '/'.", version_path)
      continue
    version = os.path.basename(version_path[:-1])
    if len(ParseVersion(version)) < 3:
      # Path does not contain a component version.
      continue

    v = LooseVersion(version)
    if v > current_version:
      # Skip the version if the path for the target platform does not exist.
      ctx = gs.GSContext()
      src = os.path.join(version_path, platform, COMPONENT_ZIP)
      if not ctx.Exists(src):
        continue

      current_version = v
      current_version_path = version_path
  return str(current_version), current_version_path


def DecideVersion(version, current_version):
  """Decide the component version

  Each version has release.major.minor[.bugfix] format.
  If release.major.minor are equal, then use current_version as version and
  increase bugfix by 1 (set to be 1 if bugfix is missing). Otherwise, use
  version (package) as final version and set bugfix 1.

  Args:
    version: (str) package version
    current_version: (str) current component version

  Returns:
    str: next component version.
  """
  version = ParseVersion(version)
  current_version = ParseVersion(current_version)
  if (len(version) != 3 and len(version) != 4) or \
     (len(current_version) != 3 and len(current_version) != 4):
    logger.fatal('version is in wrong format.')
    return None

  if LooseVersion('.'.join([str(x) for x in version[0:3]])) < \
     LooseVersion('.'.join([str(x) for x in current_version[0:3]])):
    logger.fatal('component being built is outdated.')
    return None
  if version[0] == current_version[0] and version[1] == current_version[1] and \
     version[2] == current_version[2]:
    # Rev bug fix on top of current_version.
    version = current_version
    if len(version) < 4:
      version.append(1)
    else:
      version[3] = version[3] + 1
  else:
    # Use package version.1 as next component version.
    if len(version) < 4:
      version.append(1)
    else:
      version[3] = 1
  return '.'.join([str(x) for x in version])


def CheckValidMetadata(metadata):
  """Check if metadata in configuration is valid.

  Args:
    metadata: (str) metadata in component configs.

  Returns:
    bool: if metadata is valid.
  """
  if not "package_name" in metadata or \
      not "files" in metadata or \
      not "gsbucket" in metadata or \
      not "pkgpath" in metadata or \
      not "manifest" in metadata:
    cros_build_lib.Die('attribute is missing.')
    return False
  else:
    return True


def CheckComponentFilesExistence(paths):
  """Check if paths exist.

  Args:
    paths: ([str]) a list of path.

  Returns:
    bool: true if all paths exists.
  """
  for path in paths:
    if not os.path.exists(path):
      cros_build_lib.Die('component file is missing: %s', path)
      return False
    logger.info('File to be included to final component: %s', path)
  return True


def AddDirectoryToZip(zip_file, dir_path):
  """Adds a directory to a zip file.

  This will add the whole directory to the zip, rather than contents of the
  directory. Empty (sub)directories will be ignored.

  Args:
    zip_file: (zipfile.ZipFile) the zip file to which to add the dir.
    dir_path: (string) the directory to add to the zip file.
  """
  # The directories parent path, used to calculate the target paths in the zip
  # file, which should include the |dir_path| itself.
  dir_parent = os.path.normpath(os.path.join(dir_path, os.pardir))

  for current_dir, _subdirs, file_names in os.walk(dir_path):
    for file_name in file_names:
      file_path = os.path.normpath(os.path.join(current_dir, file_name))
      zip_path = os.path.relpath(file_path, dir_parent)
      zip_file.write(file_path, zip_path)


def UploadComponent(component_dir, gsbucket):
  """Upload a component.

  Args:
    component_dir: (str) location for generated component.
    gsbucket: (str) gs bucket to upload.
  """
  logger.info('upload %s to %s', component_dir, gsbucket)
  ctx = gs.GSContext()
  if cros_build_lib.BooleanPrompt(
      prompt='Are you sure you want to upload component to official gs bucket',
      default=False,
      prolog='Once component is uploaded, it can not be modified again.'):
    # Upload component to gs.
    ctx.Copy(component_dir, gsbucket, recursive=True)


def CreateComponent(manifest_path, version, package_name, package_version,
                    platform, files, upload, gsbucket):
  """Create component zip file.

  Args:
    manifest_path: (str) path to raw manifest file.
    version: (str) component version.
    package_name: (str) the package name
    package_version: (str) package version.
    platform: (str) platform folder name on Omaha.
    files: ([str]) paths for component files.
    upload: (bool) whether to upload the generate component to Omaha.
    gsbucket: (str) Omaha gsbucket path.
  """
  if not os.path.exists(manifest_path):
    cros_build_lib.Die('manifest file is missing: %s', manifest_path)
  with open(manifest_path) as f:
    # Construct final manifest file.
    data = json.load(f)
    data[MANIFEST_VERSION_FIELD] = version
    data[MANIFEST_PACKAGE_VERSION_FIELD] = package_version
    # Create final zip file of the component and store it to a temp folder.
    with osutils.TempDir(prefix='component_') as tempdir:
      component_folder = os.path.join(tempdir, data[MANIFEST_VERSION_FIELD],
                                      platform)
      os.makedirs(component_folder)
      component_zipfile = os.path.join(component_folder, COMPONENT_ZIP)
      zf = zipfile.ZipFile(component_zipfile, 'w', zipfile.ZIP_DEFLATED)
      # Move component files into zip file.
      for f in files:
        if os.path.isdir(f):
          AddDirectoryToZip(zf, f)
        else:
          zf.write(f, os.path.basename(f))
      # Write manifest file into zip file.
      zf.writestr(MANIFEST_FILE_NAME, json.dumps(data))
      logger.info('component is generated at %s', zf.filename)
      zf.close()

      # Upload component to gs bucket.
      if upload:
        if '9999' in package_version:
          cros_build_lib.Die('Cannot upload component while the %s package '
                             'is being worked on.', package_name)
        UploadComponent(os.path.join(tempdir, data[MANIFEST_VERSION_FIELD]),
                        gsbucket)

def GetCurrentPackageVersion(current_version_path, platform):
  """Get package version of current component.

  Args:
    current_version_path: (str) path to current version component.
    platform: (str) platform name in omaha.

  Returns:
    str: package version of current component.
  """
  if current_version_path:
    ctx = gs.GSContext()
    src = os.path.join(current_version_path, platform, COMPONENT_ZIP)
    if ctx.Exists(src):
      with osutils.TempDir(prefix='component_') as tempdir:
        ctx.Copy(src, tempdir)
        cros_build_lib.RunCommand(
            ['unzip', '-o', '-d',
             tempdir, os.path.join(tempdir, COMPONENT_ZIP)],
            redirect_stdout=True, redirect_stderr=True)
        with open(os.path.join(tempdir, MANIFEST_FILE_NAME)) as f:
          manifest = json.load(f)
          if MANIFEST_PACKAGE_VERSION_FIELD in manifest:
            return manifest[MANIFEST_PACKAGE_VERSION_FIELD]
  return '0.0.0.0'

def FixPackageVersion(version):
  """Fix version to the format of X.Y.Z-rN

  Package name in ebuild is in the format of (X){1,3}-rN, we convert it
  to X.Y.Z-rN by padding 0 to Z (and Y).
  This function is added because a package like arc++ has version numbers
  (X)-rN which is not consistent with the rest of the packages.

  Args:
    version: (str) version to format.

  Returns:
    str: fixed version.
    Or None: if version is not fixable.
  """
  pattern = re.compile('([0-9]+)(\\.[0-9]+)?(\\.[0-9]+)?(-r[0-9]+)?$')
  m = pattern.match(version)
  if m is None or m.group(1) is None:
    logger.info('version %s is in wrong format.', version)
    return None
  version = m.group(1)
  for i in range(2, 4):
    version = (version + '.0') if m.group(i) is None else (version + m.group(i))
  if m.group(4) is not None:
    version += m.group(4)
  return version

def GetPackageVersion(folder_name, package_name):
  """Get the version of the package.

  It checks if the folder is for the package. If yes, return the version of the
  package.

  Args:
    folder_name: (str) name of the folder.
    package_name: (str) name of the package.

  Returns:
    str: fixed version.
  """
  pattern = re.compile('(^[\\w-]*)-[0-9]+(\\.[0-9]+){0,2}(-r[0-9]+)?$')
  m = pattern.match(folder_name)
  if m is not None and m.group(1) == package_name:
    return FixPackageVersion(folder_name[len(package_name)+1:])
  return None

def BuildComponent(component_to_build, components, board, platform,
                   gsbucket_override=None, upload=False):
  """Build a component.

  Args:
    component_to_build: (str) component to build.
    components: ([object]) a list of components.
    board: (str) board to build the component on.
    platform: (str) platform name in omaha.
    gsbucket_override: (str) gsbucket value to override in components if not
                       None.
    upload: (bool) True if uploading to Omaha; False if not uploading to Omaha.
  """
  for component in components:
    for name, metadata in component.iteritems():
      if name == component_to_build:
        if not CheckValidMetadata(metadata):
          continue
        if (metadata.get('valid_platforms') and
            not platform in metadata['valid_platforms']):
          cros_build_lib.Die('Invalid platform')
        logger.info('build component:%s', name)
        # Check if component files are built successfully.
        files = [os.path.join(cros_build_lib.GetSysroot(), 'build', board, x) \
                 for x in metadata["files"]]
        if not CheckComponentFilesExistence(files):
          cros_build_lib.Die('component files are missing.')

        # Check release versions on gs.
        if gsbucket_override is not None:
          gsbucket = gsbucket_override
        else:
          gsbucket = metadata['gsbucket']
        logger.info('Use %s gsbucket for component.', gsbucket)
        dirs = CheckGsBucket(gsbucket)
        logger.info('Dirs in gsbucket:%s', dirs)
        current_version, current_version_path = GetCurrentVersion(dirs,
                                                                  platform)
        logger.info('latest component version on Omaha gs: %s', current_version)
        # Get package version of current component.
        current_package_version = GetCurrentPackageVersion(current_version_path,
                                                           platform)

        # Check component (gentoo package) version.
        package_name = metadata["package_name"]
        for f in os.listdir(os.path.join(cros_build_lib.GetSysroot(), 'build',
                                         board, metadata["pkgpath"])):
          package_version = GetPackageVersion(f, package_name)
          if package_version is not None:
            logger.info('current package version: %s', package_version)
            logger.info('package version of current component: %s',
                        current_package_version)
            version = DecideVersion(package_version, current_version)
            logger.info('next component version on Omaha gs: %s', version)

            manifest_path = os.path.join(cros_build_lib.GetSysroot(), 'build',
                                         board, metadata["manifest"])

            CreateComponent(manifest_path, version, package_name,
                            package_version, platform, files, upload, gsbucket)
            return
        cros_build_lib.Die('Package could not be found, component could not be'
                           'built.')


def GetComponentsToBuild(path):
  """Parse components from config file.

  Args:
    path: (str) file path to config file.

  Returns:
    Object: a json object of config file content.
  """
  with open(path) as f:
    return json.load(f)


def main(argv):
  opts = GetParser().parse_args(argv)
  BuildComponent(component_to_build=opts.component,
                 components=GetComponentsToBuild(opts.config_path),
                 board=opts.board,
                 platform=opts.platform,
                 gsbucket_override=opts.gsbucket,
                 upload=opts.upload)

if __name__ == '__main__':
  commandline.ScriptWrapperMain(lambda _: main)
