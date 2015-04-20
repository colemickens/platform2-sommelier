// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/lib/soma/isolator_parser.h"

#include <set>

#include <base/logging.h>
#include <base/values.h>

#include "soma/lib/soma/container_spec_helpers.h"
#include "soma/lib/soma/device_filter.h"
#include "soma/lib/soma/namespace.h"
#include "soma/proto_bindings/soma_container_spec.pb.h"

namespace soma {
namespace parser {
const char IsolatorParserInterface::kNameKey[] = "name";
const char IsolatorParserInterface::kValueKey[] = "value";

const char IsolatorSetParser::kKey[] = "set";

bool IsolatorSetParser::Parse(const base::DictionaryValue& value,
                              ContainerSpec* spec) {
  DCHECK(spec);
  const base::ListValue* list_value = nullptr;
  if (!value.GetList(kKey, &list_value)) {
    LOG(ERROR) << "Value must be a list, not " << value;
    return false;
  }
  return ParseInternal(*list_value, spec);
}

bool IsolatorObjectParser::Parse(const base::DictionaryValue& value,
                                 ContainerSpec* spec) {
  DCHECK(spec);
  return ParseInternal(value, spec);
}

const char DevicePathFilterParser::kName[] =
    "os/bruteus/device-path-filter-set";

bool DevicePathFilterParser::ParseInternal(const base::ListValue& value,
                                           ContainerSpec* spec) {
  DevicePathFilter::Set device_path_filters;
  if (!DevicePathFilter::ParseList(value, &device_path_filters))
    return false;
  container_spec_helpers::SetDevicePathFilters(device_path_filters, spec);
  return true;
}

const char DeviceNodeFilterParser::kName[] =
    "os/bruteus/device-node-filter-set";

bool DeviceNodeFilterParser::ParseInternal(const base::ListValue& value,
                                           ContainerSpec* spec) {
  DeviceNodeFilter::Set device_node_filters;
  if (!DeviceNodeFilter::ParseList(value, &device_node_filters))
    return false;
  container_spec_helpers::SetDeviceNodeFilters(device_node_filters, spec);
  return true;
}

const char NamespacesParser::kName[] = "os/bruteus/namespaces-share-set";

bool NamespacesParser::ParseInternal(const base::ListValue& value,
                                     ContainerSpec* spec) {
  std::set<ns::Kind> namespaces;
  if (!ns::ParseList(value, &namespaces))
    return false;
  container_spec_helpers::SetNamespaces(namespaces, spec);
  return true;
}

}  // namespace parser
}  // namespace soma
