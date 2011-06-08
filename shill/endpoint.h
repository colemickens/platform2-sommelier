// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ENDPOINT_
#define SHILL_ENDPOINT_

#include <base/memory/ref_counted.h>

namespace shill {

class Endpoint;

class Endpoint : public base::RefCounted<Endpoint> {
 public:
  Endpoint();
  virtual ~Endpoint();

 private:
  DISALLOW_COPY_AND_ASSIGN(Endpoint);
};

}  // namespace shill

#endif  // SHILL_ENDPOINT_
