// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/spec_reader.h"

#include <set>
#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/json/json_reader.h>
#include <base/memory/scoped_ptr.h>
#include <base/values.h>

#include "soma/container_spec_wrapper.h"
#include "soma/namespace.h"
#include "soma/port.h"

namespace soma {
namespace parser {

const char ContainerSpecReader::kServiceBundlePathKey[] = "service_bundle_path";
const char ContainerSpecReader::kUidKey[] = "uid";
const char ContainerSpecReader::kGidKey[] = "gid";

ContainerSpecReader::ContainerSpecReader()
    : reader_(base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS) {
}

ContainerSpecReader::~ContainerSpecReader() {
}

scoped_ptr<ContainerSpecWrapper> ContainerSpecReader::Read(
    const base::FilePath& spec_file) {
  VLOG(1) << "Reading container spec at " << spec_file.value();
  std::string spec_string;
  if (!base::ReadFileToString(spec_file, &spec_string)) {
    PLOG(ERROR) << "Can't read " << spec_file.value();
    return nullptr;
  }
  return Parse(spec_string);
}

scoped_ptr<ContainerSpecWrapper> ContainerSpecReader::Parse(
    const std::string& json) {
  scoped_ptr<base::Value> root = make_scoped_ptr(reader_.ReadToValue(json));
  if (!root) {
    LOG(ERROR) << "Failed to parse: " << reader_.GetErrorMessage();
    return nullptr;
  }
  base::DictionaryValue* spec_dict = nullptr;
  if (!root->GetAsDictionary(&spec_dict)) {
    LOG(ERROR) << "Spec should have been a dictionary.";
    return nullptr;
  }

  std::string service_bundle_path;
  if (!spec_dict->GetString(kServiceBundlePathKey, &service_bundle_path)) {
    LOG(ERROR) << "service bundle path is required.";
    return nullptr;
  }

  int uid, gid;
  if (!spec_dict->GetInteger(kUidKey, &uid) ||
      !spec_dict->GetInteger(kGidKey, &gid)) {
    LOG(ERROR) << "uid and gid are required.";
    return nullptr;
  }

  scoped_ptr<ContainerSpecWrapper> spec(
      new ContainerSpecWrapper(base::FilePath(service_bundle_path), uid, gid));

  base::ListValue* namespaces = nullptr;
  if (spec_dict->GetList(ns::kListKey, &namespaces)) {
    spec->SetNamespaces(ns::ParseList(namespaces));
  }

  base::ListValue* listen_ports = nullptr;
  if (spec_dict->GetList(parser::port::kListKey, &listen_ports)) {
    std::set<parser::port::Number> tcp_ports, udp_ports;
    parser::port::ParseList(listen_ports, &tcp_ports, &udp_ports);
    spec->SetTcpListenPorts(tcp_ports);
    spec->SetUdpListenPorts(udp_ports);
  }

  base::ListValue* device_path_filters = nullptr;
  if (spec_dict->GetList(DevicePathFilter::kListKey, &device_path_filters)) {
    spec->SetDevicePathFilters(DevicePathFilterSet::Parse(device_path_filters));
  }

  base::ListValue* device_node_filters = nullptr;
  if (spec_dict->GetList(DeviceNodeFilter::kListKey, &device_node_filters)) {
    spec->SetDeviceNodeFilters(DeviceNodeFilterSet::Parse(device_node_filters));
  }

  return spec.Pass();
}
}  // namespace parser
}  // namespace soma
