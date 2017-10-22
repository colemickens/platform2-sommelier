# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# Taken from U-Boot v2017.07 (tools/dtoc)

"""Utility functions for fdt."""

from __future__ import print_function

import os
import struct
import sys
import tempfile

from chromite.lib import cros_build_lib


def fdt32_to_cpu(val):
  """Convert a device tree cell to an integer

  Args:
    val: Value to convert (4-character string representing the cell value)

  Returns:
    A native-endian integer value
  """
  if sys.version_info > (3, 0):
    if isinstance(val, bytes):
      val = val.decode('utf-8')
    val = val.encode('raw_unicode_escape')
  return struct.unpack('>I', val)[0]


def fdt_cells_to_cpu(val, cells):
  """Convert one or two cells to a long integer

  Args:
    val: Value to convert (array of one or more 4-character strings)
    cells: Cell number

  Returns:
    A native-endian long value
  """
  if not cells:
    return 0
  out = long(fdt32_to_cpu(val[0]))
  if cells == 2:
    out = out << 32 | fdt32_to_cpu(val[1])
  return out

def GetInt(node, propname, default=None):
  prop = node.props.get(propname)
  if not prop:
    return default
  value = fdt32_to_cpu(prop.value)
  if type(value) == type(list):
    raise ValueError("Node '%s' property '%s' has list value: expecting"
                     "a single integer" % (node.name, propname))
  return value


def GetString(node, propname, default=None):
  prop = node.props.get(propname)
  if not prop:
    return default
  value = prop.value
  if type(value) == type(list):
    raise ValueError("Node '%s' property '%s' has list value: expecting"
                     "a single string" % (node.name, propname))
  return value


def GetBool(node, propname, default=False):
  if propname in node.props:
    return True
  return default


def CompileDts(dts_input):
  """Compiles a single .dts file

  Args:
    dts_input: Input filename

  Returns:
    Tuple:
      Filename of resulting .dtb file
  """
  dtb_output = tempfile.NamedTemporaryFile(suffix='.dtb', delete=False)
  args = ['-I', 'dts', '-o', dtb_output.name, '-O', 'dtb']
  args.append(dts_input)
  cros_build_lib.RunCommand(['dtc'] + args, quiet=True)
  return dtb_output.name, dtb_output


def EnsureCompiled(fname):
  """Compile an fdt .dts source file into a .dtb binary blob if needed.

  Args:
    fname: Filename (if .dts it will be compiled). It not it will be
      left alone

  Returns:
    Tuple:
      Filename of resulting .dtb file
      tempfile object to unlink after the caller is finished
  """
  out = None
  _, ext = os.path.splitext(fname)
  if ext == '.dtb':
    return fname, None
  elif ext == '.md':
    out = tempfile.NamedTemporaryFile(suffix='.dts', delete=False)
    out.write('/dts-v1/;\n/ {\n')
    with open(fname) as fd:
      adding = False
      for line in fd:
        if line == '```\n':
          adding = not adding
          continue
        if adding:
          out.write(line)
    out.write('};\n')
    out.close()
    dts_input = out.name
  else:
    dts_input = fname
  result = CompileDts(dts_input)
  if out:
    os.unlink(out.name)
  return result


def CompileAll(fnames):
  """Compile a selection of .dtsi files

  This inserts the Chrome OS header and then includes the files one by one to
  ensure that error messages quote the correct file/line number.

  Args:
    fnames: List of .dtsi files to compile
  """
  out = tempfile.NamedTemporaryFile(suffix='.dts', delete=False)
  out.write('/dts-v1/;\n')
  out.write('/ { chromeos { family: family { }; models: models { };')
  out.write('schema { target-dirs { }; }; }; };\n')
  for fname in fnames:
    out.write('/include/ "%s"\n' % fname)
  out.close()
  dts_input = out.name
  result = CompileDts(dts_input)
  if out:
    os.unlink(out.name)
  return result
