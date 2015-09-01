// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_EXAMPLES_UBUNTU_AVAHI_CLIENT_H_
#define LIBWEAVE_EXAMPLES_UBUNTU_AVAHI_CLIENT_H_

#include <map>
#include <string>

#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/thread-watch.h>

#include <weave/mdns.h>

namespace weave {
namespace examples {

// Example of weave::Mdns implemented with avahi.
class MdnsImpl : public Mdns {
 public:
  MdnsImpl();

  ~MdnsImpl() override;
  void PublishService(const std::string& service_name,
                      uint16_t port,
                      const std::map<std::string, std::string>& txt) override;
  void StopPublishing(const std::string& service_name) override;
  std::string GetId() const override;

  uint16_t prev_port_{0};
  std::string prev_type_;

  std::unique_ptr<AvahiThreadedPoll, decltype(&avahi_threaded_poll_free)>
      thread_pool_{nullptr, &avahi_threaded_poll_free};

  std::unique_ptr<AvahiClient, decltype(&avahi_client_free)> client_{
      nullptr, &avahi_client_free};

  std::unique_ptr<AvahiEntryGroup, decltype(&avahi_entry_group_free)> group_{
      nullptr, &avahi_entry_group_free};
};

}  // namespace examples
}  // namespace weave

#endif  // LIBWEAVE_EXAMPLES_UBUNTU_AVAHI_CLIENT_H_
