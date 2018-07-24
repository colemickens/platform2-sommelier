// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/cellular_error.h"

#include <chromeos/errors/error_codes.h>
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

  CellularError::FromChromeosDBusError(nullptr, nullptr);
  EXPECT_TRUE(shill_error.IsSuccess());

  {
    chromeos::ErrorPtr dbus_error =
        chromeos::Error::Create(FROM_HERE,
                                chromeos::errors::dbus::kDomain,
                                kErrorIncorrectPasswordMM,
                                kErrorMessage);
    CellularError::FromChromeosDBusError(dbus_error.get(), &shill_error);
    EXPECT_EQ(Error::kIncorrectPin, shill_error.type());
  }
  {
    chromeos::ErrorPtr dbus_error =
        chromeos::Error::Create(FROM_HERE,
                                chromeos::errors::dbus::kDomain,
                                kErrorSimPinRequiredMM,
                                kErrorMessage);
    CellularError::FromChromeosDBusError(dbus_error.get(), &shill_error);
    EXPECT_EQ(Error::kPinRequired, shill_error.type());
  }
  {
    chromeos::ErrorPtr dbus_error =
        chromeos::Error::Create(FROM_HERE,
                                chromeos::errors::dbus::kDomain,
                                kErrorSimPukRequiredMM,
                                kErrorMessage);
    CellularError::FromChromeosDBusError(dbus_error.get(), &shill_error);
    EXPECT_EQ(Error::kPinBlocked, shill_error.type());
  }
  {
    chromeos::ErrorPtr dbus_error =
        chromeos::Error::Create(FROM_HERE,
                                chromeos::errors::dbus::kDomain,
                                kErrorGprsNotSubscribedMM,
                                kErrorMessage);
    CellularError::FromChromeosDBusError(dbus_error.get(), &shill_error);
    EXPECT_EQ(Error::kInvalidApn, shill_error.type());
  }
  {
    chromeos::ErrorPtr dbus_error =
        chromeos::Error::Create(FROM_HERE,
                                chromeos::errors::dbus::kDomain,
                                kErrorIncorrectPasswordMM1,
                                kErrorMessage);
    CellularError::FromChromeosDBusError(dbus_error.get(), &shill_error);
    EXPECT_EQ(Error::kOperationFailed, shill_error.type());
  }
  {
    chromeos::ErrorPtr dbus_error =
        chromeos::Error::Create(FROM_HERE,
                                chromeos::errors::dbus::kDomain,
                                "Some random error name.",
                                kErrorMessage);
    CellularError::FromChromeosDBusError(dbus_error.get(), &shill_error);
    EXPECT_EQ(Error::kOperationFailed, shill_error.type());
  }
}

TEST_F(CellularErrorTest, FromMM1DBusError) {
  Error shill_error;

  CellularError::FromMM1ChromeosDBusError(nullptr, &shill_error);
  EXPECT_TRUE(shill_error.IsSuccess());

  {
    chromeos::ErrorPtr dbus_error =
        chromeos::Error::Create(FROM_HERE,
                                chromeos::errors::dbus::kDomain,
                                kErrorIncorrectPasswordMM1,
                                kErrorMessage);
    CellularError::FromMM1ChromeosDBusError(dbus_error.get(), &shill_error);
    EXPECT_EQ(Error::kIncorrectPin, shill_error.type());
  }
  {
    chromeos::ErrorPtr dbus_error =
        chromeos::Error::Create(FROM_HERE,
                                chromeos::errors::dbus::kDomain,
                                kErrorSimPinMM1,
                                kErrorMessage);
    CellularError::FromMM1ChromeosDBusError(dbus_error.get(), &shill_error);
    EXPECT_EQ(Error::kPinRequired, shill_error.type());
  }
  {
    chromeos::ErrorPtr dbus_error =
        chromeos::Error::Create(FROM_HERE,
                                chromeos::errors::dbus::kDomain,
                                kErrorSimPukMM1,
                                kErrorMessage);
    CellularError::FromMM1ChromeosDBusError(dbus_error.get(), &shill_error);
    EXPECT_EQ(Error::kPinBlocked, shill_error.type());
  }
  {
    chromeos::ErrorPtr dbus_error =
        chromeos::Error::Create(FROM_HERE,
                                chromeos::errors::dbus::kDomain,
                                kErrorGprsNotSubscribedMM1,
                                kErrorMessage);
    CellularError::FromMM1ChromeosDBusError(dbus_error.get(), &shill_error);
    EXPECT_EQ(Error::kInvalidApn, shill_error.type());
  }
  {
    chromeos::ErrorPtr dbus_error =
        chromeos::Error::Create(FROM_HERE,
                                chromeos::errors::dbus::kDomain,
                                kErrorWrongStateMM1,
                                kErrorMessage);
    CellularError::FromMM1ChromeosDBusError(dbus_error.get(), &shill_error);
    EXPECT_EQ(Error::kWrongState, shill_error.type());
  }
  {
    chromeos::ErrorPtr dbus_error =
        chromeos::Error::Create(FROM_HERE,
                                chromeos::errors::dbus::kDomain,
                                kErrorIncorrectPasswordMM,
                                kErrorMessage);
    CellularError::FromMM1ChromeosDBusError(dbus_error.get(), &shill_error);
    EXPECT_EQ(Error::kOperationFailed, shill_error.type());
  }
  {
    chromeos::ErrorPtr dbus_error =
        chromeos::Error::Create(FROM_HERE,
                                chromeos::errors::dbus::kDomain,
                                "Some random error name.",
                                kErrorMessage);
    CellularError::FromMM1ChromeosDBusError(dbus_error.get(), &shill_error);
    EXPECT_EQ(Error::kOperationFailed, shill_error.type());
  }
}

}  // namespace shill

