// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_COMMON_DBUS_SENDER_STUB_H_
#define POWER_MANAGER_COMMON_DBUS_SENDER_STUB_H_

#include <google/protobuf/message_lite.h>

#include "base/memory/scoped_vector.h"
#include "power_manager/common/dbus_sender.h"

namespace power_manager {

// Stub implementation of DBusSenderInterface for testing that just keeps a
// record of signals that it was asked to emit.
class DBusSenderStub : public DBusSenderInterface {
 public:
  // Information about a signal that was sent.
  struct SignalInfo {
    std::string signal_name;
    std::string protobuf_type;
    std::string serialized_data;
  };

  DBusSenderStub();
  virtual ~DBusSenderStub();

  size_t num_sent_signals() { return sent_signals_.size(); }

  // Copies the signal at position |index| in |sent_signals_| (that is, the
  // |index|th-sent signal) to |protobuf|, which should be a concrete protocol
  // buffer.  false is returned if the index is out-of-range, the D-Bus signal
  // name doesn't match |expected_signal_name|, or the type of protocol buffer
  // that was attached to the signal doesn't match |protobuf|'s type.
  // |protobuf| can be NULL, in which case only the signal name is checked.
  bool GetSentSignal(size_t index,
                     const std::string& expected_signal_name,
                     google::protobuf::MessageLite* protobuf);

  // Clears |sent_signals_|.
  void ClearSentSignals();

  // DBusSenderInterface override:
  virtual void EmitBareSignal(const std::string& signal_name) OVERRIDE;
  virtual void EmitSignalWithProtocolBuffer(
      const std::string& signal_name,
      const google::protobuf::MessageLite& protobuf) OVERRIDE;

 private:
  ScopedVector<SignalInfo> sent_signals_;

  DISALLOW_COPY_AND_ASSIGN(DBusSenderStub);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_COMMON_DBUS_SENDER_STUB_H_
