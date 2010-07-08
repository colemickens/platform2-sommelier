// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "configuration.h"
#include <fstream>
#include <iostream>

#include <glog/logging.h>

const char Configuration::kCarrierKey[] = "carrier";
const char Configuration::kDefaultConfigFile[] = "/var/run/cromo/gobi-carrier";

bool Configuration::Init(const char *filename) {
  filename_ = filename;
  return Read();
}

bool Configuration::Read() {
  std::ifstream configuration(filename_.c_str(), std::ios_base::in);
  std::string tmp;
  std::getline(configuration, tmp);
  if (configuration.fail()) {
    PLOG(ERROR) << "Could not read config file " << filename_;
  } else {
    value_ = tmp;
  }
  return !configuration.fail();
}

bool Configuration::Write() {
  std::ofstream configuration(filename_.c_str(),
                              (std::ios_base::trunc |
                               std::ios_base::out));
  if (configuration.fail()) {
    PLOG(ERROR) << "Could not open file for write " << filename_;
    goto done;
   }

  configuration.write(value_.c_str(), value_.length());
  if (configuration.fail()) {
    PLOG(ERROR) << "Could not write to file " << filename_;
    goto done;
  }
done:
  configuration.close();
  if (configuration.fail()) {
    PLOG(ERROR) << "Could not close file " << filename_;
  }
  return !configuration.fail();
}


std::string Configuration::GetValueString(const char *key) {
  if (0 != strcmp(key, kCarrierKey)) {
    LOG(FATAL) << "Invalid key to GetValueString: " << key;
    return "";
  }
  return value_;
}

void Configuration::SetValueString(const char *key, const char *value_to_set) {
  if (0 != strcmp(key, kCarrierKey)) {
    LOG(FATAL) << "Invalid key to SetValueString: " << key;
    return;
  }
  if (value_ != value_to_set) {
    value_ = value_to_set;
    Write();
  }
}
