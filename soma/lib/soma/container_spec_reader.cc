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

#include "soma/lib/soma/annotations.h"
#include "soma/lib/soma/container_spec_helpers.h"
#include "soma/lib/soma/isolator_parser.h"
#include "soma/lib/soma/namespace.h"
#include "soma/lib/soma/port.h"

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

bool BuildFromAppFields(const base::DictionaryValue* app_dict,
                        ContainerSpec::Executable* executable) {
  DCHECK(app_dict);
  DCHECK(executable);

  const base::DictionaryValue* subapp_dict = nullptr;
  if (!app_dict->GetDictionary(ContainerSpecReader::kSubAppKey, &subapp_dict)) {
    LOG(ERROR) << "Each dict in 'apps' must contain a dict named 'app'.";
    return false;
  }

  std::string user, group;
  if (!subapp_dict->GetString(ContainerSpecReader::kUidKey, &user) ||
      !subapp_dict->GetString(ContainerSpecReader::kGidKey, &group)) {
    LOG(ERROR) << "User and group are required.";
    return false;
  }
  uid_t uid;
  gid_t gid;
  if (!ResolveUser(user, &uid) || !ResolveGroup(group, &gid)) {
    LOG(ERROR) << "User or group could not be resolved to an ID.";
    return false;
  }
  container_spec_helpers::SetUidAndGid(uid, gid, executable);

  const base::ListValue* to_parse = nullptr;
  if (!subapp_dict->GetList(ContainerSpecReader::kCommandLineKey, &to_parse) ||
      to_parse->GetSize() < 1) {
    LOG(ERROR) << "'app.exec' must be a list of strings.";
    return false;
  }

  std::vector<std::string> command_line(to_parse->GetSize());
  for (size_t i = 0; i < command_line.size(); ++i) {
    if (!to_parse->GetString(i, &command_line[i])) {
      LOG(ERROR) << "'app.exec' must be a list of strings.";
      return false;
    }
  }
  {
    base::FilePath exe_path(command_line[0]);
    int permissions = 00;
    if (!exe_path.IsAbsolute() || !base::PathExists(exe_path) ||
        !base::GetPosixFilePermissions(exe_path, &permissions) ||
        (permissions & base::FILE_PERMISSION_EXECUTE_BY_USER) == 0) {
      LOG(ERROR) << "Command line must reference an existing executable "
                 << "by absolute path: " << exe_path.value();
      return false;
    }
  }
  container_spec_helpers::SetCommandLine(command_line, executable);

  std::set<port::Number> tcp_ports, udp_ports;
  if (subapp_dict->GetList(port::kListKey, &to_parse)) {
    if (!port::ParseList(*to_parse, &tcp_ports, &udp_ports))
      return false;
    container_spec_helpers::SetTcpListenPorts(tcp_ports, executable);
    container_spec_helpers::SetUdpListenPorts(udp_ports, executable);
  }

  return true;
}

}  // namespace

bool ContainerSpecReader::ParseIsolators(const base::ListValue& isolators,
                                         ContainerSpec* spec) {
  for (const base::Value* value : isolators) {
    const base::DictionaryValue* isolator = nullptr;
    if (!value->GetAsDictionary(&isolator)) {
      LOG(ERROR) << "Isolators must be dicts, not " << value;
      return false;
    }

    std::string name;
    const base::DictionaryValue* object = nullptr;
    if (!isolator->GetString(IsolatorParserInterface::kNameKey, &name) ||
        !isolator->GetDictionary(IsolatorParserInterface::kValueKey, &object)) {
      LOG(ERROR) << "Isolators must be a dict with a name and a value, not\n"
                 << *isolator;
      return false;
    }

    if (isolator_parsers_.find(name) != isolator_parsers_.end()) {
      if (!isolator_parsers_[name]->Parse(*object, spec))
        return false;
    } else {
      LOG(WARNING) << "Ignoring isolator: " << name;
    }
  }
  return true;
}

ContainerSpecReader::ContainerSpecReader()
    : reader_(base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS) {
  isolator_parsers_.emplace(
      DevicePathFilterParser::kName,
      std::unique_ptr<IsolatorParserInterface>(new DevicePathFilterParser));
  isolator_parsers_.emplace(
      DeviceNodeFilterParser::kName,
      std::unique_ptr<IsolatorParserInterface>(new DeviceNodeFilterParser));
  isolator_parsers_.emplace(
      NamespacesParser::kName,
      std::unique_ptr<IsolatorParserInterface>(new NamespacesParser));
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
  if (!spec_dict->GetList(kAppsListKey, &apps_list)) {
    LOG(ERROR) << "'apps' must be a list.";
    return nullptr;
  }

  std::unique_ptr<ContainerSpec> spec =
      container_spec_helpers::CreateContainerSpec(spec_file.value());
  DCHECK(spec.get());

  std::string service_bundle_name;
  for (const base::Value* value : *apps_list) {
    const base::DictionaryValue* app_dict = nullptr;
    if (!value->GetAsDictionary(&app_dict)) {
      LOG(ERROR) << "Each entry in 'apps' must be a dict, not " << value;
      return nullptr;
    }
    std::string image_name;
    if (!app_dict->GetString(kServiceBundleNameKey, &image_name)) {
      LOG(ERROR) << "Service bundle name (image.name) is required.";
      return nullptr;
    }
    if (!service_bundle_name.empty() && image_name != service_bundle_name) {
      LOG(ERROR) << "All elements of 'apps' must have the same image.name.";
      return nullptr;
    }
    service_bundle_name = image_name;
    if (!BuildFromAppFields(app_dict, spec->add_executables()))
      return nullptr;
  }

  container_spec_helpers::SetServiceBundlePath(
      base::FilePath(kServiceBundleRoot).Append(service_bundle_name),
      spec.get());

  const base::ListValue* to_parse = nullptr;
  std::vector<std::string> service_names;
  if (spec_dict->GetList(annotations::kListKey, &to_parse)) {
    if (!annotations::ParseServiceNameList(*to_parse, &service_names))
      return nullptr;
    container_spec_helpers::SetServiceNames(service_names, spec.get());
    spec->set_is_persistent(annotations::IsPersistent(*to_parse));
  }

  if (spec_dict->GetList(kIsolatorsListKey, &to_parse) &&
      !ParseIsolators(*to_parse, spec.get())) {
    return nullptr;
  }

  return std::move(spec);
}

}  // namespace parser
}  // namespace soma
