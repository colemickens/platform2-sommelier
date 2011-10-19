// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dns_client.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <map>
#include <set>
#include <string>
#include <tr1/memory>
#include <vector>

#include <base/stl_util-inl.h>

#include <shill/shill_ares.h>
#include <shill/shill_time.h>

using std::map;
using std::set;
using std::string;
using std::vector;

namespace shill {

const int DNSClient::kDefaultTimeoutMS = 2000;
const char DNSClient::kErrorNoData[] = "The query response contains no answers";
const char DNSClient::kErrorFormErr[] = "The server says the query is bad";
const char DNSClient::kErrorServerFail[] = "The server says it had a failure";
const char DNSClient::kErrorNotFound[] = "The queried-for domain was not found";
const char DNSClient::kErrorNotImp[] = "The server doesn't implement operation";
const char DNSClient::kErrorRefused[] = "The server replied, refused the query";
const char DNSClient::kErrorBadQuery[] = "Locally we could not format a query";
const char DNSClient::kErrorNetRefused[] = "The network connection was refused";
const char DNSClient::kErrorTimedOut[] = "The network connection was timed out";
const char DNSClient::kErrorUnknown[] = "DNS Resolver unknown internal error";

// Private to the implementation of resolver so callers don't include ares.h
struct DNSClientState {
  ares_channel channel;
  map< ares_socket_t, std::tr1::shared_ptr<IOHandler> > read_handlers;
  map< ares_socket_t, std::tr1::shared_ptr<IOHandler> > write_handlers;
  struct timeval start_time_;
};

DNSClient::DNSClient(IPAddress::Family family,
                     const string &interface_name,
                     const vector<string> &dns_servers,
                     int timeout_ms,
                     EventDispatcher *dispatcher,
                     Callback1<bool>::Type *callback)
    : address_(IPAddress(family)),
      interface_name_(interface_name),
      dns_servers_(dns_servers),
      dispatcher_(dispatcher),
      callback_(callback),
      timeout_ms_(timeout_ms),
      running_(false),
      resolver_state_(NULL),
      read_callback_(NewCallback(this, &DNSClient::HandleDNSRead)),
      write_callback_(NewCallback(this, &DNSClient::HandleDNSWrite)),
      task_factory_(this),
      ares_(Ares::GetInstance()),
      time_(Time::GetInstance()) {}

DNSClient::~DNSClient() {
  Stop();
}

bool DNSClient::Start(const string &hostname) {
  if (running_) {
    LOG(ERROR) << "Only one DNS request is allowed at a time";
    return false;
  }

  if (!resolver_state_.get()) {
    struct ares_options options;
    memset(&options, 0, sizeof(options));

    vector<struct in_addr> server_addresses;
    for (vector<string>::iterator it = dns_servers_.begin();
         it != dns_servers_.end();
         ++it) {
      struct in_addr addr;
      if (inet_aton(it->c_str(), &addr) != 0) {
        server_addresses.push_back(addr);
      }
    }

    if (server_addresses.empty()) {
      LOG(ERROR) << "No valid DNS server addresses";
      return false;
    }

    options.servers = server_addresses.data();
    options.nservers = server_addresses.size();
    options.timeout = timeout_ms_;

    resolver_state_.reset(new DNSClientState);
    int status = ares_->InitOptions(&resolver_state_->channel,
                                   &options,
                                   ARES_OPT_SERVERS | ARES_OPT_TIMEOUTMS);
    if (status != ARES_SUCCESS) {
      LOG(ERROR) << "ARES initialization returns error code: " << status;
      resolver_state_.reset();
      return false;
    }

    ares_->SetLocalDev(resolver_state_->channel, interface_name_.c_str());
  }

  running_ = true;
  time_->GetTimeOfDay(&resolver_state_->start_time_, NULL);
  error_.clear();
  ares_->GetHostByName(resolver_state_->channel, hostname.c_str(),
                       address_.family(), ReceiveDNSReplyCB, this);

  if (!RefreshHandles()) {
    LOG(ERROR) << "Impossibly short timeout.";
    Stop();
    return false;
  }

  return true;
}

void DNSClient::Stop() {
  if (!resolver_state_.get()) {
    return;
  }

  running_ = false;
  task_factory_.RevokeAll();
  ares_->Destroy(resolver_state_->channel);
  resolver_state_.reset();
}

void DNSClient::HandleDNSRead(int fd) {
  ares_->ProcessFd(resolver_state_->channel, fd, ARES_SOCKET_BAD);
  RefreshHandles();
}

void DNSClient::HandleDNSWrite(int fd) {
  ares_->ProcessFd(resolver_state_->channel, ARES_SOCKET_BAD, fd);
  RefreshHandles();
}

void DNSClient::HandleTimeout() {
  ares_->ProcessFd(resolver_state_->channel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
  if (!RefreshHandles()) {
    // If we have timed out, ARES might still have sockets open.
    // Force them closed by doing an explicit shutdown.  This is
    // different from HandleDNSRead and HandleDNSWrite where any
    // change in our running_ state would be as a result of ARES
    // itself and therefore properly synchronized with it: if a
    // search completes during the course of ares_->ProcessFd(),
    // the ARES fds and other state is guaranteed to have cleaned
    // up and ready for a new request.  Since this timeout is
    // genererated outside of the library it is best to completely
    // shutdown ARES and start with fresh state for a new request.
    Stop();
  }
}

void DNSClient::ReceiveDNSReply(int status, struct hostent *hostent) {
  if (!running_) {
    // We can be called during ARES shutdown -- ignore these events.
    return;
  }
  running_ = false;

  if (status == ARES_SUCCESS &&
      hostent != NULL &&
      hostent->h_addrtype == address_.family() &&
      hostent->h_length == IPAddress::GetAddressLength(address_.family()) &&
      hostent->h_addr_list != NULL &&
      hostent->h_addr_list[0] != NULL) {
    address_ = IPAddress(address_.family(),
                         ByteString(reinterpret_cast<unsigned char *>(
                             hostent->h_addr_list[0]), hostent->h_length));
    callback_->Run(true);
  } else {
    switch (status) {
      case ARES_ENODATA:
        error_ = kErrorNoData;
        break;
      case ARES_EFORMERR:
        error_ = kErrorFormErr;
        break;
      case ARES_ESERVFAIL:
        error_ = kErrorServerFail;
        break;
      case ARES_ENOTFOUND:
        error_ = kErrorNotFound;
        break;
      case ARES_ENOTIMP:
        error_ = kErrorNotImp;
        break;
      case ARES_EREFUSED:
        error_ = kErrorRefused;
        break;
      case ARES_EBADQUERY:
      case ARES_EBADNAME:
      case ARES_EBADFAMILY:
      case ARES_EBADRESP:
        error_ = kErrorBadQuery;
        break;
      case ARES_ECONNREFUSED:
        error_ = kErrorNetRefused;
        break;
      case ARES_ETIMEOUT:
        error_ = kErrorTimedOut;
        break;
      default:
        error_ = kErrorUnknown;
        if (status == ARES_SUCCESS) {
          LOG(ERROR) << "ARES returned success but hostent was invalid!";
        } else {
          LOG(ERROR) << "ARES returned unhandled error status " << status;
        }
        break;
    }
    callback_->Run(false);
  }
}

void DNSClient::ReceiveDNSReplyCB(void *arg, int status,
                                  int /*timeouts*/,
                                  struct hostent *hostent) {
  DNSClient *res = static_cast<DNSClient *>(arg);
  res->ReceiveDNSReply(status, hostent);
}

bool DNSClient::RefreshHandles() {
  map< ares_socket_t, std::tr1::shared_ptr<IOHandler> > old_read =
      resolver_state_->read_handlers;
  map< ares_socket_t, std::tr1::shared_ptr<IOHandler> > old_write =
      resolver_state_->write_handlers;

  resolver_state_->read_handlers.clear();
  resolver_state_->write_handlers.clear();

  ares_socket_t sockets[ARES_GETSOCK_MAXNUM];
  int action_bits = ares_->GetSock(resolver_state_->channel, sockets,
                                 ARES_GETSOCK_MAXNUM);

  for (int i = 0; i < ARES_GETSOCK_MAXNUM; i++) {
    if (ARES_GETSOCK_READABLE(action_bits, i)) {
      if (ContainsKey(old_read, sockets[i])) {
        resolver_state_->read_handlers[sockets[i]] = old_read[sockets[i]];
      } else {
        resolver_state_->read_handlers[sockets[i]] =
            std::tr1::shared_ptr<IOHandler> (
                dispatcher_->CreateReadyHandler(sockets[i],
                                                IOHandler::kModeInput,
                                                read_callback_.get()));
      }
    }
    if (ARES_GETSOCK_WRITABLE(action_bits, i)) {
      if (ContainsKey(old_write, sockets[i])) {
        resolver_state_->write_handlers[sockets[i]] = old_write[sockets[i]];
      } else {
        resolver_state_->write_handlers[sockets[i]] =
            std::tr1::shared_ptr<IOHandler> (
                dispatcher_->CreateReadyHandler(sockets[i],
                                                IOHandler::kModeOutput,
                                                write_callback_.get()));
      }
    }
  }

  if (!running_) {
    // We are here just to clean up socket and timer handles, and the
    // ARES state was cleaned up during the last call to ares_process_fd().
    task_factory_.RevokeAll();
    return false;
  }

  // Schedule timer event for the earlier of our timeout or one requested by
  // the resolver library.
  struct timeval now, elapsed_time, timeout_tv;
  time_->GetTimeOfDay(&now, NULL);
  timersub(&now, &resolver_state_->start_time_, &elapsed_time);
  timeout_tv.tv_sec = timeout_ms_ / 1000;
  timeout_tv.tv_usec = (timeout_ms_ % 1000) * 1000;
  if (timercmp(&elapsed_time, &timeout_tv, >=)) {
    // There are 3 cases of interest:
    //  - If we got here from Start(), we will have the side-effect of
    //    both invoking the callback and returning False in Start().
    //    Start() will call Stop() which will shut down ARES.
    //  - If we got here from the tail of an IO event (racing with the
    //    timer, we can't call Stop() since that will blow away the
    //    IOHandler we are running in, however we will soon be called
    //    again by the timeout proc so we can clean up the ARES state
    //    then.
    //  - If we got here from a timeout handler, it will safely call
    //    Stop() when we return false.
    error_ = kErrorTimedOut;
    callback_->Run(false);
    running_ = false;
    return false;
  } else {
    struct timeval max, ret_tv;
    timersub(&timeout_tv, &elapsed_time, &max);
    struct timeval *tv = ares_->Timeout(resolver_state_->channel,
                                        &max, &ret_tv);
    task_factory_.RevokeAll();
    dispatcher_->PostDelayedTask(
        task_factory_.NewRunnableMethod(&DNSClient::HandleTimeout),
        tv->tv_sec * 1000 + tv->tv_usec / 1000);
  }

  return true;
}

}  // namespace shill
