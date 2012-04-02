// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_NSS_
#define SHILL_NSS_

#include <string>
#include <vector>

#include <base/file_path.h>
#include <base/lazy_instance.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

namespace shill {

class GLib;

class NSS {
 public:
  virtual ~NSS();

  // This is a singleton -- use NSS::GetInstance()->Foo()
  static NSS *GetInstance();

  void Init(GLib *glib);

  // Returns an empty path on failure.
  virtual FilePath GetPEMCertfile(const std::string &nickname,
                                  const std::vector<char> &id);

  // Returns an empty path on failure.
  virtual FilePath GetDERCertfile(const std::string &nickname,
                                  const std::vector<char> &id);

 protected:
  NSS();

 private:
  friend struct base::DefaultLazyInstanceTraits<NSS>;
  friend class NSSTest;
  FRIEND_TEST(NSSTest, GetCertfile);

  FilePath GetCertfile(const std::string &nickname,
                       const std::vector<char> &id,
                       const std::string &type);

  GLib *glib_;

  DISALLOW_COPY_AND_ASSIGN(NSS);
};

}  // namespace shill

#endif  // SHILL_NSS_
