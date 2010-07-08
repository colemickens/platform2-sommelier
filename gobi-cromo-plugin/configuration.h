// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLUGIN_CONFIGURATION_H_
#define PLUGIN_CONFIGURATION_H_

#include <string>
#include <base/basictypes.h>
#include <gtest/gtest_prod.h>  // For FRIEND_TEST

class Configuration {
 public:
  // NB: This is a general interface, but currently only one key,
  // "carrier" is supported
  static const char kCarrierKey[];
  static const char kDefaultConfigFile[];

  Configuration() {};
  virtual ~Configuration() {};

  // Load specified file and return success
  bool Init(const char *filename);
  // Return queried key or empty string if no value
  std::string GetValueString(const char *key);
  // Set key and attempt to persist settings if necessary
  void SetValueString(const char *key, const char *value);

 protected:
  bool Write();
  bool Read();
  std::string filename_;
  std::string value_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Configuration);
};

#endif  // PLUGIN_CONFIGURATION_H_
