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
#include "soma/lib/soma/userdb.h"
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

const char ACLParser::kServiceKey[] = "service";
const char ACLParser::kWhitelistKey[] = "whitelist";

ACLParser::ACLParser(UserdbInterface* userdb) : userdb_(userdb) {
}

const char UserACLParser::kName[] = "os/bruteus/service-user-whitelist";

UserACLParser::UserACLParser(UserdbInterface* userdb) : ACLParser(userdb) {
}

bool UserACLParser::ParseInternal(const base::DictionaryValue& value,
                                  ContainerSpec* spec) {
  std::string service_name;
  const base::ListValue* whitelist = nullptr;
  if (!value.GetString(kServiceKey, &service_name) ||
      !value.GetList(kWhitelistKey, &whitelist)) {
    LOG(ERROR) << "ACL isolator must consist of a name and a whitelist, not"
               << std::endl
               << value;
    return false;
  }
  std::set<uid_t> acl;
  for (const base::Value* value : *whitelist) {
    std::string user;
    uid_t uid = -1;
    if (!value->GetAsString(&user) || !userdb_->ResolveUser(user, &uid)) {
      LOG(ERROR) << "ACL must be list of usernames, not " << *value;
      return false;
    }
    acl.insert(uid);
  }
  container_spec_helpers::SetUserACL(service_name, acl, spec);
  return true;
}

const char GroupACLParser::kName[] = "os/bruteus/service-group-whitelist";

GroupACLParser::GroupACLParser(UserdbInterface* userdb) : ACLParser(userdb) {
}

bool GroupACLParser::ParseInternal(const base::DictionaryValue& value,
                                   ContainerSpec* spec) {
  std::string service_name;
  const base::ListValue* whitelist = nullptr;
  if (!value.GetString(kServiceKey, &service_name) ||
      !value.GetList(kWhitelistKey, &whitelist)) {
    LOG(ERROR) << "ACL isolator must consist of a name and a whitelist, not"
               << std::endl
               << value;
    return false;
  }
  std::set<uid_t> acl;
  for (const base::Value* value : *whitelist) {
    std::string group;
    uid_t uid = -1;
    if (!value->GetAsString(&group) || !userdb_->ResolveGroup(group, &uid)) {
      LOG(ERROR) << "ACL must be list of groupnames, not " << *value;
      return false;
    }
    acl.insert(uid);
  }
  container_spec_helpers::SetGroupACL(service_name, acl, spec);
  return true;
}

}  // namespace parser
}  // namespace soma
