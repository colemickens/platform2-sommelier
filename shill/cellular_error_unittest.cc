// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_error.h"

#include <dbus-c++/error.h>
#include <gtest/gtest.h>

namespace shill {

class CellularErrorTest : public testing::Test {
};

namespace {

const char kErrorIncorrectPasswordMM[] =
    "org.freedesktop.ModemManager.Modem.Gsm.IncorrectPassword";

const char kErrorSimPinRequiredMM[] =
    "org.freedesktop.ModemManager.Modem.Gsm.SimPinRequired";

const char kErrorSimPukRequiredMM[] =
    "org.freedesktop.ModemManager.Modem.Gsm.SimPukRequired";

const char kErrorGprsNotSubscribedMM[] =
    "org.freedesktop.ModemManager.Modem.Gsm.GprsNotSubscribed";

const char kErrorIncorrectPasswordMM1[] =
    "org.freedesktop.ModemManager1.Error.MobileEquipment.IncorrectPassword";

const char kErrorSimPinMM1[] =
    "org.freedesktop.ModemManager1.Error.MobileEquipment.SimPin";

const char kErrorSimPukMM1[] =
    "org.freedesktop.ModemManager1.Error.MobileEquipment.SimPuk";

const char kErrorGprsNotSubscribedMM1[] =
    "org.freedesktop.ModemManager1.Error.MobileEquipment."
    "GprsServiceOptionNotSubscribed";

const char kErrorWrongStateMM1[] =
    "org.freedesktop.ModemManager1.Error.Core.WrongState";


const char kErrorMessage[] = "Some error message.";

}  // namespace

TEST_F(CellularErrorTest, FromDBusError) {
  Error shill_error;

  CellularError::FromDBusError(DBus::Error(), nullptr);
  EXPECT_TRUE(shill_error.IsSuccess());

  CellularError::FromDBusError(DBus::Error(), &shill_error);
  EXPECT_TRUE(shill_error.IsSuccess());

  CellularError::FromDBusError(
      DBus::Error(kErrorIncorrectPasswordMM, kErrorMessage),
      &shill_error);
  EXPECT_EQ(Error::kIncorrectPin, shill_error.type());

  CellularError::FromDBusError(
      DBus::Error(kErrorSimPinRequiredMM, kErrorMessage),
      &shill_error);
  EXPECT_EQ(Error::kPinRequired, shill_error.type());

  CellularError::FromDBusError(
      DBus::Error(kErrorSimPukRequiredMM, kErrorMessage),
      &shill_error);
  EXPECT_EQ(Error::kPinBlocked, shill_error.type());

  CellularError::FromDBusError(
      DBus::Error(kErrorGprsNotSubscribedMM, kErrorMessage),
      &shill_error);
  EXPECT_EQ(Error::kInvalidApn, shill_error.type());

  CellularError::FromDBusError(
      DBus::Error(kErrorIncorrectPasswordMM1, kErrorMessage),
      &shill_error);
  EXPECT_EQ(Error::kOperationFailed, shill_error.type());

  CellularError::FromDBusError(
      DBus::Error("Some random error name.", kErrorMessage),
      &shill_error);
  EXPECT_EQ(Error::kOperationFailed, shill_error.type());
}

TEST_F(CellularErrorTest, FromMM1DBusError) {
  Error shill_error;

  CellularError::FromDBusError(DBus::Error(), nullptr);
  EXPECT_TRUE(shill_error.IsSuccess());

  CellularError::FromMM1DBusError(DBus::Error(), &shill_error);
  EXPECT_TRUE(shill_error.IsSuccess());

  CellularError::FromMM1DBusError(
      DBus::Error(kErrorIncorrectPasswordMM1, kErrorMessage),
      &shill_error);
  EXPECT_EQ(Error::kIncorrectPin, shill_error.type());

  CellularError::FromMM1DBusError(
      DBus::Error(kErrorSimPinMM1, kErrorMessage),
      &shill_error);
  EXPECT_EQ(Error::kPinRequired, shill_error.type());

  CellularError::FromMM1DBusError(
      DBus::Error(kErrorSimPukMM1, kErrorMessage),
      &shill_error);
  EXPECT_EQ(Error::kPinBlocked, shill_error.type());

  CellularError::FromMM1DBusError(
      DBus::Error(kErrorGprsNotSubscribedMM1, kErrorMessage),
      &shill_error);
  EXPECT_EQ(Error::kInvalidApn, shill_error.type());

  CellularError::FromMM1DBusError(
      DBus::Error(kErrorWrongStateMM1, kErrorMessage),
      &shill_error);
  EXPECT_EQ(Error::kWrongState, shill_error.type());

  CellularError::FromMM1DBusError(
      DBus::Error(kErrorIncorrectPasswordMM, kErrorMessage),
      &shill_error);
  EXPECT_EQ(Error::kOperationFailed, shill_error.type());

  CellularError::FromMM1DBusError(
      DBus::Error("Some random error name.", kErrorMessage),
      &shill_error);
  EXPECT_EQ(Error::kOperationFailed, shill_error.type());
}

}  // namespace shill

