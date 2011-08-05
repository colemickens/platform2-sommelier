// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_RESOLVER_
#define SHILL_RESOLVER_

#include <base/file_path.h>
#include <base/memory/ref_counted.h>
#include <base/memory/singleton.h>

#include "shill/refptr_types.h"

namespace shill {

// This provides a static function for dumping the DNS information out
// of an ipconfig into a "resolv.conf" formatted file.
class Resolver {
 public:
  // Since this is a singleton, use Resolver::GetInstance()->Foo()
  static Resolver *GetInstance();

  void set_path(const FilePath &path) { path_ = path; }

  // Set the default domain name mservice parameters, given an ipconfig entry
  bool SetDNS(const IPConfigRefPtr &ipconfig);

  // Remove any created domain name service file
  bool ClearDNS();

 private:
  friend struct DefaultSingletonTraits<Resolver>;
  friend class ResolverTest;

  // Don't allow this object to be created
  Resolver();
  ~Resolver();

  FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(Resolver);
};

}  // namespace shill

#endif  // SHILL_RESOLVER_
