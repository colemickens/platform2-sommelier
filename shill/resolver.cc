// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/resolver.h"

#include <string>
#include <vector>

#include <base/file_util.h>
#include <base/string_util.h>
#include <base/stringprintf.h>

#include "shill/ipconfig.h"
#include "shill/logging.h"

using base::StringPrintf;
using std::string;
using std::vector;

namespace shill {

namespace {
base::LazyInstance<Resolver> g_resolver = LAZY_INSTANCE_INITIALIZER;
}  // namespace

const char Resolver::kDefaultShortTimeoutTechnologies[] = "ethernet,wifi";
const char Resolver::kDefaultTimeoutOptions[] =
    "options single-request timeout:1 attempts:3";
const char Resolver::kShortTimeoutOptions[] =
    "options single-request timeout-ms:300 attempts:15";

Resolver::Resolver() {}

Resolver::~Resolver() {}

Resolver* Resolver::GetInstance() {
  return g_resolver.Pointer();
}

bool Resolver::SetDNSFromIPConfig(const IPConfigRefPtr &ipconfig,
                                  TimeoutParameters timeout) {
  SLOG(Resolver, 2) << __func__;

  CHECK(!path_.empty());

  const IPConfig::Properties &props = ipconfig->properties();

  return SetDNSFromLists(props.dns_servers, props.domain_search, timeout);
}

bool Resolver::SetDNSFromLists(const std::vector<std::string> &dns_servers,
                               const std::vector<std::string> &domain_search,
                               TimeoutParameters timeout) {
  SLOG(Resolver, 2) << __func__;

  if (dns_servers.empty() && domain_search.empty()) {
    SLOG(Resolver, 2) << "DNS list is empty";
    return ClearDNS();
  }

  vector<string> lines;
  vector<string>::const_iterator iter;
  for (iter = dns_servers.begin();
       iter != dns_servers.end(); ++iter) {
    lines.push_back("nameserver " + *iter);
  }

  if (!domain_search.empty()) {
    lines.push_back("search " + JoinString(domain_search, ' '));
  }

  // Send queries one-at-a-time, rather than parallelizing IPv4
  // and IPv6 queries for a single host.  Also override the default
  // 5-second request timeout and 2 retries.
  if (timeout == kDefaultTimeout) {
    lines.push_back(kDefaultTimeoutOptions);
  } else if (timeout == kShortTimeout) {
    lines.push_back(kShortTimeoutOptions);
  } else {
    NOTIMPLEMENTED() << "Unknown Resolver timeout parameters";
  }

  // Newline at end of file
  lines.push_back("");

  string contents = JoinString(lines, '\n');

  SLOG(Resolver, 2) << "Writing DNS out to " << path_.value();
  int count = file_util::WriteFile(path_, contents.c_str(), contents.size());

  return count == static_cast<int>(contents.size());
}

bool Resolver::ClearDNS() {
  SLOG(Resolver, 2) << __func__;

  CHECK(!path_.empty());

  return file_util::Delete(path_, false);
}

}  // namespace shill
