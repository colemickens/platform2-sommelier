// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This code is derived from the 'iw' source code.  The copyright and license
// of that code is as follows:
//
// Copyright (c) 2007, 2008  Johannes Berg
// Copyright (c) 2007  Andy Lutomirski
// Copyright (c) 2007  Mike Kershaw
// Copyright (c) 2008-2009  Luis R. Rodriguez
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#ifndef SHILL_KERNEL_BOUND_NLMESSAGE_H_
#define SHILL_KERNEL_BOUND_NLMESSAGE_H_

#include "shill/kernel_bound_nlmessage.h"

#include <base/basictypes.h>
#include <base/bind.h>

struct nl_msg;

namespace shill {
struct NetlinkSocket;

// TODO(wdg): eventually, KernelBoundNlMessage and UserBoundNlMessage should
// be combined into a monolithic NlMessage.
//
// Provides a wrapper around a netlink message destined for kernel-space.
class KernelBoundNlMessage {
 public:
  KernelBoundNlMessage() : message_(NULL) {}
  virtual ~KernelBoundNlMessage();

  // Non-trivial initialization.
  bool Init();

  // Message ID is equivalent to the message's sequence number.
  uint32_t GetId() const;

  // Add a netlink header to the message.
  bool AddNetlinkHeader(uint32_t port, uint32_t seq, int family_id, int hdrlen,
                        int flags, uint8_t cmd, uint8_t version);

  // Add a netlink attribute to the message.
  int AddAttribute(int attrtype, int attrlen, const void *data);

  // Sends |this| over the netlink socket.
  virtual bool Send(NetlinkSocket *socket);

 private:
  static const uint32_t kIllegalMessage;

  struct nl_msg *message_;

  DISALLOW_COPY_AND_ASSIGN(KernelBoundNlMessage);
};

}  // namespace shill

#endif  // SHILL_KERNEL_BOUND_NLMESSAGE_H_
