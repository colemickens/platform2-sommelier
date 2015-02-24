// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/spec_reader.h"

#include <string>
#include <utility>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/json/json_reader.h>
#include <base/memory/scoped_ptr.h>
#include <base/values.h>

#include "soma/container_spec.h"

namespace soma {
namespace {
// Helper function that parses a list of integer pairs.
std::vector<std::pair<int, int>> ParseIntegerPairs(base::ListValue* filters) {
  std::vector<std::pair<int, int>> to_return;
  for (base::Value* filter : *filters) {
    base::ListValue* nested = nullptr;
    if (!(filter->GetAsList(&nested) && nested->GetSize() == 2)) {
      LOG(ERROR) << "Device node filter must be a list of 2 elements.";
      continue;
    }
    int major, minor;
    if (!nested->GetInteger(0, &major) || !nested->GetInteger(1, &minor)) {
      LOG(ERROR) << "Device node filter must contain 2 ints.";
      continue;
    }
    to_return.push_back(std::make_pair(major, minor));
  }
  return to_return;
}
}  // anonymous namespace

const char ContainerSpecReader::kServiceBundlePathKey[] = "service bundle path";
const char ContainerSpecReader::kUidKey[] = "uid";
const char ContainerSpecReader::kGidKey[] = "gid";

const char ContainerSpecReader::kDevicePathFiltersKey[] = "device path filters";
const char ContainerSpecReader::kDeviceNodeFiltersKey[] = "device node filters";

ContainerSpecReader::ContainerSpecReader()
    : reader_(base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS) {
}

ContainerSpecReader::~ContainerSpecReader() {
}

scoped_ptr<ContainerSpec> ContainerSpecReader::Read(
    const base::FilePath& spec_file) {
  LOG(INFO) << "Reading container spec at " << spec_file.value();
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

  base::ListValue* device_path_filters = nullptr;
  std::string temp_filter_string;
  if (spec_dict->GetList(kDevicePathFiltersKey, &device_path_filters)) {
    for (base::Value* filter : *device_path_filters) {
      if (!filter->GetAsString(&temp_filter_string)) {
        LOG(ERROR) << "Device path filters must be strings.";
        continue;
      }
      spec->AddDevicePathFilter(temp_filter_string);
    }
  }

  base::ListValue* device_node_filters = nullptr;
  if (spec_dict->GetList(kDeviceNodeFiltersKey, &device_node_filters)) {
    for (const auto& num_pair : ParseIntegerPairs(device_node_filters)) {
      spec->AddDeviceNodeFilter(num_pair.first, num_pair.second);
    }
  }

  return spec.Pass();
}

}  // namespace soma
