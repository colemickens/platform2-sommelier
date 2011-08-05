// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/resolver.h"

#include <string>
#include <vector>

#include <base/file_util.h>
#include <base/string_util.h>
#include <base/stringprintf.h>

#include "shill/ipconfig.h"

namespace shill {

using base::StringPrintf;
using std::string;
using std::vector;

Resolver::Resolver() {}

Resolver::~Resolver() {}

Resolver* Resolver::GetInstance() {
  return Singleton<Resolver>::get();
}

bool Resolver::SetDNS(const IPConfigRefPtr &ipconfig) {
  CHECK(!path_.empty());

  const IPConfig::Properties &props = ipconfig->properties();

  if (!props.dns_servers.size() && !props.domain_search.size()) {
    return ClearDNS();
  }

  vector<string> lines;
  vector<string>::const_iterator iter;
  for (iter = props.dns_servers.begin();
       iter != props.dns_servers.end(); ++iter) {
    lines.push_back("nameserver " + *iter);
  }

  if (props.domain_search.size()) {
    lines.push_back("search " + JoinString(props.domain_search, ' '));
  }

  // Newline at end of file
  lines.push_back("");

  string contents = JoinString(lines, '\n');
  int count = file_util::WriteFile(path_, contents.c_str(), contents.size());

  return count == static_cast<int>(contents.size());
}

bool Resolver::ClearDNS() {
  CHECK(!path_.empty());

  return file_util::Delete(path_, false);
}

}  // namespace shill
