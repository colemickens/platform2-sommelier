// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/shims/ppp.h"

#include <arpa/inet.h>
#include <netinet/in.h>

extern "C" {
#include <pppd/fsm.h>
#include <pppd/ipcp.h>
}

#include <base/command_line.h>
#include <base/logging.h>
#include <chromeos/syslog_logging.h>
#include <dbus-c++/eventloop-integration.h>

#include "shill/l2tp_ipsec_driver.h"
#include "shill/rpc_task.h"
#include "shill/shims/environment.h"
#include "shill/shims/task_proxy.h"

using std::map;
using std::string;

namespace shill {

namespace shims {

static base::LazyInstance<PPP> g_ppp = LAZY_INSTANCE_INITIALIZER;

PPP::PPP() : running_(false) {}

PPP::~PPP() {}

// static
PPP *PPP::GetInstance() {
  return g_ppp.Pointer();
}

void PPP::Init() {
  if (running_) {
    return;
  }
  running_ = true;
  CommandLine::Init(0, NULL);
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogHeader);
  LOG(INFO) << "PPP started.";
}

bool PPP::GetSecret(string *username, string *password) {
  LOG(INFO) << __func__;
  if (!CreateProxy()) {
    return false;
  }
  bool success = proxy_->GetSecret(username, password);
  DestroyProxy();
  return success;
}

void PPP::OnConnect(const string &ifname) {
  LOG(INFO) << __func__ << "(" << ifname << ")";
  if (!ipcp_gotoptions[0].ouraddr) {
    LOG(ERROR) << "ouraddr not set.";
    return;
  }
  map<string, string> dict;
  dict[kL2TPIPSecInterfaceName] = ifname;
  dict[kL2TPIPSecInternalIP4Address] =
      ConvertIPToText(&ipcp_gotoptions[0].ouraddr);
  dict[kL2TPIPSecExternalIP4Address] =
      ConvertIPToText(&ipcp_hisoptions[0].hisaddr);
  if (ipcp_gotoptions[0].default_route) {
    dict[kL2TPIPSecGatewayAddress] = dict[kL2TPIPSecExternalIP4Address];
  }
  if (ipcp_gotoptions[0].dnsaddr[0]) {
    dict[kL2TPIPSecDNS1] = ConvertIPToText(&ipcp_gotoptions[0].dnsaddr[0]);
  }
  if (ipcp_gotoptions[0].dnsaddr[1]) {
    dict[kL2TPIPSecDNS2] = ConvertIPToText(&ipcp_gotoptions[0].dnsaddr[1]);
  }
  string lns_address;
  if (Environment::GetInstance()->GetVariable("LNS_ADDRESS", &lns_address)) {
    dict[kL2TPIPSecLNSAddress] = lns_address;
  }
  if (CreateProxy()) {
    proxy_->Notify(kL2TPIPSecReasonConnect, dict);
    DestroyProxy();
  }
}

void PPP::OnDisconnect() {
  LOG(INFO) << __func__;
  if (CreateProxy()) {
    map<string, string> dict;
    proxy_->Notify(kL2TPIPSecReasonDisconnect, dict);
    DestroyProxy();
  }
}

bool PPP::CreateProxy() {
  Environment *environment = Environment::GetInstance();
  string service, path;
  if (!environment->GetVariable(shill::kRPCTaskServiceVariable, &service) ||
      !environment->GetVariable(shill::kRPCTaskPathVariable, &path)) {
    LOG(ERROR) << "Environment variables not available.";
    return false;
  }

  dispatcher_.reset(new DBus::BusDispatcher());
  DBus::default_dispatcher = dispatcher_.get();
  connection_.reset(new DBus::Connection(DBus::Connection::SystemBus()));
  proxy_.reset(new TaskProxy(connection_.get(), path, service));
  LOG(INFO) << "Task proxy created: " << service << " - " << path;
  return true;
}

void PPP::DestroyProxy() {
  proxy_.reset();
  connection_.reset();
  DBus::default_dispatcher = NULL;
  dispatcher_.reset();
  LOG(INFO) << "Task proxy destroyed.";
}

// static
string PPP::ConvertIPToText(const void *addr) {
  char text[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, addr, text, INET_ADDRSTRLEN);
  return text;
}

}  // namespace shims

}  // namespace shill
