// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DHCP_PROVIDER_
#define SHILL_DHCP_PROVIDER_

#include <base/memory/singleton.h>

namespace shill {

class DHCPProvider {
 public:
  // This is a singleton -- use DHCPProvider::GetInstance()->Foo()
  static DHCPProvider *GetInstance();

 private:
  friend struct DefaultSingletonTraits<DHCPProvider>;

  DHCPProvider();
  virtual ~DHCPProvider();

  DISALLOW_COPY_AND_ASSIGN(DHCPProvider);
};

}  // namespace shill

#endif  // SHILL_DHCP_PROVIDER_
