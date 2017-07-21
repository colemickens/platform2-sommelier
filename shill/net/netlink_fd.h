//
// Copyright 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef SHILL_NET_NETLINK_FD_H_
#define SHILL_NET_NETLINK_FD_H_

namespace shill {

class Sockets;

// Keep this large enough to avoid overflows on IPv6 SNM routing update spikes
constexpr int kNetlinkReceiveBufferSize = 512 * 1024;

int OpenNetlinkSocketFD(Sockets* sockets,
                        int netlink_family,
                        int netlink_groups_mask);

}  // namespace shill

#endif  // SHILL_NET_NETLINK_FD_H_
