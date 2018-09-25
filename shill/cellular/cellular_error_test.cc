// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/cellular_error.h"

#include <brillo/errors/error_codes.h>
#include <gtest/gtest.h>

namespace shill {
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

struct TestParam {
  TestParam(const char* dbus_error, Error::Type error_type)
      : dbus_error(dbus_error), error_type(error_type) {}

  const char* dbus_error;
  Error::Type error_type;
};

class CellularErrorTest : public testing::TestWithParam<TestParam> {};

TEST_P(CellularErrorTest, FromDBusError) {
  TestParam param = GetParam();

  brillo::ErrorPtr dbus_error =
      brillo::Error::Create(FROM_HERE,
                            brillo::errors::dbus::kDomain,
                            param.dbus_error,
                            kErrorMessage);
  Error shill_error;
  CellularError::FromChromeosDBusError(dbus_error.get(), &shill_error);
  EXPECT_EQ(param.error_type, shill_error.type());
}

INSTANTIATE_TEST_CASE_P(
    CellularErrorTest,
    CellularErrorTest,
    testing::Values(
        TestParam(kErrorIncorrectPasswordMM, Error::kIncorrectPin),
        TestParam(kErrorSimPinRequiredMM, Error::kPinRequired),
        TestParam(kErrorSimPukRequiredMM, Error::kPinBlocked),
        TestParam(kErrorGprsNotSubscribedMM, Error::kInvalidApn),
        TestParam(kErrorIncorrectPasswordMM1, Error::kOperationFailed),
        TestParam("Some random error name.", Error::kOperationFailed)));


class CellularErrorMM1Test : public testing::TestWithParam<TestParam> {};

TEST_P(CellularErrorMM1Test, FromDBusError) {
  TestParam param = GetParam();

  brillo::ErrorPtr dbus_error =
      brillo::Error::Create(FROM_HERE,
                            brillo::errors::dbus::kDomain,
                            param.dbus_error,
                            kErrorMessage);
  Error shill_error;
  CellularError::FromMM1ChromeosDBusError(dbus_error.get(), &shill_error);
  EXPECT_EQ(param.error_type, shill_error.type());
}

INSTANTIATE_TEST_CASE_P(
    CellularErrorMM1Test,
    CellularErrorMM1Test,
    testing::Values(
        TestParam(kErrorIncorrectPasswordMM1, Error::kIncorrectPin),
        TestParam(kErrorSimPinMM1, Error::kPinRequired),
        TestParam(kErrorSimPukMM1, Error::kPinBlocked),
        TestParam(kErrorGprsNotSubscribedMM1, Error::kInvalidApn),
        TestParam(kErrorWrongStateMM1, Error::kWrongState),
        TestParam(kErrorIncorrectPasswordMM, Error::kOperationFailed),
        TestParam("Some random error name.", Error::kOperationFailed)));

}  // namespace
}  // namespace shill
