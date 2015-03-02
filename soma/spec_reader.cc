// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/spec_reader.h"

#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/json/json_reader.h>
#include <base/memory/scoped_ptr.h>
#include <base/values.h>

#include "soma/container_spec.h"
#include "soma/namespace.h"
#include "soma/port.h"

namespace soma {

const char ContainerSpecReader::kServiceBundlePathKey[] = "service bundle path";
const char ContainerSpecReader::kUidKey[] = "uid";
const char ContainerSpecReader::kGidKey[] = "gid";

ContainerSpecReader::ContainerSpecReader()
    : reader_(base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS) {
}

ContainerSpecReader::~ContainerSpecReader() {
}

scoped_ptr<ContainerSpec> ContainerSpecReader::Read(
    const base::FilePath& spec_file) {
  VLOG(1) << "Reading container spec at " << spec_file.value();
  std::string spec_string;
  if (!base::ReadFileToString(spec_file, &spec_string)) {
    PLOG(ERROR) << "Can't read " << spec_file.value();
    return nullptr;
  }
  return Parse(spec_string);
}

scoped_ptr<ContainerSpec> ContainerSpecReader::Parse(const std::string& json) {
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

  scoped_ptr<ContainerSpec> spec(
      new ContainerSpec(base::FilePath(service_bundle_path), uid, gid));

  base::ListValue* namespaces = nullptr;
  if (spec_dict->GetList(ns::kListKey, &namespaces)) {
    spec->SetNamespaces(ns::ParseList(namespaces));
  }

  base::ListValue* listen_ports = nullptr;
  if (spec_dict->GetList(listen_port::kListKey, &listen_ports)) {
    spec->SetListenPorts(listen_port::ParseList(listen_ports));
  }

  base::ListValue* device_path_filters = nullptr;
  if (spec_dict->GetList(DevicePathFilter::kListKey, &device_path_filters)) {
    spec->SetDevicePathFilters(ParseDevicePathFilters(device_path_filters));
  }

  base::ListValue* device_node_filters = nullptr;
  if (spec_dict->GetList(DeviceNodeFilter::kListKey, &device_node_filters)) {
    spec->SetDeviceNodeFilters(ParseDeviceNodeFilters(device_node_filters));
  }

  return spec.Pass();
}

}  // namespace soma
