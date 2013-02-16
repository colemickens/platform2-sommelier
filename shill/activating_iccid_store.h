// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ACTIVATING_ICCID_STORE_H_
#define SHILL_ACTIVATING_ICCID_STORE_H_

#include <string>

#include <base/file_path.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/key_file_store.h"

namespace shill {

class GLib;
class StoreInterface;

// ActivatingIccidStore stores the network activation status for a
// particular SIM. Once an online payment for the activation of a 3GPP
// network is successful, the associated SIM is regarded as pending
// activation and stored in the persistent profile. Once shill knows that
// the activation associated with a particular SIM is successful, it is removed
// from the profile and the cellular service is marked as activated.
class ActivatingIccidStore {
 public:
  enum State {
    // This state indicates that information for a articular SIM was never
    // stored in this database.
    kStateUnknown,
    // This state indicates that an online payment has been made but the modem
    // has not yet been able to register with the network.
    kStatePending,
    // This state indicates that the modem has registered with the network but
    // the network has not yet confirmed that the service has been activated.
    // Currently, shill knows that activation has gone through, when a non-zero
    // MDN has been received OTA.
    kStateActivated,

    kStateMax,
  };

  // Constructor performs no initialization.
  ActivatingIccidStore();
  virtual ~ActivatingIccidStore();

  // Tries to open the underlying store interface from the given file path.
  // Returns false if it fails to open the file.
  //
  // If called more than once on the same instance, the file that was already
  // open will allways be flushed and closed, however it is not guaranteed that
  // the file will always be successfully reopened (technically it should, but
  // it is not guaranteed).
  virtual bool InitStorage(GLib *glib, const FilePath &storage_path);

  // Returns the activation state for a SIM with the given ICCID. A return value
  // of kStateUnknown indicates that the given ICCID was not found.
  virtual State GetActivationState(const std::string &iccid) const;

  // Sets the activation state for the given ICCID. If an entry for this iccid
  // was not found, a new entry will be created. Returns true on success.
  virtual bool SetActivationState(const std::string &iccid, State state);

  // Removes the entry for the given ICCID from the database. Returns true if
  // the operation was successful. If the iccid did not exist in the database,
  // still returns true.
  virtual bool RemoveEntry(const std::string &iccid);

 private:
  friend class ActivatingIccidStoreTest;
  friend class CellularCapabilityUniversalTest;
  FRIEND_TEST(ActivatingIccidStoreTest, FileInteractions);
  FRIEND_TEST(ActivatingIccidStoreTest, GetActivationState);
  FRIEND_TEST(ActivatingIccidStoreTest, RemoveEntry);
  FRIEND_TEST(ActivatingIccidStoreTest, SetActivationState);

  static const char kGroupId[];
  static const char kStorageFileName[];

  scoped_ptr<StoreInterface> storage_;

  DISALLOW_COPY_AND_ASSIGN(ActivatingIccidStore);
};

}  // namespace shill

#endif  // SHILL_ACTIVATING_ICCID_STORE_H_
