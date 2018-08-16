// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "portier/nd_proxy.h"

#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/stl_util.h>

#include "portier/ipv6_util.h"

namespace portier {

using std::shared_ptr;
using std::string;

using brillo::MessageLoop;
using shill::ByteString;
using shill::IPAddress;

using TaskId = brillo::MessageLoop::TaskId;
using Code = Status::Code;

using FdTaskPair = std::pair<const int32_t, TaskId>;
using NameTaskPair = std::pair<const string, TaskId>;
using NameIfPair = std::pair<const string, shared_ptr<ProxyInterface>>;
using NameNamePair = std::pair<const string, string>;

namespace {

// Specified disable time for proxy interfaces which have been disabled
// for loop prevention purposes.  See RFC 4389 section 4.1.3.3.
constexpr base::TimeDelta kInterfaceDisableTime =
    base::TimeDelta::FromMinutes(60);

}  // namespace

std::unique_ptr<NeighborDiscoveryProxy> NeighborDiscoveryProxy::Create(
    bool nested_mode) {
  auto ndp_ptr = std::unique_ptr<NeighborDiscoveryProxy>(
      new NeighborDiscoveryProxy(nested_mode));
  return ndp_ptr;
}

Status NeighborDiscoveryProxy::ManagerInterface(const string& if_name) {
  if (IsManagingInterface(if_name)) {
    return Status(Code::ALREADY_EXISTS)
           << "The interface " << if_name << " is already being managed";
  }

  auto uif = ProxyInterface::Create(if_name);
  if (!uif) {
    return Status(Code::UNEXPECTED_FAILURE)
           << "Failed to create proxy interface " << if_name;
  }

  shared_ptr<ProxyInterface> proxy_if(std::move(uif));
  proxy_ifs_.insert(NameIfPair(if_name, proxy_if));

  // Setup event handler.
  const int nd_fd = proxy_if->GetNDFd();
  const TaskId nd_task_id = MessageLoop::current()->WatchFileDescriptor(
      nd_fd, MessageLoop::kWatchRead, true /* presistent */,
      base::Bind(&NeighborDiscoveryProxy::HandleNeighborDiscoverPacket,
                 base::Unretained(this), if_name));
  fd_tasks_.insert(FdTaskPair(nd_fd, nd_task_id));

  const int ipv6_fd = proxy_if->GetIPv6Fd();
  const TaskId ipv6_task_id = MessageLoop::current()->WatchFileDescriptor(
      ipv6_fd, MessageLoop::kWatchRead, true /* presistent */,
      base::Bind(&NeighborDiscoveryProxy::HandleIPv6Packet,
                 base::Unretained(this), if_name));
  fd_tasks_.insert(FdTaskPair(ipv6_fd, ipv6_task_id));

  LOG(INFO) << "ND Proxy is now managing interface " << if_name;
  return Status();
}

Status NeighborDiscoveryProxy::ReleaseInterface(const string& if_name) {
  auto it = proxy_ifs_.find(if_name);
  if (it == proxy_ifs_.end()) {
    return Status(Code::DOES_NOT_EXIST)
           << "The interface " << if_name << " is not being managed";
  }

  auto proxy_if = it->second;

  // Remove from proxy group if member.
  if (proxy_if->HasGroup()) {
    auto group = proxy_if->GetGroup();
    group->RemoveMember(proxy_if);
  }

  // Cancel all associated tasks.
  const auto nd_pair = fd_tasks_.find(proxy_if->GetNDFd());
  if (nd_pair != fd_tasks_.end()) {
    MessageLoop::current()->CancelTask(nd_pair->second);
    fd_tasks_.erase(nd_pair);
  }
  const auto ipv6_pair = fd_tasks_.find(proxy_if->GetIPv6Fd());
  if (ipv6_pair != fd_tasks_.end()) {
    MessageLoop::current()->CancelTask(ipv6_pair->second);
    fd_tasks_.erase(ipv6_pair);
  }
  const auto loop_pair = loop_tasks_.find(if_name);
  if (loop_pair != loop_tasks_.end()) {
    MessageLoop::current()->CancelTask(loop_pair->second);
    loop_tasks_.erase(loop_pair);
  }

  // Remove interface from interface list.
  proxy_ifs_.erase(it);
  DCHECK_EQ(proxy_if.use_count(), 1);
  proxy_if.reset();

  LOG(INFO) << "ND Proxy has stopped managing interface " << if_name;
  return Status();
}

bool NeighborDiscoveryProxy::IsManagingInterface(const string& if_name) const {
  return base::ContainsKey(proxy_ifs_, if_name);
}

Status NeighborDiscoveryProxy::CreateProxyGroup(const string& pg_name) {
  const Status create_status = group_manager_.CreateGroup(pg_name);
  if (create_status) {
    LOG(INFO) << "Created proxy group " << pg_name;
  }
  return create_status;
}

Status NeighborDiscoveryProxy::ReleaseProxyGroup(const string& pg_name) {
  const Status release_status = group_manager_.ReleaseGroup(pg_name);
  if (release_status) {
    LOG(INFO) << "Released proxy group " << pg_name;
  }
  return release_status;
}

bool NeighborDiscoveryProxy::HasProxyGroup(const string& pg_name) const {
  return group_manager_.HasGroup(pg_name);
}

// Membership.
Status NeighborDiscoveryProxy::AddToGroup(const string& if_name,
                                          const string& pg_name,
                                          bool as_upstream) {
  auto proxy_if = GetInterface(if_name);
  if (!proxy_if) {
    return Status(Code::DOES_NOT_EXIST)
           << "Interface " << if_name << " does not exists";
  }
  auto group = group_manager_.GetGroup(pg_name);
  if (!group) {
    return Status(Code::DOES_NOT_EXIST)
           << "Proxy group " << pg_name << " does not exists";
  }
  if (proxy_if->HasGroup()) {
    return Status(Code::ALREADY_EXISTS)
           << "Interface " << if_name << " is already a member of group "
           << proxy_if->GetGroup()->name();
  }
  if (!group->AddMember(proxy_if)) {
    return Status(Code::UNEXPECTED_FAILURE)
           << "Failed to add interface " << if_name << " to proxy group "
           << pg_name;
  }
  if (as_upstream) {
    group->SetUpstream(proxy_if);
  }
  return Status();
}

Status NeighborDiscoveryProxy::RemoveFromGroup(const string& if_name) {
  auto proxy_if = GetInterface(if_name);
  if (!proxy_if) {
    return Status(Code::DOES_NOT_EXIST)
           << "Interface " << if_name << " does not exists";
  }
  auto group = proxy_if->GetGroup();
  if (!group) {
    return Status(Code::DOES_NOT_EXIST)
           << "Interface " << if_name << " is not a member of any group";
  }
  if (!group->RemoveMember(proxy_if)) {
    return Status(Code::UNEXPECTED_FAILURE)
           << "Failed to remove interface " << if_name << " from proxy group "
           << group->name();
  }
  // Clear any loop issue.  Should be done after so that no packet is
  // proxied.
  const auto loop_pair = loop_tasks_.find(if_name);
  if (loop_pair != loop_tasks_.end()) {
    MessageLoop::current()->CancelTask(loop_pair->second);
    loop_tasks_.erase(loop_pair);
  }
  return Status();
}

Status NeighborDiscoveryProxy::SetAsUpstream(const std::string& if_name) {
  auto proxy_if = GetInterface(if_name);
  if (!proxy_if) {
    return Status(Code::DOES_NOT_EXIST)
           << "Interface " << if_name << " does not exists";
  }
  auto group = proxy_if->GetGroup();
  if (!group) {
    return Status(Code::DOES_NOT_EXIST)
           << "Interface " << if_name << " is not a member of any group";
  }
  group->SetUpstream(proxy_if);
  return Status();
}

// private.
shared_ptr<ProxyInterface> NeighborDiscoveryProxy::GetInterface(
    const string& if_name) const {
  auto it = proxy_ifs_.find(if_name);
  if (it != proxy_ifs_.cend()) {
    return it->second;
  }
  return nullptr;
}

// private.
void NeighborDiscoveryProxy::HandleLoopDetection(
    shared_ptr<ProxyInterface> proxy_if) {
  DCHECK(proxy_if);
  DCHECK(proxy_if->HasGroup());
  // Clear old task, might be left over from a previous loop detection.
  const auto loop_pair = loop_tasks_.find(proxy_if->name());
  if (loop_pair != loop_tasks_.end()) {
    MessageLoop::current()->CancelTask(loop_pair->second);
    loop_tasks_.erase(loop_pair);
  }
  proxy_if->MarkLoopDetected();
  const TaskId loop_task_id = MessageLoop::current()->PostDelayedTask(
      base::Bind(&NeighborDiscoveryProxy::LoopTimeOut, base::Unretained(this),
                 proxy_if->name(), proxy_if->GetGroup()->name()),
      kInterfaceDisableTime);
  loop_tasks_.insert(NameTaskPair(proxy_if->name(), loop_task_id));
}

// Packet event handlers.

// private.
void NeighborDiscoveryProxy::HandleNeighborDiscoverPacket(string if_name) {
  auto proxy_if = GetInterface(if_name);
  if (!proxy_if) {
    // Possible that interface was removed while the call back was queued.
    return;
  }
  if (!proxy_if->IsEnabled()) {
    proxy_if->DiscardNeighborDiscoveryInput();
  }
  if (!proxy_if->HasGroup()) {
    LOG(WARNING) << "Proxy interface " << proxy_if->name()
                 << " was enabled, but not part of a group";
    proxy_if->MarkGroupless();
    proxy_if->DiscardNeighborDiscoveryInput();
    return;
  }

  IPv6EtherHeader header_fields;
  NeighborDiscoveryMessage nd_message;
  Status receive_status =
      proxy_if->ReceiveNeighborDiscoveryMessage(&header_fields, &nd_message);

  if (receive_status.code() == Code::RESULT_UNAVAILABLE) {
    // Possible if a zero packet was received.
    return;
  }
  if (!receive_status) {
    receive_status << "Failed to receive ND on if " << proxy_if->name();
    LOG(ERROR) << receive_status;
    return;
  }

  // Check if locally destined.
  if (proxy_if->HasIPv6Address(header_fields.destination_address)) {
    // Don't need to handle locally destined packets.
    return;
  }

  ProxyGroup* group = proxy_if->GetGroup();
  // We already did a check for proxy_if->HasGroup().
  CHECK(group);

  // Loop prevention check.
  const NeighborDiscoveryMessage::Type nd_type = nd_message.type();
  if (nd_type == NeighborDiscoveryMessage::kTypeRouterAdvert) {
    if (!proxy_if->IsUpstream()) {
      // Router advertisements should not be received on a downstream
      // interface.
      LOG(WARNING) << "An RA was received on downstream interface "
                   << proxy_if->name();
      HandleLoopDetection(proxy_if);
      return;
    }
    // else, interface is upstream.

    // Check if there is a potential loop.
    bool proxy_flag = false;
    nd_message.GetProxyFlag(&proxy_flag);
    if (proxy_flag && !IsNested()) {
      // If the daemon is not running in nested mode, than the proxy bit
      // indicates a loop on this interface.
      LOG(WARNING) << "A proxied RA set received on interface "
                   << proxy_if->name();
      HandleLoopDetection(proxy_if);
      return;
    }
  }

  // Handle the case of multicast.
  if (IPv6AddressIsMulticast(header_fields.destination_address)) {
    LLAddress destination_ll_address;
    IPv6GetMulticastLinkLayerAddress(header_fields.destination_address,
                                     &destination_ll_address);
    LOG(INFO) << "Multicast ND Msg (" << nd_message.type() << ") "
              << proxy_if->name() << " from "
              << header_fields.source_address.ToString();
    // Multicast, then forward packet to all interfaces other than
    // the one received on.
    auto group_members = group->GetMembers();
    for (auto& group_if : group_members) {
      if (group_if == proxy_if) {
        // Don't proxy out the same interface.
        continue;
      }
      if (!group_if->IsEnabled()) {
        continue;
      }

      const Status send_status = group_if->ProxyNeighborDiscoveryMessage(
          header_fields, destination_ll_address, nd_message);
      if (!send_status) {
        LOG(ERROR) << send_status;
      }
    }
    return;
  }

  NeighborCacheEntry out_entry;
  if (!neighbor_cache_.GetEntry(header_fields.destination_address,
                                group->name(), &out_entry)) {
    // TODO(siquit): Queue ND packet and perform neighbor discovery.
    LOG(INFO) << "No neighbor for "
              << header_fields.destination_address.ToString();
    return;
  }

  shared_ptr<ProxyInterface> out_if = GetInterface(out_entry.if_name);

  if (!out_if || !out_if->IsEnabled()) {
    return;
  }

  if (out_if->name() == proxy_if->name()) {
    // Don't proxy out the same interface.  Should be silently dropped.
    return;
  }

  LOG(INFO) << "Unicast ND Msg (" << nd_message.type() << ") "
            << proxy_if->name() << " from "
            << header_fields.source_address.ToString() << " out "
            << out_if->name() << " to "
            << header_fields.destination_address.ToString();

  Status send_status = out_if->ProxyNeighborDiscoveryMessage(
      header_fields, out_entry.ll_address, nd_message);
  if (!send_status) {
    send_status << "Failed to proxy from " << proxy_if->name() << " to "
                << out_if->name();
    LOG(ERROR) << send_status;
  }
}

void NeighborDiscoveryProxy::HandleIPv6Packet(string if_name) {
  auto proxy_if = GetInterface(if_name);
  if (!proxy_if) {
    // Possible that interface was removed while the call back was queued.
    return;
  }
  if (!proxy_if->IsEnabled()) {
    proxy_if->DiscardIPv6Input();
  }
  if (!proxy_if->HasGroup()) {
    LOG(WARNING) << "Proxy interface " << proxy_if->name()
                 << " was enabled, but not part of a group";
    proxy_if->MarkGroupless();
    proxy_if->DiscardIPv6Input();
    return;
  }

  IPv6EtherHeader header_fields;
  ByteString payload;
  Status receive_status = proxy_if->ReceiveIPv6Packet(&header_fields, &payload);
  if (receive_status.code() == Code::RESULT_UNAVAILABLE) {
    // Possible if a zero packet was received.
    return;
  }
  if (!receive_status) {
    receive_status << "Failed to receive ND on if " << proxy_if->name();
    LOG(ERROR) << receive_status;
    return;
  }

  // Check if locally destined.
  if (proxy_if->HasIPv6Address(header_fields.destination_address)) {
    // Don't need to handle locally destined packets.
    return;
  }

  ProxyGroup* group = proxy_if->GetGroup();
  // We already did a check for proxy_if->HasGroup().
  CHECK(group);

  // If multicast, then forward packet to all interfaces other than
  // the one received on.
  if (IPv6AddressIsMulticast(header_fields.destination_address)) {
    LLAddress destination_ll_address;
    IPv6GetMulticastLinkLayerAddress(header_fields.destination_address,
                                     &destination_ll_address);

    LOG(INFO) << "Multicast IPv6 Packet " << proxy_if->name() << " from "
              << header_fields.source_address.ToString();
    // Multicast, then forward packet to all interfaces other than
    // the one received on.
    auto group_members = group->GetMembers();
    for (auto& group_if : group_members) {
      if (group_if == proxy_if) {
        // Don't proxy out the same interface.
        continue;
      }
      if (!group_if->IsEnabled()) {
        continue;
      }

      const Status send_status = group_if->SendIPv6Packet(
          header_fields, destination_ll_address, payload);
      if (!send_status) {
        LOG(ERROR) << send_status;
      }
    }
    return;
  }

  NeighborCacheEntry out_entry;
  if (!neighbor_cache_.GetEntry(header_fields.destination_address,
                                group->name(), &out_entry)) {
    // TODO(sigquit): Queue IP packet and perform neighbor discovery.
    return;
  }

  shared_ptr<ProxyInterface> out_if = GetInterface(out_entry.if_name);
  if (!out_if || !out_if->IsEnabled()) {
    return;
  }

  if (out_if->name() == proxy_if->name()) {
    // Don't proxy out the same interface.  Should be silently dropped.
    return;
  }
  LOG(INFO) << "Unicast IPv6 Packet " << proxy_if->name() << " from "
            << header_fields.source_address.ToString() << " out "
            << out_if->name() << " to "
            << header_fields.destination_address.ToString();

  Status send_status =
      out_if->SendIPv6Packet(header_fields, out_entry.ll_address, payload);
  if (!send_status) {
    send_status << "Failed to proxy from " << proxy_if->name() << " to "
                << out_if->name();
    LOG(ERROR) << send_status;
  }
}

void NeighborDiscoveryProxy::LoopTimeOut(string if_name, string pg_name) {
  auto proxy_if = GetInterface(if_name);
  if (!proxy_if || !proxy_if->HasGroup() ||
      proxy_if->GetGroup()->name() != pg_name ||
      !base::ContainsKey(loop_tasks_, if_name)) {
    return;
  }
  proxy_if->ClearLoopDetected();
}

}  // namespace portier
