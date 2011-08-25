// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROPERTY_STORE_UNITTEST_H_
#define SHILL_PROPERTY_STORE_UNITTEST_H_

#include <map>
#include <string>
#include <vector>

#include <base/memory/scoped_ptr.h>
#include <base/memory/scoped_temp_dir.h>
#include <dbus-c++/dbus.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/dbus_adaptor.h"
#include "shill/error.h"
#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/mock_glib.h"
#include "shill/property_store.h"
#include "shill/shill_event.h"

namespace shill {

class PropertyStoreTest : public testing::TestWithParam< ::DBus::Variant > {
 public:
  // In real code, it's frowned upon to have non-POD static members, as there
  // can be ordering issues if your constructors have side effects.
  // These constructors don't, and declaring these as static lets me
  // autogenerate a bunch of unit test code that I would otherwise need to
  // copypasta.  So I think it's safe and worth it.
  static const ::DBus::Variant kBoolV;
  static const ::DBus::Variant kByteV;
  static const ::DBus::Variant kInt16V;
  static const ::DBus::Variant kInt32V;
  static const ::DBus::Variant kStringV;
  static const ::DBus::Variant kStringmapV;
  static const ::DBus::Variant kStringmapsV;
  static const ::DBus::Variant kStringsV;
  static const ::DBus::Variant kStrIntPairV;
  static const ::DBus::Variant kUint16V;
  static const ::DBus::Variant kUint32V;

  PropertyStoreTest();
  virtual ~PropertyStoreTest();

  virtual void SetUp();

 protected:
  Manager *manager() { return &manager_; }

  const std::string &run_path() const { return path_; }
  const std::string &storage_path() const { return path_; }

  const std::string &invalid_args() const { return invalid_args_; }
  const std::string &invalid_prop() const { return invalid_prop_; }

  // TODO(cmasone): make these private as per http://crosbug.com/19573
  MockControl control_interface_;
  EventDispatcher dispatcher_;
  MockGLib glib_;

 private:
  const std::string invalid_args_;
  const std::string invalid_prop_;
  ScopedTempDir dir_;
  const std::string path_;
  Manager manager_;

};

}  // namespace shill
#endif  // SHILL_PROPERTY_STORE_UNITTEST_H_
