// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/spec_reader.h"

#include <sys/types.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/json/json_reader.h>
#include <base/strings/string_number_conversions.h>
#include <base/values.h>
#include <chromeos/userdb_utils.h>

#include "soma/container_spec_wrapper.h"
#include "soma/namespace.h"
#include "soma/port.h"
#include "soma/service_name.h"

namespace soma {
namespace parser {

#define APPKEY "app."

const char ContainerSpecReader::kServiceBundleRoot[] = "/bricks";
const char ContainerSpecReader::kServiceBundleNameKey[] = "image.name";
const char ContainerSpecReader::kAppsKey[] = "apps";
const char ContainerSpecReader::kCommandLineKey[] = APPKEY "exec";
const char ContainerSpecReader::kGidKey[] = APPKEY "group";
const char ContainerSpecReader::kUidKey[] = APPKEY "user";

namespace {

bool ResolveUser(const std::string& user, uid_t* uid) {
  DCHECK(uid);
  if (base::StringToUint(user, uid))
    return true;
  return chromeos::userdb::GetUserInfo(user, uid, nullptr);
}

bool ResolveGroup(const std::string& group, gid_t* gid) {
  DCHECK(gid);
  if (base::StringToUint(group, gid))
    return true;
  return chromeos::userdb::GetGroupInfo(group, gid);
}

}  // namespace

ContainerSpecReader::ContainerSpecReader()
    : reader_(base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS) {
}

ContainerSpecReader::~ContainerSpecReader() {
}

std::unique_ptr<ContainerSpecWrapper> ContainerSpecReader::Read(
    const base::FilePath& spec_file) {
  VLOG(1) << "Reading container spec at " << spec_file.value();
  std::string spec_string;
  if (!base::ReadFileToString(spec_file, &spec_string)) {
    PLOG(ERROR) << "Can't read " << spec_file.value();
    return nullptr;
  }
  return Parse(spec_string);
}

std::unique_ptr<ContainerSpecWrapper> ContainerSpecReader::Parse(
    const std::string& json) {
  std::unique_ptr<const base::Value> root(reader_.ReadToValue(json));
  if (!root) {
    LOG(ERROR) << "Failed to parse: " << reader_.GetErrorMessage();
    return nullptr;
  }
  const base::DictionaryValue* spec_dict = nullptr;
  if (!root->GetAsDictionary(&spec_dict)) {
    LOG(ERROR) << "Spec should have been a dictionary.";
    return nullptr;
  }

  const base::ListValue* apps_list = nullptr;
  const base::DictionaryValue* app_dict = nullptr;
  if (!spec_dict->GetList(kAppsKey, &apps_list) || apps_list->GetSize() != 1 ||
      !apps_list->GetDictionary(0, &app_dict)) {
    LOG(ERROR) << "'apps' must be a list of a single dict.";
    return nullptr;
  }

  std::unique_ptr<ContainerSpecWrapper> spec = ParseRequiredFields(app_dict);
  if (!spec.get())
    return nullptr;

  const base::ListValue* to_parse = nullptr;
  std::vector<std::string> service_names;
  if (spec_dict->GetList(service_name::kListKey, &to_parse)) {
    if (!service_name::ParseList(to_parse, &service_names))
      return nullptr;
    spec->SetServiceNames(service_names);
  }

  std::set<parser::ns::Kind> namespaces;
  if (spec_dict->GetList(ns::kListKey, &to_parse)) {
    if (!ns::ParseList(to_parse, &namespaces))
      return nullptr;
    spec->SetNamespaces(namespaces);
  }

  std::set<parser::port::Number> tcp_ports, udp_ports;
  if (spec_dict->GetList(parser::port::kListKey, &to_parse)) {
    if (!parser::port::ParseList(to_parse, &tcp_ports, &udp_ports))
      return nullptr;
    spec->SetTcpListenPorts(tcp_ports);
    spec->SetUdpListenPorts(udp_ports);
  }

  DevicePathFilterSet device_path_filters;
  if (spec_dict->GetList(DevicePathFilter::kListKey, &to_parse)) {
    if (!DevicePathFilterSet::Parse(to_parse, &device_path_filters))
      return nullptr;
    spec->SetDevicePathFilters(device_path_filters);
  }
  DeviceNodeFilterSet device_node_filters;
  if (spec_dict->GetList(DeviceNodeFilter::kListKey, &to_parse)) {
    if (!DeviceNodeFilterSet::Parse(to_parse, &device_node_filters))
      return nullptr;
    spec->SetDeviceNodeFilters(device_node_filters);
  }

  return std::move(spec);
}

std::unique_ptr<ContainerSpecWrapper> ContainerSpecReader::ParseRequiredFields(
    const base::DictionaryValue* app_dict) {
  DCHECK(app_dict);

  std::string service_bundle_name;
  if (!app_dict->GetString(kServiceBundleNameKey, &service_bundle_name)) {
    LOG(ERROR) << "Service bundle name (image.name) is required.";
    return nullptr;
  }

  std::string user, group;
  if (!app_dict->GetString(kUidKey, &user) ||
      !app_dict->GetString(kGidKey, &group)) {
    LOG(ERROR) << "User and group are required.";
    return nullptr;
  }
  uid_t uid;
  gid_t gid;
  if (!ResolveUser(user, &uid) || !ResolveGroup(group, &gid)) {
    LOG(ERROR) << "User or group could not be resolved to an ID.";
    return nullptr;
  }

  const base::ListValue* command_line = nullptr;
  if (!app_dict->GetList(kCommandLineKey, &command_line) ||
      command_line->GetSize() < 1) {
    LOG(ERROR) << "'app.exec' must be a list of strings.";
    return nullptr;
  }

  std::unique_ptr<ContainerSpecWrapper> spec(
      new ContainerSpecWrapper(
          base::FilePath(kServiceBundleRoot).Append(service_bundle_name),
          uid, gid));

  std::vector<std::string> cl(command_line->GetSize());
  for (size_t i = 0; i < cl.size(); ++i) {
    if (!command_line->GetString(i, &cl[i])) {
      LOG(ERROR) << "'app.exec' must be a list of strings.";
      return nullptr;
    }
  }
  spec->SetCommandLine(cl);
  return std::move(spec);
}

}  // namespace parser
}  // namespace soma
