#!/usr/bin/python

# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A code generator for TPM 2.0 structures and commands.

The generator takes as input a structures file as emitted by the
extract_structures.sh script and a commands file as emitted by the
extract_commands.sh script.  It outputs valid C++ into tpm_generated.{h,cc}.

The input grammar is documented in the extract_* scripts. Sample input for
structures looks like this:
_BEGIN_TYPES
_OLD_TYPE UINT32
_NEW_TYPE TPM_HANDLE
_END
_BEGIN_CONSTANTS
_CONSTANTS (UINT32) TPM_SPEC
_TYPE UINT32
_NAME TPM_SPEC_FAMILY
_VALUE 0x322E3000
_NAME TPM_SPEC_LEVEL
_VALUE 00
_END
_BEGIN_STRUCTURES
_STRUCTURE TPMS_TIME_INFO
_TYPE UINT64
_NAME time
_TYPE TPMS_CLOCK_INFO
_NAME clockInfo
_END

Sample input for commands looks like this:
_BEGIN
_INPUT_START TPM2_Startup
_TYPE TPMI_ST_COMMAND_TAG
_NAME tag
_COMMENT TPM_ST_NO_SESSIONS
_TYPE UINT32
_NAME commandSize
_TYPE TPM_CC
_NAME commandCode
_COMMENT TPM_CC_Startup {NV}
_TYPE TPM_SU
_NAME startupType
_COMMENT TPM_SU_CLEAR or TPM_SU_STATE
_OUTPUT_START TPM2_Startup
_TYPE TPM_ST
_NAME tag
_COMMENT see clause 8
_TYPE UINT32
_NAME responseSize
_TYPE TPM_RC
_NAME responseCode
_END
"""

import argparse
import re

import union_selectors

_BASIC_TYPES = ['uint8_t', 'int8_t', 'int', 'uint16_t', 'int16_t',
                'uint32_t', 'int32_t', 'uint64_t', 'int64_t']
_OUTPUT_FILE_H = 'tpm_generated.h'
_OUTPUT_FILE_CC = 'tpm_generated.cc'
_COPYRIGHT_HEADER = ('// Copyright (c) 2014 The Chromium OS Authors. All '
                     'rights reserved.\n// Use of this source code is '
                     'governed by a BSD-style license that can be found\n'
                     '// in the LICENSE file.\n\n// THIS CODE IS GENERATED - '
                     'DO NOT MODIFY!\n')
_HEADER_FILE_GUARD_HEADER = """
#ifndef %(name)s
#define %(name)s
"""
_HEADER_FILE_GUARD_FOOTER = """
#endif  // %(name)s
"""
_HEADER_FILE_INCLUDES = """
#include <string>

#include <base/basictypes.h>
#include <base/callback_forward.h>

#include "trunks/authorization_delegate.h"
"""
_IMPLEMENTATION_FILE_INCLUDES = """
#include <string>

#include <base/basictypes.h>
#include <base/bind.h>
#include <base/callback.h>
#include <base/logging.h>
#include <base/sys_byteorder.h>

#include "trunks/authorization_delegate.h"
"""
_LOCAL_INCLUDE = """
#include "%(filename)s"

"""
_NAMESPACE_BEGIN = """
namespace Trunks {
"""
_NAMESPACE_END = """
}  // namespace Trunks
"""
_CLASS_BEGIN = """
class Tpm {
 public:
"""
_CLASS_END = """
};
"""
_SERIALIZE_BASIC_TYPE = """
TPM_RC Serialize_%(type)s(const %(type)s& value, std::string* buffer) {
  VLOG(2) << __func__;
  %(type)s value_net = value;
  switch (sizeof(%(type)s)) {
    case 2:
      value_net = base::HostToNet16(value);
      break;
    case 4:
      value_net = base::HostToNet32(value);
      break;
    case 8:
      value_net = base::HostToNet64(value);
      break;
  }
  const char* value_bytes = reinterpret_cast<const char*>(&value_net);
  buffer->append(value_bytes, sizeof(%(type)s));
  return TPM_RC_SUCCESS;
}

TPM_RC Parse_%(type)s(std::string* buffer, %(type)s* value) {
  VLOG(2) << __func__;
  if (buffer->size() < sizeof(%(type)s))
    return TPM_RC_INSUFFICIENT;
  %(type)s value_net = 0;
  memcpy(&value_net, buffer->data(), sizeof(%(type)s));
  switch (sizeof(%(type)s)) {
    case 2:
      *value = base::NetToHost16(value_net);
      break;
    case 4:
      *value = base::NetToHost32(value_net);
      break;
    case 8:
      *value = base::NetToHost64(value_net);
      break;
    default:
      *value = value_net;
  }
  buffer->erase(0, sizeof(%(type)s));
  return TPM_RC_SUCCESS;
}
"""
_SERIALIZE_DECLARATION = """
TPM_RC Serialize_%(type)s(
    const %(type)s& value,
    std::string* buffer);

TPM_RC Parse_%(type)s(
    std::string* buffer,
    %(type)s* value);
"""


class Typedef(object):
  """Represents a TPM typedef."""

  _TYPEDEF = 'typedef %(old_type)s %(new_type)s;\n'
  _SERIALIZE_FUNCTION = """
TPM_RC Serialize_%(new)s(
    const %(new)s& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_%(old)s(value, buffer);
}
"""
  _PARSE_FUNCTION = """
TPM_RC Parse_%(new)s(
    std::string* buffer,
    %(new)s* value) {
  VLOG(2) << __func__;
  return Parse_%(old)s(buffer, value);
}
"""

  def __init__(self, old_type, new_type):
    self.old_type = old_type
    self.new_type = new_type

  def OutputForward(self, out_file, defined_types, typemap):
    """Outputs a typedef definition, forward declaration does not apply."""
    self.Output(out_file, defined_types, typemap)

  def Output(self, out_file, defined_types, typemap):
    """Writes a typedef definition to |out_file|.

    Any outstanding dependencies will be forward declared.

    Args:
      out_file: The output file.
      defined_types: A set of types for which definitions have already been
        generated.
      typemap: A dict mapping type names to the corresponding object.
    """
    if self.new_type in defined_types:
      return
    # Make sure the dependency is already defined.
    if self.old_type not in defined_types:
      typemap[self.old_type].OutputForward(out_file, defined_types, typemap)
    out_file.write(self._TYPEDEF % {'old_type': self.old_type,
                                    'new_type': self.new_type})
    defined_types.add(self.new_type)

  def OutputSerialize(self, out_file, serialized_types, typemap):
    """Writes a serialize and parse function for the typedef to |out_file|.

    Args:
      out_file: The output file.
      serialized_types: A set of types for which serialize and parse functions
        have already been generated.
      typemap: A dict mapping type names to the corresponding object.
    """
    if self.new_type in serialized_types:
      return
    if self.old_type not in serialized_types:
      typemap[self.old_type].OutputSerialize(out_file, serialized_types,
                                             typemap)
    out_file.write(self._SERIALIZE_FUNCTION % {'old': self.old_type,
                                               'new': self.new_type})
    out_file.write(self._PARSE_FUNCTION % {'old': self.old_type,
                                           'new': self.new_type})
    serialized_types.add(self.new_type)


class Constant(object):
  """Represents a TPM constant."""

  _CONSTANT = 'const %(type)s %(name)s = %(value)s;\n'

  def __init__(self, const_type, name, value):
    self.const_type = const_type
    self.name = name
    self.value = value

  def Output(self, out_file, defined_types, typemap):
    """Writes a constant definition to |out_file|.

    Any outstanding dependencies will be forward declared.

    Args:
      out_file: The output file.
      defined_types: A set of types for which definitions have already been
        generated.
      typemap: A dict mapping type names to the corresponding object.
    """
    # Make sure the dependency is already defined.
    if self.const_type not in defined_types:
      typemap[self.const_type].OutputForward(out_file, defined_types, typemap)
    out_file.write(self._CONSTANT % {'type': self.const_type,
                                     'name': self.name,
                                     'value': self.value})


class Structure(object):
  """Represents a TPM structure or union."""

  _STRUCTURE = 'struct %(name)s {\n'
  _STRUCTURE_FORWARD = 'struct %(name)s;\n'
  _UNION = 'union %(name)s {\n'
  _UNION_FORWARD = 'union %(name)s;\n'
  _STRUCTURE_END = '};\n\n'
  _STRUCTURE_FIELD = '  %(type)s %(name)s;\n'
  _SERIALIZE_FUNCTION_START = """
TPM_RC Serialize_%(type)s(
    const %(type)s& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;
"""
  _SERIALIZE_FIELD = """
  result = Serialize_%(type)s(value.%(name)s, buffer);
  if (result) {
    return result;
  }
"""
  _SERIALIZE_FIELD_ARRAY = """
  for (uint32_t i = 0; i < value.%(count)s; ++i) {
    result = Serialize_%(type)s(value.%(name)s[i], buffer);
    if (result) {
      return result;
    }
  }
"""
  _SERIALIZE_FIELD_WITH_SELECTOR = """
  result = Serialize_%(type)s(
      value.%(name)s,
      value.%(selector_name)s,
      buffer);
  if (result) {
    return result;
  }
"""
  _PARSE_FUNCTION_START = """
TPM_RC Parse_%(type)s(
    std::string* buffer,
    %(type)s* value) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;
"""
  _PARSE_FIELD = """
  result = Parse_%(type)s(buffer, &value->%(name)s);
  if (result) {
    return result;
  }
"""
  _PARSE_FIELD_ARRAY = """
  for (uint32_t i = 0; i < value->%(count)s; ++i) {
    result = Parse_%(type)s(buffer, &value->%(name)s[i]);
    if (result) {
      return result;
    }
  }
"""
  _PARSE_FIELD_WITH_SELECTOR = """
  result = Parse_%(type)s(
      buffer,
      value->%(selector_name)s,
      &value->%(name)s);
  if (result) {
    return result;
  }
"""
  _SERIALIZE_FUNCTION_END = '  return result;\n}\n'
  _ARRAY_FIELD_RE = re.compile(r'(.*)\[(.*)\]')
  _ARRAY_FIELD_SIZE_RE = re.compile(r'^(count|size)')
  _UNION_TYPE_RE = re.compile(r'^TPMU_.*')
  _SERIALIZE_UNION_FUNCTION_START = """
TPM_RC Serialize_%(union_type)s(
    const %(union_type)s& value,
    %(selector_type)s selector,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;
"""
  _SERIALIZE_UNION_FIELD = """
  if (selector == %(selector_value)s) {
    result = Serialize_%(field_type)s(value.%(field_name)s, buffer);
    if (result) {
      return result;
    }
  }
"""
  _SERIALIZE_UNION_FIELD_ARRAY = """
  if (selector == %(selector_value)s) {
    for (uint32_t i = 0; i < %(count)s; ++i) {
      result = Serialize_%(field_type)s(value.%(field_name)s[i], buffer);
      if (result) {
        return result;
      }
    }
  }
"""
  _PARSE_UNION_FUNCTION_START = """
TPM_RC Parse_%(union_type)s(
    std::string* buffer,
    %(selector_type)s selector,
    %(union_type)s* value) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;
"""
  _PARSE_UNION_FIELD = """
  if (selector == %(selector_value)s) {
    result = Parse_%(field_type)s(buffer, &value->%(field_name)s);
    if (result) {
      return result;
    }
  }
"""
  _PARSE_UNION_FIELD_ARRAY = """
  if (selector == %(selector_value)s) {
    for (uint32_t i = 0; i < %(count)s; ++i) {
      result = Parse_%(field_type)s(buffer, &value->%(field_name)s[i]);
      if (result) {
        return result;
      }
    }
  }
"""
  _EMPTY_UNION_CASE = """
  if (selector == %(selector_value)s) {
    // Do nothing.
  }
"""

  def __init__(self, name, is_union):
    self.name = name
    self.is_union = is_union
    self.fields = []
    self.depends_on = []
    self.forwarded = False

  def AddField(self, field_name, field_type):
    """Adds a field for this struct."""
    self.fields.append((field_name, field_type))

  def AddDependency(self, required_type):
    """Adds an explicit dependency on another type.

    This is used in cases where there is an additional dependency other than the
    field types, which are implicit dependencies.  For example, a field like
    FIELD_TYPE value[sizeof(OTHER_TYPE)] would need OTHER_TYPE to be already
    declared.

    Args:
      required_type: The type this structure depends on.
    """
    self.depends_on.append(required_type)

  def _GetFieldTypes(self):
    return set([field[0] for field in self.fields])

  def OutputForward(self, out_file, unused_defined_types, unused_typemap):
    """Writes a structure forward declaration to |out_file|."""
    if self.forwarded:
      return
    if self.is_union:
      out_file.write(self._UNION_FORWARD % {'name': self.name})
    else:
      out_file.write(self._STRUCTURE_FORWARD % {'name': self.name})
    self.forwarded = True

  def Output(self, out_file, defined_types, typemap):
    """Writes a structure definition to |out_file|.

    Any outstanding dependencies will be defined.

    Args:
      out_file: The output file.
      defined_types: A set of types for which definitions have already been
        generated.
      typemap: A dict mapping type names to the corresponding object.
    """
    if self.name in defined_types:
      return
    # Make sure any dependencies are already defined.
    for field_type in self._GetFieldTypes():
      if field_type not in defined_types:
        typemap[field_type].Output(out_file, defined_types, typemap)
    for required_type in self.depends_on:
      if required_type not in defined_types:
        typemap[required_type].Output(out_file, defined_types, typemap)
    if self.is_union:
      out_file.write(self._UNION % {'name': self.name})
    else:
      out_file.write(self._STRUCTURE % {'name': self.name})
    for field in self.fields:
      out_file.write(self._STRUCTURE_FIELD % {'type': field[0],
                                              'name': field[1]})
    out_file.write(self._STRUCTURE_END)
    defined_types.add(self.name)

  def OutputSerialize(self, out_file, serialized_types, typemap):
    """Writes serialize and parse functions for a structure to |out_file|.

    Args:
      out_file: The output file.
      serialized_types: A set of types for which serialize and parse functions
        have already been generated.  This type name of this structure will be
        added on success.
      typemap: A dict mapping type names to the corresponding object.
    """
    if (self.name in serialized_types or
        self.name == 'TPMU_NAME' or
        self.name == 'TPMU_ENCRYPTED_SECRET'):
      return
    # Make sure any dependencies already have serialize functions defined.
    for field_type in self._GetFieldTypes():
      if field_type not in serialized_types:
        typemap[field_type].OutputSerialize(out_file, serialized_types, typemap)
    if self.is_union:
      self._OutputUnionSerialize(out_file)
      serialized_types.add(self.name)
      return
    out_file.write(self._SERIALIZE_FUNCTION_START % {'type': self.name})
    for field in self.fields:
      if self._ARRAY_FIELD_RE.search(field[1]):
        self._OutputArrayField(out_file, field, self._SERIALIZE_FIELD_ARRAY)
      elif self._UNION_TYPE_RE.search(field[0]):
        self._OutputUnionField(out_file, field,
                               self._SERIALIZE_FIELD_WITH_SELECTOR)
      else:
        out_file.write(self._SERIALIZE_FIELD % {'type': field[0],
                                                'name': field[1]})
    out_file.write(self._SERIALIZE_FUNCTION_END)
    out_file.write(self._PARSE_FUNCTION_START % {'type': self.name})
    for field in self.fields:
      if self._ARRAY_FIELD_RE.search(field[1]):
        self._OutputArrayField(out_file, field, self._PARSE_FIELD_ARRAY)
      elif self._UNION_TYPE_RE.search(field[0]):
        self._OutputUnionField(out_file, field, self._PARSE_FIELD_WITH_SELECTOR)
      else:
        out_file.write(self._PARSE_FIELD % {'type': field[0],
                                            'name': field[1]})
    out_file.write(self._SERIALIZE_FUNCTION_END)
    serialized_types.add(self.name)

  def _OutputUnionSerialize(self, out_file):
    """Writes serialize and parse functions for a union to |out_file|.

    This is more complex than the struct case because only one field of the
    union is serialized / parsed based on the value of a selector.  Arrays are
    also handled differently: the full size of the array is serialized instead
    of looking for a field which specifies the count.

    Args:
      out_file: The output file
    """
    selector_type = union_selectors.GetUnionSelectorType(self.name)
    selector_values = union_selectors.GetUnionSelectorValues(self.name)
    field_types = {f[1]: f[0] for f in self.fields}
    out_file.write(self._SERIALIZE_UNION_FUNCTION_START %
                   {'union_type': self.name, 'selector_type': selector_type})
    for selector in selector_values:
      field_name = union_selectors.GetUnionSelectorField(self.name, selector)
      if not field_name:
        out_file.write(self._EMPTY_UNION_CASE % {'selector_value': selector})
        continue
      field_type = field_types[field_name]
      array_match = self._ARRAY_FIELD_RE.search(field_name)
      if array_match:
        field_name = array_match.group(1)
        count = array_match.group(2)
        out_file.write(self._SERIALIZE_UNION_FIELD_ARRAY %
                       {'selector_value': selector,
                        'count': count,
                        'field_type': field_type,
                        'field_name': field_name})
      else:
        out_file.write(self._SERIALIZE_UNION_FIELD %
                       {'selector_value': selector,
                        'field_type': field_type,
                        'field_name': field_name})
    out_file.write(self._SERIALIZE_FUNCTION_END)
    out_file.write(self._PARSE_UNION_FUNCTION_START %
                   {'union_type': self.name, 'selector_type': selector_type})
    for selector in selector_values:
      field_name = union_selectors.GetUnionSelectorField(self.name, selector)
      if not field_name:
        out_file.write(self._EMPTY_UNION_CASE % {'selector_value': selector})
        continue
      field_type = field_types[field_name]
      array_match = self._ARRAY_FIELD_RE.search(field_name)
      if array_match:
        field_name = array_match.group(1)
        count = array_match.group(2)
        out_file.write(self._PARSE_UNION_FIELD_ARRAY %
                       {'selector_value': selector,
                        'count': count,
                        'field_type': field_type,
                        'field_name': field_name})
      else:
        out_file.write(self._PARSE_UNION_FIELD %
                       {'selector_value': selector,
                        'field_type': field_type,
                        'field_name': field_name})
    out_file.write(self._SERIALIZE_FUNCTION_END)

  def _OutputUnionField(self, out_file, field, code_format):
    """Writes serialize / parse code for a union field.

    In this case |self| may not necessarily represent a union but |field| does.
    This requires that a field of an acceptable selector type appear somewhere
    in the struct.  The value of this field is used as the selector value when
    calling the serialize / parse function for the union.

    Args:
      out_file: The output file.
      field: The union field to be processed as a (type, name) tuple.
      code_format: Must be (_SERIALIZE|_PARSE)_FIELD_WITH_SELECTOR
    """
    selector_types = union_selectors.GetUnionSelectorTypes(field[0])
    selector_name = ''
    for tmp in self.fields:
      if tmp[0] in selector_types:
        selector_name = tmp[1]
        break
    assert selector_name, 'Missing selector for %s in %s!' % (field[1],
                                                              self.name)
    out_file.write(code_format % {'type': field[0],
                                  'selector_name': selector_name,
                                  'name': field[1]})

  def _OutputArrayField(self, out_file, field, code_format):
    """Writes serialize / parse code for an array field.

    The allocated size of the array is ignored and a field which holds the
    actual count of items in the array must exist.  Only the number of items
    represented by the value of that count field are serialized / parsed.

    Args:
      out_file: The output file.
      field: The array field to be processed as a (type, name) tuple.
      code_format: Must be (_SERIALIZE|_PARSE)_FIELD_ARRAY
    """
    field_name = self._ARRAY_FIELD_RE.search(field[1]).group(1)
    for count_field in self.fields:
      assert count_field != field, ('Missing count field for %s in %s!' %
                                    (field[1], self.name))
      if self._ARRAY_FIELD_SIZE_RE.search(count_field[1]):
        out_file.write(code_format % {'count': count_field[1],
                                      'type': field[0],
                                      'name': field_name})
        break


class Define(object):
  """Represents a preprocessor define."""

  _DEFINE = '#if !defined(%(name)s)\n#define %(name)s %(value)s\n#endif\n'

  def __init__(self, name, value):
    self.name = name
    self.value = value

  def Output(self, out_file):
    """Writes a preprocessor define to |out_file|."""
    out_file.write(self._DEFINE % {'name': self.name, 'value': self.value})


class StructureParser(object):
  """Structure definition parser.

  The input text file is extracted from the PDF file containing the TPM
  structures specification from the Trusted Computing Group. The syntax
  of the text file is defined by extract_structures.sh.

  - Parses typedefs to a list of Typedef objects.
  - Parses constants to a list of Constant objects.
  - Parses structs and unions to a list of Structure objects.
  - Parses defines to a list of Define objects.

  The parser also creates 'typemap' dict which maps every type to its generator
  object.  This typemap helps manage type dependencies.
  """

  # Compile regular expressions.
  _BEGIN_TYPES_RE = re.compile(r'^_BEGIN_TYPES$')
  _BEGIN_CONSTANTS_RE = re.compile(r'^_BEGIN_CONSTANTS$')
  _BEGIN_STRUCTURES_RE = re.compile(r'^_BEGIN_STRUCTURES$')
  _BEGIN_UNIONS_RE = re.compile(r'^_BEGIN_UNIONS$')
  _BEGIN_DEFINES_RE = re.compile(r'^_BEGIN_DEFINES$')
  _END_RE = re.compile(r'^_END$')
  _OLD_TYPE_RE = re.compile(r'^_OLD_TYPE\s+(\w+)$')
  _NEW_TYPE_RE = re.compile(r'^_NEW_TYPE\s+(\w+)$')
  _CONSTANTS_SECTION_RE = re.compile(r'^_CONSTANTS.* (\w+)$')
  _STRUCTURE_SECTION_RE = re.compile(r'^_STRUCTURE\s+(\w+)$')
  _UNION_SECTION_RE = re.compile(r'^_UNION\s+(\w+)$')
  _TYPE_RE = re.compile(r'^_TYPE\s+(\w+)$')
  _NAME_RE = re.compile(r'^_NAME\s+([a-zA-Z0-9_()\[\]/\*\+\-]+)$')
  _VALUE_RE = re.compile(r'^_VALUE\s+(.+)$')
  _SIZEOF_RE = re.compile(r'^.*sizeof\(([a-zA-Z0-9_]*)\).*$')

  def __init__(self, in_file):
    self._line = None
    self._in_file = in_file

  def _NextLine(self):
    try:
      self._line = self._in_file.next()
    except StopIteration:
      self._line = None

  def Parse(self):
    """Parse everything in a structures file.

    Returns:
      Lists of objects and a type-map as described in the class documentation.
      Returns these in the following order: types, constants, structs, defines,
      typemap.
    """

    self._NextLine()
    types = []
    constants = []
    structs = []
    defines = []
    typemap = {}
    while self._line:
      if self._BEGIN_TYPES_RE.search(self._line):
        types += self._ParseTypes(typemap)
      elif self._BEGIN_CONSTANTS_RE.search(self._line):
        constants += self._ParseConstants(types, typemap)
      elif self._BEGIN_STRUCTURES_RE.search(self._line):
        structs += self._ParseStructures(self._STRUCTURE_SECTION_RE, typemap)
      elif self._BEGIN_UNIONS_RE.search(self._line):
        structs += self._ParseStructures(self._UNION_SECTION_RE, typemap)
      elif self._BEGIN_DEFINES_RE.search(self._line):
        defines += self._ParseDefines()
      else:
        print 'Invalid file format: %s' % self._line
        break
      self._NextLine()
    # Empty structs not handled by the extractor.
    self._AddEmptyStruct('TPMU_SYM_DETAILS', True, structs, typemap)
    # Defines which are used in TPM 2.0 Part 2 but not defined there.
    defines.append(Define(
        'MAX_CAP_DATA', '(MAX_CAP_BUFFER-sizeof(TPM_CAP)-sizeof(UINT32))'))
    defines.append(Define(
        'MAX_CAP_ALGS', '(TPM_ALG_LAST - TPM_ALG_FIRST + 1)'))
    defines.append(Define(
        'MAX_CAP_HANDLES', '(MAX_CAP_DATA/sizeof(TPM_HANDLE))'))
    defines.append(Define(
        'MAX_CAP_CC', '((TPM_CC_LAST - TPM_CC_FIRST) + 1)'))
    defines.append(Define(
        'MAX_TPM_PROPERTIES', '(MAX_CAP_DATA/sizeof(TPMS_TAGGED_PROPERTY))'))
    defines.append(Define(
        'MAX_PCR_PROPERTIES', '(MAX_CAP_DATA/sizeof(TPMS_TAGGED_PCR_SELECT))'))
    defines.append(Define(
        'MAX_ECC_CURVES', '(MAX_CAP_DATA/sizeof(TPM_ECC_CURVE))'))
    defines.append(Define('HASH_COUNT', '3'))
    return types, constants, structs, defines, typemap

  def _AddEmptyStruct(self, name, is_union, structs, typemap):
    """Adds an empty Structure object to |structs| and |typemap|."""
    s = Structure(name, is_union)
    structs.append(s)
    typemap[name] = s
    return

  def _ParseTypes(self, typemap):
    """Parses a typedefs section.

    The current line should be _BEGIN_TYPES and the method will stop parsing
    when an _END line is found.

    Args:
      typemap: A dictionary to which parsed types are added.

    Returns:
      A list of Typedef objects.
    """
    types = []
    self._NextLine()
    while not self._END_RE.search(self._line):
      match = self._OLD_TYPE_RE.search(self._line)
      if not match:
        print 'Invalid old type: %s' % self._line
        return types
      old_type = match.group(1)
      self._NextLine()
      match = self._NEW_TYPE_RE.search(self._line)
      if not match:
        print 'Invalid new type: %s' % self._line
        return types
      new_type = match.group(1)
      t = Typedef(old_type, new_type)
      types.append(t)
      typemap[new_type] = t
      self._NextLine()
    return types

  def _ParseConstants(self, types, typemap):
    """Parses a constants section.

    The current line should be _BEGIN_CONSTANTS and the method will stop parsing
    when an _END line is found.

    Args:
      types: A typedef will be appended for each constant group.
      typemap: A dictionary to which parsed types are added.

    Returns:
      A list of Constant objects.
    """
    constants = []
    self._NextLine()
    while not self._END_RE.search(self._line):
      match = self._CONSTANTS_SECTION_RE.search(self._line)
      if not match:
        print 'Invalid constants section: %s' % self._line
        return constants
      constant_typename = match.group(1)
      self._NextLine()
      match = self._TYPE_RE.search(self._line)
      if not match:
        print 'Invalid constants type: %s' % self._line
        return constants
      constant_type = match.group(1)
      # Create a typedef for the constant group name (e.g. TPM_RC).
      typedef = Typedef(constant_type, constant_typename)
      typemap[constant_typename] = typedef
      types.append(typedef)
      self._NextLine()
      match = self._NAME_RE.search(self._line)
      if not match:
        print 'Invalid constant name: %s' % self._line
        return constants
      while match:
        name = match.group(1)
        self._NextLine()
        match = self._VALUE_RE.search(self._line)
        if not match:
          print 'Invalid constant value: %s' % self._line
          return constants
        value = match.group(1)
        constants.append(Constant(constant_typename, name, value))
        self._NextLine()
        match = self._NAME_RE.search(self._line)
    return constants

  def _ParseStructures(self, section_re, typemap):
    """Parses structures / unions.

    The current line should be _BEGIN_STRUCTURES or _BEGIN_UNIONS and the method
    will stop parsing when an _END line is found.

    Args:
      section_re: The regular expression to use for matching section tokens.
      typemap: A dictionary to which parsed types are added.

    Returns:
      A list of Structure objects.
    """
    structures = []
    is_union = section_re == self._UNION_SECTION_RE
    self._NextLine()
    while not self._END_RE.search(self._line):
      match = section_re.search(self._line)
      if not match:
        print 'Invalid structure section: %s' % self._line
        return structures
      current_structure = Structure(match.group(1), is_union)
      self._NextLine()
      match = self._TYPE_RE.search(self._line)
      if not match:
        print 'Invalid field type: %s' % self._line
        return structures
      while match:
        field_type = match.group(1)
        self._NextLine()
        match = self._NAME_RE.search(self._line)
        if not match:
          print 'Invalid field name: %s' % self._line
          return structures
        field_name = match.group(1)
        # If the field name includes 'sizeof(SOME_TYPE)', record the dependency
        # on SOME_TYPE.
        match = self._SIZEOF_RE.search(field_name)
        if match:
          current_structure.AddDependency(match.group(1))
        # Manually change unfortunate names.
        if field_name == 'xor':
          field_name = 'xor_'
        current_structure.AddField(field_type, field_name)
        self._NextLine()
        match = self._TYPE_RE.search(self._line)
      structures.append(current_structure)
      typemap[current_structure.name] = current_structure
    return structures

  def _ParseDefines(self):
    """Parses preprocessor defines.

    The current line should be _BEGIN_DEFINES and the method will stop parsing
    when an _END line is found.

    Returns:
      A list of Define objects.
    """
    defines = []
    self._NextLine()
    while not self._END_RE.search(self._line):
      match = self._NAME_RE.search(self._line)
      if not match:
        print 'Invalid name: %s' % self._line
        return defines
      name = match.group(1)
      self._NextLine()
      match = self._VALUE_RE.search(self._line)
      if not match:
        print 'Invalid value: %s' % self._line
        return defines
      value = match.group(1)
      defines.append(Define(name, value))
      self._NextLine()
    return defines


class Command(object):
  """Represents a TPM command."""

  _APPENDED_ARGS = """
      AuthorizationDelegate* authorization_delegate,
      const %(method_name)sResponse& callback"""

  def __init__(self, name):
    self.name = name
    self.command_code = ''
    self.request_args = None
    self.response_args = None

  def OutputDeclarations(self, out_file):
    self._OutputCallbackSignature(out_file)
    self._OutputMethodSignature(out_file)

  def _OutputMethodSignature(self, out_file):
    args = self._ArgList(self.request_args)
    if args:
      args += ','
    args += self._APPENDED_ARGS % {'method_name': self._MethodName()}
    out_file.write('  void %s(%s);\n' % (self._MethodName(), args))

  def _OutputCallbackSignature(self, out_file):
    args = self._ArgList(self.response_args)
    if args:
      args = ',' + args
    args = '\n      TPM_RC response_code' + args
    out_file.write('  typedef base::Callback<void(%s)> %sResponse;\n' %
                   (args, self._MethodName()))

  def _MethodName(self):
    if not self.name.startswith('TPM2_'):
      return self.name
    return self.name[5:]

  def _ArgList(self, args):
    if args:
      arg_list = ['%s %s' % (a['type'], a['name']) for a in args]
      return '\n      ' + ',\n      '.join(arg_list)
    return ''


class CommandParser(object):
  """Command definition parser.

  The input text file is extracted from the PDF file containing the TPM
  command specification from the Trusted Computing Group. The syntax
  of the text file is defined by extract_commands.sh.
  """

  # Regular expressions to pull relevant bits from annotated lines.
  _INPUT_START_RE = re.compile(r'^_INPUT_START\s+(\w+)$')
  _OUTPUT_START_RE = re.compile(r'^_OUTPUT_START\s+(\w+)$')
  _TYPE_RE = re.compile(r'^_TYPE\s+(\w+)$')
  _NAME_RE = re.compile(r'^_NAME\s+(\w+)$')
  # Pull the command code from a comment like: _COMMENT TPM_CC_Startup {NV}.
  _COMMENT_CC_RE = re.compile(r'^_COMMENT\s+(TPM_CC_\w+).*$')
  _COMMENT_RE = re.compile(r'^_COMMENT\s+(.*)')
  # Args which are handled internally by the generated method.
  _INTERNAL_ARGS = ('tag', 'Tag', 'commandSize', 'commandCode', 'responseSize',
                    'responseCode')

  def __init__(self, in_file):
    self._line = None
    self._in_file = in_file

  def _NextLine(self):
    try:
      self._line = self._in_file.next()
    except StopIteration:
      self._line = None

  def Parse(self):
    """Parses everything in a commands file.

    Returns:
      A list of extracted Command objects.
    """

    commands = []
    self._NextLine()
    if self._line != '_BEGIN\n':
      print 'Invalid format for first line: %s\n' % self._line
      return commands
    self._NextLine()

    while self._line != '_END\n':
      cmd = self._ParseCommand()
      if not cmd:
        break
      commands.append(cmd)
    return commands

  def _ParseCommand(self):
    """Parses inputs and outputs for a single TPM command."""
    match = self._INPUT_START_RE.search(self._line)
    if not match:
      print 'Cannot match command input from line: %s\n' % self._line
      return None
    name = match.group(1)
    cmd = Command(name)
    self._NextLine()
    cmd.request_args = self._ParseCommandArgs(cmd)
    match = self._OUTPUT_START_RE.search(self._line)
    if not match or match.group(1) != name:
      print 'Cannot match command output from line: %s\n' % self._line
      return None
    self._NextLine()
    cmd.response_args = self._ParseCommandArgs(cmd)
    if not cmd.command_code:
      print 'Command code not found for %s' % name
      return None
    return cmd

  def _ParseCommandArgs(self, cmd):
    """Parses a set of arguments."""
    args = []
    command_code = None
    match = self._TYPE_RE.search(self._line)
    while match:
      arg_type = match.group(1)
      self._NextLine()
      match = self._NAME_RE.search(self._line)
      if not match:
        print 'Cannot match argument name from line: %s\n' % self._line
        break
      arg_name = match.group(1)
      self._NextLine()
      match = self._COMMENT_CC_RE.search(self._line)
      if match:
        cmd.command_code = match.group(1)
      match = self._COMMENT_RE.search(self._line)
      if match:
        description = match.group(1)
        self._NextLine()
      else:
        description = ''
      if arg_name not in self._INTERNAL_ARGS:
        args.append({'type': arg_type,
                     'name': arg_name,
                     'command_code': command_code,
                     'description': description})
      match = self._TYPE_RE.search(self._line)
    return args


def GenerateHeader(types, constants, structs, defines, typemap, commands):
  """Generates a header file with declarations for all given generator objects.

  Args:
    types: A list of Typedef objects.
    constants: A list of Constant objects.
    structs: A list of Structure objects.
    defines: A list of Define objects.
    typemap: A dict mapping type names to the corresponding object.
    commands: A list of Command objects.
  """
  out_file = open(_OUTPUT_FILE_H, 'w')
  out_file.write(_COPYRIGHT_HEADER)
  guard_name = 'TRUNKS_%s_' % _OUTPUT_FILE_H.upper().replace('.', '_')
  out_file.write(_HEADER_FILE_GUARD_HEADER % {'name': guard_name})
  out_file.write(_HEADER_FILE_INCLUDES)
  out_file.write(_NAMESPACE_BEGIN)
  out_file.write('\n')
  # These types are built-in or defined by <stdint.h>; they serve as base cases
  # when defining type dependencies.
  defined_types = set(_BASIC_TYPES)
  # Generate defines.  These must be generated before any other code.
  for define in defines:
    define.Output(out_file)
  out_file.write('\n')
  # Generate typedefs.  These are declared before structs because they are not
  # likely to depend on structs and when they do a simple forward declaration
  # for the struct can be generated.  This improves the readability of the
  # generated code.
  for typedef in types:
    typedef.Output(out_file, defined_types, typemap)
  out_file.write('\n')
  # Generate constant definitions.  Again, generated before structs to improve
  # readability.
  for constant in constants:
    constant.Output(out_file, defined_types, typemap)
  out_file.write('\n')
  # Generate structs.  All non-struct dependencies should be already declared.
  for struct in structs:
    struct.Output(out_file, defined_types, typemap)
  # Generate serialize / parse function declarations.
  for basic_type in _BASIC_TYPES:
    out_file.write(_SERIALIZE_DECLARATION % {'type': basic_type})
  for typedef in types:
    out_file.write(_SERIALIZE_DECLARATION % {'type': typedef.new_type})
  for struct in structs:
    out_file.write(_SERIALIZE_DECLARATION % {'type': struct.name})
  # Generate a declaration for a 'Tpm' class, which includes one method for
  # every TPM 2.0 command.
  out_file.write(_CLASS_BEGIN)
  for command in commands:
    command.OutputDeclarations(out_file)
  out_file.write(_CLASS_END)
  out_file.write(_NAMESPACE_END)
  out_file.write(_HEADER_FILE_GUARD_FOOTER % {'name': guard_name})
  out_file.close()


def GenerateImplementation(types, structs, typemap, commands):
  """Generates implementation code for each command.

  Args:
    types: A list of Typedef objects.
    structs: A list of Structure objects.
    typemap: A dict mapping type names to the corresponding object.
    commands: A list of Command objects.
  """
  out_file = open(_OUTPUT_FILE_CC, 'w')
  out_file.write(_COPYRIGHT_HEADER)
  out_file.write(_LOCAL_INCLUDE % {'filename': _OUTPUT_FILE_H})
  out_file.write(_IMPLEMENTATION_FILE_INCLUDES)
  out_file.write(_NAMESPACE_BEGIN)
  serialized_types = set(_BASIC_TYPES)
  for basic_type in _BASIC_TYPES:
    out_file.write(_SERIALIZE_BASIC_TYPE % {'type': basic_type})
  for typedef in types:
    typedef.OutputSerialize(out_file, serialized_types, typemap)
  for struct in structs:
    struct.OutputSerialize(out_file, serialized_types, typemap)
  # TODO(dkrahn): Generate implementation of the 'Tpm' class.
  _ = commands
  out_file.write(_NAMESPACE_END)
  out_file.close()


def main():
  parser = argparse.ArgumentParser(description='TPM 2.0 code generator')
  parser.add_argument('structures_file')
  parser.add_argument('commands_file')
  args = parser.parse_args()
  structure_parser = StructureParser(open(args.structures_file))
  types, constants, structs, defines, typemap = structure_parser.Parse()
  command_parser = CommandParser(open(args.commands_file))
  commands = command_parser.Parse()
  GenerateHeader(types, constants, structs, defines, typemap, commands)
  GenerateImplementation(types, structs, typemap, commands)
  print 'Processed %d commands.' % len(commands)


if __name__ == '__main__':
  main()
