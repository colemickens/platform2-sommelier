// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>

#include <brillo/flag_helper.h>

#include <dbus/bus.h>
#include <dbus/object_manager.h>

#include "biod/biometric.h"

static const char kHelpText[] =
    "biod_client_tool, used to pretend to be a biometrics client, like a lock "
    "screen or fingerprint enrollment app\n\n"
    "commands:\n"
    "  enroll <biometric> <user id> <label> - Starts an enrollment session for "
    "the biometric that will result in an enrollment with the given user ID "
    "and label.\n"
    "  authenticate <biometric> - Performs authentication with the given "
    "biometric until the program is interrupted.\n"
    "  list - Lists all biometrics and their enrollments.\n"
    "  unenroll <enrollment> - Removes the given enrollment.\n"
    "  set_label <enrollment> <label> - Sets the label for the given "
    "enrollment to <label>.\n"
    "  destroy_all [biometric] - Destroys all enrollments for the given "
    "biometric, or all biometrics if no object path is given.\n\n"
    "The <biometric> parameter is the D-Bus object path of the biometric, and "
    "can be abbreviated as the path's basename (the part after the last "
    "forward slash)\n\n"
    "The <enrollment> parameter is also a D-Bus object path that can be "
    "abbreviated but must include the biometric name in the path (e.g. "
    "FakeBiometric/Enrollment55).";

static const int kDbusTimeoutMs = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT;
static const char kBiodServiceName[] = "org.chromium.BiometricsDaemon";
static const char kBiodRootPath[] = "/org/chromium/BiometricsDaemon";
static const char kBiodBiometricInterface[] =
    "org.chromium.BiometricsDaemon.Biometric";
static const char kBiodEnrollInterface[] =
    "org.chromium.BiometricsDaemon.Enroll";
static const char kBiodAuthenticationInterface[] =
    "org.chromium.BiometricsDaemon.Authentication";
static const char kBiodEnrollmentInterface[] =
    "org.chromium.BiometricsDaemon.Enrollment";

using BiometricType = biod::Biometric::Type;
using ScanResult = biod::Biometric::ScanResult;
using ScanCallback = biod::Biometric::ScanCallback;
using AttemptCallback = biod::Biometric::AttemptCallback;
using FailureCallback = biod::Biometric::FailureCallback;

std::string ToString(BiometricType type) {
  switch (type) {
    case BiometricType::kFingerprint:
      return "Fingerprint";
    case BiometricType::kRetina:
      return "Retina";
    case BiometricType::kFace:
      return "Face";
    case BiometricType::kVoice:
      return "Voice";
    default:
      return "Unknown";
  }
}

std::string ToString(ScanResult result) {
  switch (result) {
    case ScanResult::kSuccess:
      return "Success";
    case ScanResult::kPartial:
      return "Partial";
    case ScanResult::kInsufficient:
      return "Insufficient";
    case ScanResult::kSensorDirty:
      return "Sensor Dirty";
    case ScanResult::kTooSlow:
      return "Too Slow";
    case ScanResult::kTooFast:
      return "Too Fast";
    case ScanResult::kImmobile:
      return "Immobile";
    default:
      return "Unknown Result";
  }
}

class EnrollmentProxy {
 public:
  EnrollmentProxy(const scoped_refptr<dbus::Bus>& bus,
                  const dbus::ObjectPath& path)
      : bus_(bus), path_(path) {
    proxy_ = bus_->GetObjectProxy(kBiodServiceName, path_);
    GetLabel();
  }

  const dbus::ObjectPath& path() const { return path_; }
  const std::string& label() const { return label_; }

  bool SetLabel(const std::string& label) {
    dbus::MethodCall method_call(kBiodEnrollmentInterface, "SetLabel");
    dbus::MessageWriter method_writer(&method_call);
    method_writer.AppendString(label);
    return !!proxy_->CallMethodAndBlock(&method_call, kDbusTimeoutMs);
  }

  bool Remove() {
    dbus::MethodCall method_call(kBiodEnrollmentInterface, "Remove");
    return !!proxy_->CallMethodAndBlock(&method_call, kDbusTimeoutMs);
  }

 private:
  void GetLabel() {
    dbus::MethodCall method_call(dbus::kPropertiesInterface,
                                 dbus::kPropertiesGet);
    dbus::MessageWriter method_writer(&method_call);
    method_writer.AppendString(kBiodEnrollmentInterface);
    method_writer.AppendString("Label");
    std::unique_ptr<dbus::Response> response =
        proxy_->CallMethodAndBlock(&method_call, kDbusTimeoutMs);
    CHECK(response);

    dbus::MessageReader response_reader(response.get());
    CHECK(response_reader.PopVariantOfString(&label_));
  }

  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectPath path_;
  dbus::ObjectProxy* proxy_;

  std::string label_;
};

class BiometricProxy {
 public:
  using FinishCallback = base::Callback<void(bool success)>;

  BiometricProxy(const scoped_refptr<dbus::Bus>& bus,
                 const dbus::ObjectPath& path,
                 dbus::MessageReader* pset_reader)
      : bus_(bus) {
    proxy_ = bus_->GetObjectProxy(kBiodServiceName, path);

    while (pset_reader->HasMoreData()) {
      dbus::MessageReader pset_entry_reader(nullptr);
      std::string property_name;
      CHECK(pset_reader->PopDictEntry(&pset_entry_reader));
      CHECK(pset_entry_reader.PopString(&property_name));

      if (property_name == "Type") {
        CHECK(pset_entry_reader.PopVariantOfUint32(
            reinterpret_cast<uint32_t*>(&type_)));
      }
    }

    proxy_->ConnectToSignal(
        kBiodBiometricInterface,
        "Scanned",
        base::Bind(&BiometricProxy::OnScanned, base::Unretained(this)),
        base::Bind(&BiometricProxy::OnSignalConnected, base::Unretained(this)));
    proxy_->ConnectToSignal(
        kBiodBiometricInterface,
        "Attempt",
        base::Bind(&BiometricProxy::OnAttempt, base::Unretained(this)),
        base::Bind(&BiometricProxy::OnSignalConnected, base::Unretained(this)));
    proxy_->ConnectToSignal(
        kBiodBiometricInterface,
        "Failure",
        base::Bind(&BiometricProxy::OnFailure, base::Unretained(this)),
        base::Bind(&BiometricProxy::OnSignalConnected, base::Unretained(this)));

    GetEnrollments();
  }

  const dbus::ObjectPath& path() const { return proxy_->object_path(); }
  BiometricType type() const { return type_; }

  void SetFinishHandler(const FinishCallback& on_finish) {
    on_finish_ = on_finish;
  }

  dbus::ObjectProxy* StartEnroll(const std::string& user_id,
                                 const std::string& label) {
    dbus::MethodCall method_call(kBiodBiometricInterface, "StartEnroll");
    dbus::MessageWriter method_writer(&method_call);
    method_writer.AppendString(user_id);
    method_writer.AppendString(label);

    std::unique_ptr<dbus::Response> response =
        proxy_->CallMethodAndBlock(&method_call, kDbusTimeoutMs);
    if (!response)
      return nullptr;

    dbus::MessageReader response_reader(response.get());
    dbus::ObjectPath enroll_path;
    CHECK(response_reader.PopObjectPath(&enroll_path));
    dbus::ObjectProxy* enroll_proxy =
        bus_->GetObjectProxy(kBiodServiceName, enroll_path);
    return enroll_proxy;
  }

  dbus::ObjectProxy* StartAuthentication() {
    dbus::MethodCall method_call(kBiodBiometricInterface,
                                 "StartAuthentication");

    std::unique_ptr<dbus::Response> response =
        proxy_->CallMethodAndBlock(&method_call, kDbusTimeoutMs);
    if (!response)
      return nullptr;

    dbus::MessageReader response_reader(response.get());
    dbus::ObjectPath auth_path;
    CHECK(response_reader.PopObjectPath(&auth_path));
    dbus::ObjectProxy* auth_proxy =
        bus_->GetObjectProxy(kBiodServiceName, auth_path);
    return auth_proxy;
  }

  bool DestroyAllEnrollments() {
    dbus::MethodCall method_call(kBiodBiometricInterface,
                                 "DestroyAllEnrollments");
    return !!proxy_->CallMethodAndBlock(&method_call, kDbusTimeoutMs);
  }

  std::vector<EnrollmentProxy>& enrollments() { return enrollments_; }

  const std::vector<EnrollmentProxy>& enrollments() const {
    return enrollments_;
  }

 private:
  void GetEnrollments() {
    dbus::MethodCall method_call(kBiodBiometricInterface, "GetEnrollments");
    std::unique_ptr<dbus::Response> response =
        proxy_->CallMethodAndBlock(&method_call, kDbusTimeoutMs);
    CHECK(response);

    dbus::MessageReader response_reader(response.get());
    dbus::MessageReader enrollments_reader(nullptr);
    CHECK(response_reader.PopArray(&enrollments_reader));
    while (enrollments_reader.HasMoreData()) {
      dbus::ObjectPath enrollment_path;
      CHECK(enrollments_reader.PopObjectPath(&enrollment_path));
      enrollments_.emplace_back(bus_, enrollment_path);
    }
  }

  void OnFinish(bool success) {
    if (!on_finish_.is_null())
      on_finish_.Run(success);
  }

  void OnScanned(dbus::Signal* signal) {
    dbus::MessageReader signal_reader(signal);

    ScanResult scan_result;
    bool complete;
    uint8_t percent_complete;

    CHECK(signal_reader.PopUint32(reinterpret_cast<uint32_t*>(&scan_result)));
    CHECK(signal_reader.PopBool(&complete));
    if (signal_reader.HasMoreData()) {
      CHECK(signal_reader.PopByte(&percent_complete));
      LOG(INFO) << "Biometric Scanned: " << ToString(scan_result) << " "
                << static_cast<int>(percent_complete) << "% complete";
    } else {
      LOG(INFO) << "Biometric Scanned: " << ToString(scan_result);
    }

    if (complete) {
      LOG(INFO) << "Biometric enrollment complete";
      OnFinish(true);
    }
  }

  void OnAttempt(dbus::Signal* signal) {
    dbus::MessageReader signal_reader(signal);

    ScanResult scan_result;
    dbus::MessageReader user_ids_reader(nullptr);
    std::vector<std::string> user_ids;

    CHECK(signal_reader.PopUint32(reinterpret_cast<uint32_t*>(&scan_result)));
    CHECK(signal_reader.PopArray(&user_ids_reader));

    std::stringstream user_ids_joined;

    while (user_ids_reader.HasMoreData()) {
      user_ids.emplace_back();
      CHECK(user_ids_reader.PopString(&user_ids.back()));
      user_ids_joined << user_ids.back() << " ";
    }

    LOG(INFO) << "Authentication: " << ToString(scan_result) << ", recognized "
              << user_ids.size() << " user IDs: " << user_ids_joined.str();
  }

  void OnFailure(dbus::Signal* signal) {
    LOG(ERROR) << "Biometric device failed";
    OnFinish(false);
  }

  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool success) {
    if (!success) {
      LOG(ERROR) << "Failed to connect to signal " << signal << " on interface "
                 << interface;
      OnFinish(false);
    }
  }

  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectProxy* proxy_;

  BiometricType type_;
  std::vector<EnrollmentProxy> enrollments_;

  FinishCallback on_finish_;
};

class BiodProxy {
 public:
  explicit BiodProxy(const scoped_refptr<dbus::Bus>& bus) : bus_(bus) {
    proxy_ =
        bus_->GetObjectProxy(kBiodServiceName, dbus::ObjectPath(kBiodRootPath));

    dbus::MethodCall get_objects_method(dbus::kObjectManagerInterface,
                                        dbus::kObjectManagerGetManagedObjects);

    std::unique_ptr<dbus::Response> objects_msg =
        proxy_->CallMethodAndBlock(&get_objects_method, kDbusTimeoutMs);
    CHECK(objects_msg) << "Failed to retrieve biometrics.";

    dbus::MessageReader reader(objects_msg.get());
    dbus::MessageReader array_reader(nullptr);
    CHECK(reader.PopArray(&array_reader));

    while (array_reader.HasMoreData()) {
      dbus::MessageReader dict_entry_reader(nullptr);
      dbus::ObjectPath object_path;
      CHECK(array_reader.PopDictEntry(&dict_entry_reader));
      CHECK(dict_entry_reader.PopObjectPath(&object_path));

      dbus::MessageReader interface_reader(nullptr);
      CHECK(dict_entry_reader.PopArray(&interface_reader));

      while (interface_reader.HasMoreData()) {
        dbus::MessageReader interface_entry_reader(nullptr);
        std::string interface_name;
        CHECK(interface_reader.PopDictEntry(&interface_entry_reader));
        CHECK(interface_entry_reader.PopString(&interface_name));

        dbus::MessageReader pset_reader(nullptr);
        CHECK(interface_entry_reader.PopArray(&pset_reader));
        if (interface_name == kBiodBiometricInterface) {
          biometrics_.emplace_back(bus_, object_path, &pset_reader);
        }
      }
    }
  }

  BiometricProxy* GetBiometric(base::StringPiece path) {
    bool short_path =
        !base::StartsWith(path, "/", base::CompareCase::SENSITIVE);

    for (BiometricProxy& biometric : biometrics_) {
      const std::string& biometric_path = biometric.path().value();
      if (short_path) {
        if (base::EndsWith(
                biometric_path, path, base::CompareCase::SENSITIVE)) {
          return &biometric;
        }
      } else if (biometric_path == path) {
        return &biometric;
      }
    }
    return nullptr;
  }

  EnrollmentProxy* GetEnrollment(base::StringPiece path) {
    bool short_path =
        !base::StartsWith(path, "/", base::CompareCase::SENSITIVE);

    if (short_path) {
      std::vector<base::StringPiece> parts = base::SplitStringPiece(
          path, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
      if (parts.size() != 2) {
        LOG(ERROR)
            << "Expected a single path separator in short enrollment path";
        return nullptr;
      }

      base::StringPiece biometric_name = parts[0];
      BiometricProxy* biometric = GetBiometric(biometric_name);
      if (!biometric)
        return nullptr;

      base::StringPiece enrollment_name = parts[1];
      for (EnrollmentProxy& enrollment : biometric->enrollments()) {
        if (base::EndsWith(enrollment.path().value(),
                           enrollment_name,
                           base::CompareCase::SENSITIVE)) {
          return &enrollment;
        }
      }

      return nullptr;
    } else {
      for (BiometricProxy& biometric : biometrics_) {
        for (EnrollmentProxy& enrollment : biometric.enrollments()) {
          if (enrollment.path().value() == path) {
            return &enrollment;
          }
        }
      }
    }

    return nullptr;
  }

  std::vector<BiometricProxy>& biometrics() { return biometrics_; }

 private:
  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectProxy* proxy_;

  std::vector<BiometricProxy> biometrics_;
};

void OnFinish(base::RunLoop* run_loop, int* ret_ptr, bool success) {
  *ret_ptr = success ? 0 : 1;
  run_loop->Quit();
}

int DoEnroll(BiometricProxy* biometric,
             const std::string& user_id,
             const std::string& label) {
  dbus::ObjectProxy* enroll_object = biometric->StartEnroll(user_id, label);

  if (enroll_object) {
    LOG(INFO) << "Biometric enrollment started";
  } else {
    LOG(ERROR) << "Biometric enrollment failed to start";
    return 1;
  }

  base::RunLoop run_loop;

  int ret = 1;
  biometric->SetFinishHandler(base::Bind(&OnFinish, &run_loop, &ret));

  run_loop.Run();

  if (ret) {
    LOG(INFO) << "Ending biometric enrollment";
    dbus::MethodCall cancel_call(kBiodEnrollInterface, "Cancel");
    enroll_object->CallMethodAndBlock(&cancel_call, kDbusTimeoutMs);
  }

  return ret;
}

int DoAuthenticate(BiometricProxy* biometric) {
  dbus::ObjectProxy* auth_object = biometric->StartAuthentication();

  if (auth_object) {
    LOG(INFO) << "Biometric authentication started";
  } else {
    LOG(ERROR) << "Biometric authentication failed to start";
    return 1;
  }

  base::RunLoop run_loop;

  int ret = 1;
  biometric->SetFinishHandler(base::Bind(&OnFinish, &run_loop, &ret));

  run_loop.Run();

  if (ret) {
    LOG(INFO) << "Ending biometric authentication";
    dbus::MethodCall end_call(kBiodAuthenticationInterface, "End");
    auth_object->CallMethodAndBlock(&end_call, kDbusTimeoutMs);
  }

  return ret;
}

int DoList(BiodProxy* biod) {
  LOG(INFO) << kBiodRootPath << " : BioD Root Object Path";
  for (const BiometricProxy& biometric : biod->biometrics()) {
    base::StringPiece biometric_path = biometric.path().value();
    if (base::StartsWith(
            biometric_path, kBiodRootPath, base::CompareCase::SENSITIVE)) {
      biometric_path = biometric_path.substr(sizeof(kBiodRootPath));
    }
    LOG(INFO) << "  " << biometric_path << " : " << ToString(biometric.type())
              << " Biometric";

    biometric_path = biometric.path().value();

    for (const EnrollmentProxy& enrollment : biometric.enrollments()) {
      base::StringPiece enrollment_path(enrollment.path().value());
      if (base::StartsWith(
              enrollment_path, biometric_path, base::CompareCase::SENSITIVE)) {
        enrollment_path = enrollment_path.substr(biometric_path.size() + 1);
      }
      LOG(INFO) << "    " << enrollment_path
                << " : Enrollment Label=" << enrollment.label();
    }
  }
  return 0;
}

int main(int argc, char* argv[]) {
  brillo::FlagHelper::Init(argc, argv, kHelpText);

  base::CommandLine::StringVector args =
      base::CommandLine::ForCurrentProcess()->GetArgs();

  if (args.size() == 0) {
    LOG(ERROR) << "Expected a command.";
    LOG(INFO) << "Get help with with the --help flag.";
    return 1;
  }

  base::MessageLoopForIO message_loop;
  dbus::Bus::Options bus_options;
  bus_options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(bus_options));
  CHECK(bus->Connect()) << "Failed to connect to system D-Bus.";

  BiodProxy biod(bus.get());

  const auto& command = args[0];
  if (command == "enroll") {
    if (args.size() < 4) {
      LOG(ERROR) << "Expected 3 parameters for enroll command.";
      return 1;
    }
    BiometricProxy* biometric = biod.GetBiometric(args[1]);
    CHECK(biometric) << "Failed to find biometric with given path";
    return DoEnroll(biometric, args[2], args[3]);
  }

  if (command == "authenticate") {
    if (args.size() < 2) {
      LOG(ERROR) << "Expected 2 parameters for authenticate command.";
      return 1;
    }
    BiometricProxy* biometric = biod.GetBiometric(args[1]);
    CHECK(biometric) << "Failed to find biometric with given path";
    return DoAuthenticate(biometric);
  }

  if (command == "list") {
    return DoList(&biod);
  }

  if (command == "unenroll") {
    if (args.size() < 2) {
      LOG(ERROR) << "Expected 1 parameter for unenroll command.";
      return 1;
    }
    EnrollmentProxy* enrollment = biod.GetEnrollment(args[1]);
    CHECK(enrollment) << "Failed to find enrollment with given path";
    return enrollment->Remove() ? 0 : 1;
  }

  if (command == "set_label") {
    if (args.size() < 3) {
      LOG(ERROR) << "Expected 2 parameters for set_enroll command.";
      return 1;
    }
    EnrollmentProxy* enrollment = biod.GetEnrollment(args[1]);
    CHECK(enrollment) << "Failed to find enrollment with given path";
    return enrollment->SetLabel(args[2]);
  }

  if (command == "destroy_all") {
    if (args.size() >= 2) {
      BiometricProxy* biometric = biod.GetBiometric(args[1]);
      CHECK(biometric) << "Failed to find biometric with given path";
      return biometric->DestroyAllEnrollments() ? 0 : 1;
    } else {
      int ret = 0;
      for (BiometricProxy& biometric : biod.biometrics()) {
        if (!biometric.DestroyAllEnrollments()) {
          LOG(ERROR) << "Failed to destroy enrollment from Biometric at "
                     << biometric.path().value();
        }
      }
      if (ret)
        LOG(WARNING) << "Not all enrollments were destroyed";
      return ret;
    }
  }

  LOG(ERROR) << "Unrecognized command " << command;
  LOG(INFO) << kHelpText;

  return 1;
}
