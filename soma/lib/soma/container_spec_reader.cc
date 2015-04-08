// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/lib/soma/container_spec_reader.h"

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

#include "soma/lib/soma/container_spec_helpers.h"
#include "soma/lib/soma/namespace.h"
#include "soma/lib/soma/port.h"
#include "soma/lib/soma/service_name.h"

namespace soma {
namespace parser {

const char ContainerSpecReader::kServiceBundleRoot[] = "/bricks";
const char ContainerSpecReader::kServiceBundleNameKey[] = "image.name";
const char ContainerSpecReader::kAppsListKey[] = "apps";

const char ContainerSpecReader::kSubAppKey[] = "app";
const char ContainerSpecReader::kCommandLineKey[] = "exec";
const char ContainerSpecReader::kGidKey[] = "group";
const char ContainerSpecReader::kUidKey[] = "user";

const char ContainerSpecReader::kIsolatorsListKey[] = "isolators";
const char ContainerSpecReader::kIsolatorNameKey[] = "name";
const char ContainerSpecReader::kIsolatorSetKey[] = "value.set";

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

std::unique_ptr<ContainerSpec> BuildFromAppFields(
    const std::string& name,
    const base::DictionaryValue* app_dict) {
  DCHECK(app_dict);

  std::string service_bundle_name;
  if (!app_dict->GetString(ContainerSpecReader::kServiceBundleNameKey,
                           &service_bundle_name)) {
    LOG(ERROR) << "Service bundle name (image.name) is required.";
    return nullptr;
  }

  const base::DictionaryValue* subapp_dict = nullptr;
  if (!app_dict->GetDictionary(ContainerSpecReader::kSubAppKey, &subapp_dict)) {
    LOG(ERROR) << "Each dict in 'apps' must contain a dict named 'app'.";
    return nullptr;
  }

  std::string user, group;
  if (!subapp_dict->GetString(ContainerSpecReader::kUidKey, &user) ||
      !subapp_dict->GetString(ContainerSpecReader::kGidKey, &group)) {
    LOG(ERROR) << "User and group are required.";
    return nullptr;
  }
  uid_t uid;
  gid_t gid;
  if (!ResolveUser(user, &uid) || !ResolveGroup(group, &gid)) {
    LOG(ERROR) << "User or group could not be resolved to an ID.";
    return nullptr;
  }

  const base::ListValue* to_parse = nullptr;
  if (!subapp_dict->GetList(ContainerSpecReader::kCommandLineKey, &to_parse) ||
      to_parse->GetSize() < 1) {
    LOG(ERROR) << "'app.exec' must be a list of strings.";
    return nullptr;
  }

  std::vector<std::string> command_line(to_parse->GetSize());
  for (size_t i = 0; i < command_line.size(); ++i) {
    if (!to_parse->GetString(i, &command_line[i])) {
      LOG(ERROR) << "'app.exec' must be a list of strings.";
      return nullptr;
    }
  }

  std::unique_ptr<ContainerSpec> spec(
      container_spec_helpers::CreateContainerSpec(
          name,
          base::FilePath(ContainerSpecReader::kServiceBundleRoot).Append(
              service_bundle_name),
          command_line,
          uid,
          gid));

  std::set<port::Number> tcp_ports, udp_ports;
  if (subapp_dict->GetList(port::kListKey, &to_parse)) {
    if (!port::ParseList(*to_parse, &tcp_ports, &udp_ports))
      return nullptr;
    container_spec_helpers::SetTcpListenPorts(tcp_ports, spec.get());
    container_spec_helpers::SetUdpListenPorts(udp_ports, spec.get());
  }

  return std::move(spec);
}

bool ParseIsolators(const base::ListValue& isolators, ContainerSpec* spec) {
  for (const base::Value* value : isolators) {
    const base::DictionaryValue* isolator = nullptr;
    if (!value->GetAsDictionary(&isolator)) {
      LOG(ERROR) << "Isolators must be dicts, not " << value;
      return false;
    }

    std::string name;
    const base::ListValue* set = nullptr;
    if (!isolator->GetString(ContainerSpecReader::kIsolatorNameKey, &name) ||
        !isolator->GetList(ContainerSpecReader::kIsolatorSetKey, &set)) {
      LOG(ERROR) << "Isolators must be a dict with a name and a value, not "
                 << isolator;
      return false;
    }

    // Should we support more than a few more of these, this will need to get
    // refactored to be more elegant.
    if (name == DevicePathFilter::kListKey) {
      DevicePathFilter::Set device_path_filters;
      if (!DevicePathFilter::ParseList(*set, &device_path_filters))
        return false;
      container_spec_helpers::SetDevicePathFilters(device_path_filters, spec);
    } else if (name == DeviceNodeFilter::kListKey) {
      DeviceNodeFilter::Set device_node_filters;
      if (!DeviceNodeFilter::ParseList(*set, &device_node_filters))
        return false;
      container_spec_helpers::SetDeviceNodeFilters(device_node_filters, spec);
    } else if (name == ns::kListKey) {
      std::set<ns::Kind> namespaces;
      if (!ns::ParseList(*set, &namespaces))
        return false;
      container_spec_helpers::SetNamespaces(namespaces, spec);
    }
  }
  return true;
}

}  // namespace

ContainerSpecReader::ContainerSpecReader()
    : reader_(base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS) {
}

ContainerSpecReader::~ContainerSpecReader() {
}

std::unique_ptr<ContainerSpec> ContainerSpecReader::Read(
    const base::FilePath& spec_file) {
  VLOG(1) << "Reading container spec at " << spec_file.value();
  std::string json;
  if (!base::ReadFileToString(spec_file, &json)) {
    PLOG(ERROR) << "Can't read " << spec_file.value();
    return nullptr;
  }

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
  if (!spec_dict->GetList(kAppsListKey, &apps_list) ||
      apps_list->GetSize() != 1 ||
      !apps_list->GetDictionary(0, &app_dict)) {
    LOG(ERROR) << "'apps' must be a list of a single dict.";
    return nullptr;
  }

  std::unique_ptr<ContainerSpec> spec =
      BuildFromAppFields(spec_file.value(), app_dict);
  if (!spec.get())
    return nullptr;

  const base::ListValue* to_parse = nullptr;
  std::vector<std::string> service_names;
  if (spec_dict->GetList(service_name::kListKey, &to_parse)) {
    if (!service_name::ParseList(to_parse, &service_names))
      return nullptr;
    container_spec_helpers::SetServiceNames(service_names, spec.get());
  }

  if (spec_dict->GetList(kIsolatorsListKey, &to_parse) &&
      !ParseIsolators(*to_parse, spec.get())) {
    return nullptr;
  }

  return std::move(spec);
}

}  // namespace parser
}  // namespace soma
