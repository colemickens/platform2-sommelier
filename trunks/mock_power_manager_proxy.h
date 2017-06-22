//
// Copyright (C) 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef TRUNKS_MOCK_POWER_MANAGER_PROXY_H_
#define TRUNKS_MOCK_POWER_MANAGER_PROXY_H_

#include <gmock/gmock.h>
#include <power_manager-client/power_manager/dbus-proxies.h>

namespace trunks {

class MockPowerManagerProxy : public org::chromium::PowerManagerProxyInterface {
 public:
  MockPowerManagerProxy(dbus::ObjectProxy* object_proxy = nullptr)
    : object_proxy_(object_proxy) {}

  void set_object_proxy(dbus::ObjectProxy* object_proxy) {
    object_proxy_ = object_proxy;
  }

  dbus::ObjectProxy* GetObjectProxy() const { return object_proxy_; }

  MOCK_CONST_METHOD0(GetObjectPath,
                     const dbus::ObjectPath&());
  MOCK_METHOD2(RequestShutdown,
               bool(brillo::ErrorPtr*,
                    int));
  MOCK_METHOD3(RequestShutdownAsync,
               void(const base::Callback<void()>&,
                    const base::Callback<void(brillo::Error*)>&,
                    int));
  MOCK_METHOD3(RequestRestart,
               bool(int32_t,
                    brillo::ErrorPtr*,
                    int));
  MOCK_METHOD4(RequestRestartAsync,
               void(int32_t,
                    const base::Callback<void()>&,
                    const base::Callback<void(brillo::Error*)>&,
                    int));
  MOCK_METHOD3(RequestSuspend,
               bool(uint64_t,
                    brillo::ErrorPtr*,
                    int));
  MOCK_METHOD4(RequestSuspendAsync,
               void(uint64_t,
                    const base::Callback<void()>&,
                    const base::Callback<void(brillo::Error*)>&,
                    int));
  MOCK_METHOD3(DecreaseScreenBrightness,
               bool(bool,
                    brillo::ErrorPtr*,
                    int));
  MOCK_METHOD4(DecreaseScreenBrightnessAsync,
               void(bool,
                    const base::Callback<void()>&,
                    const base::Callback<void(brillo::Error*)>&,
                    int));
  MOCK_METHOD2(IncreaseScreenBrightness,
               bool(brillo::ErrorPtr*,
                    int));
  MOCK_METHOD3(IncreaseScreenBrightnessAsync,
               void(const base::Callback<void()>&,
                    const base::Callback<void(brillo::Error*)>&,
                    int));
  MOCK_METHOD3(GetScreenBrightnessPercent,
               bool(double*,
                    brillo::ErrorPtr*,
                    int));
  MOCK_METHOD3(GetScreenBrightnessPercentAsync,
               void(const base::Callback<void(double)>&,
                    const base::Callback<void(brillo::Error*)>&,
                    int));
  MOCK_METHOD4(SetScreenBrightnessPercent,
               bool(double,
                    int32_t,
                    brillo::ErrorPtr*,
                    int));
  MOCK_METHOD5(SetScreenBrightnessPercentAsync,
               void(double,
                    int32_t,
                    const base::Callback<void()>&,
                    const base::Callback<void(brillo::Error*)>&,
                    int));
  MOCK_METHOD2(DecreaseKeyboardBrightness,
               bool(brillo::ErrorPtr*,
                    int));
  MOCK_METHOD3(DecreaseKeyboardBrightnessAsync,
               void(const base::Callback<void()>&,
                    const base::Callback<void(brillo::Error*)>&,
                    int));
  MOCK_METHOD2(IncreaseKeyboardBrightness,
               bool(brillo::ErrorPtr*,
                    int));
  MOCK_METHOD3(IncreaseKeyboardBrightnessAsync,
               void(const base::Callback<void()>&,
                    const base::Callback<void(brillo::Error*)>&,
                    int));
  MOCK_METHOD3(GetPowerSupplyProperties,
               bool(std::vector<uint8_t>*,
                    brillo::ErrorPtr*,
                    int));
  MOCK_METHOD3(GetPowerSupplyPropertiesAsync,
               void(const base::Callback<void(const std::vector<uint8_t>&)>&,
                    const base::Callback<void(brillo::Error*)>&,
                    int));
  MOCK_METHOD3(HandleVideoActivity,
               bool(bool,
                    brillo::ErrorPtr*,
                    int));
  MOCK_METHOD4(HandleVideoActivityAsync,
               void(bool,
                    const base::Callback<void()>&,
                    const base::Callback<void(brillo::Error*)>&,
                    int));
  MOCK_METHOD3(HandleUserActivity,
               bool(int32_t,
                    brillo::ErrorPtr*,
                    int));
  MOCK_METHOD4(HandleUserActivityAsync,
               void(int32_t,
                    const base::Callback<void()>&,
                    const base::Callback<void(brillo::Error*)>&,
                    int));
  MOCK_METHOD3(SetIsProjecting,
               bool(bool,
                    brillo::ErrorPtr*,
                    int));
  MOCK_METHOD4(SetIsProjectingAsync,
               void(bool,
                    const base::Callback<void()>&,
                    const base::Callback<void(brillo::Error*)>&,
                    int));
  MOCK_METHOD3(SetPolicy,
               bool(const std::vector<uint8_t>&,
                    brillo::ErrorPtr*,
                    int));
  MOCK_METHOD4(SetPolicyAsync,
               void(const std::vector<uint8_t>&,
                    const base::Callback<void()>&,
                    const base::Callback<void(brillo::Error*)>&,
                    int));
  MOCK_METHOD3(SetPowerSource,
               bool(const std::string&,
                    brillo::ErrorPtr*,
                    int));
  MOCK_METHOD4(SetPowerSourceAsync,
               void(const std::string&,
                    const base::Callback<void()>&,
                    const base::Callback<void(brillo::Error*)>&,
                    int));
  MOCK_METHOD3(HandlePowerButtonAcknowledgment,
               bool(int64_t,
                    brillo::ErrorPtr*,
                    int));
  MOCK_METHOD4(HandlePowerButtonAcknowledgmentAsync,
               void(int64_t,
                    const base::Callback<void()>&,
                    const base::Callback<void(brillo::Error*)>&,
                    int));
  MOCK_METHOD3(IgnoreNextPowerButtonPress,
               bool(int64_t,
                    brillo::ErrorPtr*,
                    int));
  MOCK_METHOD4(IgnoreNextPowerButtonPressAsync,
               void(int64_t,
                    const base::Callback<void()>&,
                    const base::Callback<void(brillo::Error*)>&,
                    int));
  MOCK_METHOD4(RegisterSuspendDelay,
               bool(const std::vector<uint8_t>&,
                    std::vector<uint8_t>*,
                    brillo::ErrorPtr*,
                    int));
  MOCK_METHOD4(RegisterSuspendDelayAsync,
               void(const std::vector<uint8_t>&,
                    const base::Callback<void(const std::vector<uint8_t>&)>&,
                    const base::Callback<void(brillo::Error*)>&,
                    int));
  MOCK_METHOD3(UnregisterSuspendDelay,
               bool(const std::vector<uint8_t>&,
                    brillo::ErrorPtr*,
                    int));
  MOCK_METHOD4(UnregisterSuspendDelayAsync,
               void(const std::vector<uint8_t>&,
                    const base::Callback<void()>&,
                    const base::Callback<void(brillo::Error*)>&,
                    int));
  MOCK_METHOD3(HandleSuspendReadiness,
               bool(const std::vector<uint8_t>&,
                    brillo::ErrorPtr*,
                    int));
  MOCK_METHOD4(HandleSuspendReadinessAsync,
               void(const std::vector<uint8_t>&,
                    const base::Callback<void()>&,
                    const base::Callback<void(brillo::Error*)>&,
                    int));
  MOCK_METHOD4(RegisterDarkSuspendDelay,
               bool(const std::vector<uint8_t>&,
                    std::vector<uint8_t>*,
                    brillo::ErrorPtr*,
                    int));
  MOCK_METHOD4(RegisterDarkSuspendDelayAsync,
               void(const std::vector<uint8_t>&,
                    const base::Callback<void(const std::vector<uint8_t>&)>&,
                    const base::Callback<void(brillo::Error*)>&,
                    int));
  MOCK_METHOD3(UnregisterDarkSuspendDelay,
               bool(const std::vector<uint8_t>&,
                    brillo::ErrorPtr*,
                    int));
  MOCK_METHOD4(UnregisterDarkSuspendDelayAsync,
               void(const std::vector<uint8_t>&,
                    const base::Callback<void()>&,
                    const base::Callback<void(brillo::Error*)>&,
                    int));
  MOCK_METHOD3(HandleDarkSuspendReadiness,
               bool(const std::vector<uint8_t>&,
                    brillo::ErrorPtr*,
                    int));
  MOCK_METHOD4(HandleDarkSuspendReadinessAsync,
               void(const std::vector<uint8_t>&,
                    const base::Callback<void()>&,
                    const base::Callback<void(brillo::Error*)>&,
                    int));
  MOCK_METHOD3(RecordDarkResumeWakeReason,
               bool(const std::vector<uint8_t>&,
                    brillo::ErrorPtr*,
                    int));
  MOCK_METHOD4(RecordDarkResumeWakeReasonAsync,
               void(const std::vector<uint8_t>&,
                    const base::Callback<void()>&,
                    const base::Callback<void(brillo::Error*)>&,
                    int));
  MOCK_METHOD2(RegisterBrightnessChangedSignalHandler,
               void(const base::Callback<void(int32_t,
                                              bool)>&,
                    dbus::ObjectProxy::OnConnectedCallback));
  MOCK_METHOD2(RegisterKeyboardBrightnessChangedSignalHandler,
               void(const base::Callback<void(int32_t,
                                              bool)>&,
                    dbus::ObjectProxy::OnConnectedCallback));
  MOCK_METHOD2(RegisterPeripheralBatteryStatusSignalHandler,
               void(const base::Callback<void(const std::vector<uint8_t>&)>&,
                    dbus::ObjectProxy::OnConnectedCallback));
  MOCK_METHOD2(RegisterPowerSupplyPollSignalHandler,
               void(const base::Callback<void(const std::vector<uint8_t>&)>&,
                    dbus::ObjectProxy::OnConnectedCallback));
  MOCK_METHOD2(RegisterSuspendImminentSignalHandler,
               void(const base::Callback<void(const std::vector<uint8_t>&)>&,
                    dbus::ObjectProxy::OnConnectedCallback));
  MOCK_METHOD2(RegisterSuspendDoneSignalHandler,
               void(const base::Callback<void(const std::vector<uint8_t>&)>&,
                    dbus::ObjectProxy::OnConnectedCallback));
  MOCK_METHOD2(RegisterDarkSuspendImminentSignalHandler,
               void(const base::Callback<void(const std::vector<uint8_t>&)>&,
                    dbus::ObjectProxy::OnConnectedCallback));
  MOCK_METHOD2(RegisterInputEventSignalHandler,
               void(const base::Callback<void(const std::vector<uint8_t>&)>&,
                    dbus::ObjectProxy::OnConnectedCallback));
  MOCK_METHOD2(RegisterIdleActionImminentSignalHandler,
               void(const base::Callback<void(const std::vector<uint8_t>&)>&,
                    dbus::ObjectProxy::OnConnectedCallback));
  MOCK_METHOD2(RegisterIdleActionDeferredSignalHandler,
               void(const base::Closure&,
                    dbus::ObjectProxy::OnConnectedCallback));
 private:
  dbus::ObjectProxy* object_proxy_;
};

}  // namespace trunks

#endif  // TRUNKS_MOCK_POWER_MANAGER_PROXY_H_
