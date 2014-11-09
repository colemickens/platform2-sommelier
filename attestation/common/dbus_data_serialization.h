// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATTESTATION_COMMON_DBUS_DATA_SERIALIZATION_H_
#define ATTESTATION_COMMON_DBUS_DATA_SERIALIZATION_H_

#include <chromeos/dbus/data_serialization.h>

#include "attestation/common/dbus_interface.pb.h"

// These methods must be in chromeos::dbus_utils namespace since we are
// specializing DBusSignature template structure already defined in
// chromeos/dbus/data_serialization.h.
namespace chromeos {
namespace dbus_utils {

inline bool AppendValueToWriter(dbus::MessageWriter* writer,
                                const attestation::StatsResponse& value) {
  writer->AppendProtoAsArrayOfBytes(value);
  return true;
}

inline bool PopValueFromReader(dbus::MessageReader* reader,
                               attestation::StatsResponse* value) {
  return reader->PopArrayOfBytesAsProto(value);
}

// Specializations/overloads to send "StatsResponse" structure over D-Bus.
template<>
struct DBusType<attestation::StatsResponse>
    : public DBusType<google::protobuf::MessageLite> {};


}  // namespace dbus_utils
}  // namespace chromeos

#endif  // ATTESTATION_COMMON_DBUS_DATA_SERIALIZATION_H_
