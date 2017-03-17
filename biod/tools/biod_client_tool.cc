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

#include "biod/biometrics_manager.h"

static const char kHelpText[] =
    "biod_client_tool, used to pretend to be a biometrics client, like a lock "
    "screen or fingerprint enrollment app\n\n"
    "commands:\n"
    "  enroll <biometrics manager> <user id> <label> - Starts an enroll "
    "session for the biometrics manager that will result in the enrollment of "
    "a record with the given user ID and label.\n"
    "  authenticate <biometrics manager> - Performs authentication with the "
    "given biometrics manager until the program is interrupted.\n"
    "  list [<user_id>] - Lists available biometrics managers and optionally "
    "user's records.\n"
    "  unenroll <record> - Removes the given record.\n"
    "  set_label <record> <label> - Sets the label for the given record to "
    "<label>.\n"
    "  destroy_all [<biometrics manager>] - Destroys all records for the given "
    "biometrics manager, or all biometrics managers if no object path is "
    "given.\n\n"
    "The <biometrics manager> parameter is the D-Bus object path of the "
    "biometrics manager, and can be abbreviated as the path's basename (the "
    "part after the last forward slash)\n\n"
    "The <record> parameter is also a D-Bus object path.";

static const int kDbusTimeoutMs = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT;
static const char kBiodServiceName[] = "org.chromium.BiometricsDaemon";
static const char kBiodRootPath[] = "/org/chromium/BiometricsDaemon";
static const char kBiodBiometricsManagerInterface[] =
    "org.chromium.BiometricsDaemon.BiometricsManager";
static const char kBiodEnrollSessionInterface[] =
    "org.chromium.BiometricsDaemon.EnrollSession";
static const char kBiodAuthSessionInterface[] =
    "org.chromium.BiometricsDaemon.AuthSession";
static const char kBiodRecordInterface[] =
    "org.chromium.BiometricsDaemon.Record";

using BiometricsManagerType = biod::BiometricsManager::Type;
using ScanResult = biod::BiometricsManager::ScanResult;
using EnrollScanDoneCallback = biod::BiometricsManager::EnrollScanDoneCallback;
using AuthScanDoneCallback = biod::BiometricsManager::AuthScanDoneCallback;
using SessionFailedCallback = biod::BiometricsManager::SessionFailedCallback;

std::string ToString(BiometricsManagerType type) {
  switch (type) {
    case BiometricsManagerType::kFingerprint:
      return "Fingerprint";
    case BiometricsManagerType::kRetina:
      return "Retina";
    case BiometricsManagerType::kFace:
      return "Face";
    case BiometricsManagerType::kVoice:
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

class RecordProxy {
 public:
  RecordProxy(const scoped_refptr<dbus::Bus>& bus, const dbus::ObjectPath& path)
      : bus_(bus), path_(path) {
    proxy_ = bus_->GetObjectProxy(kBiodServiceName, path_);
    GetLabel();
  }

  const dbus::ObjectPath& path() const { return path_; }
  const std::string& label() const { return label_; }

  bool SetLabel(const std::string& label) {
    dbus::MethodCall method_call(kBiodRecordInterface, "SetLabel");
    dbus::MessageWriter method_writer(&method_call);
    method_writer.AppendString(label);
    return !!proxy_->CallMethodAndBlock(&method_call, kDbusTimeoutMs);
  }

  bool Remove() {
    dbus::MethodCall method_call(kBiodRecordInterface, "Remove");
    return !!proxy_->CallMethodAndBlock(&method_call, kDbusTimeoutMs);
  }

 private:
  void GetLabel() {
    dbus::MethodCall method_call(dbus::kPropertiesInterface,
                                 dbus::kPropertiesGet);
    dbus::MessageWriter method_writer(&method_call);
    method_writer.AppendString(kBiodRecordInterface);
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

class BiometricsManagerProxy {
 public:
  using FinishCallback = base::Callback<void(bool success)>;

  BiometricsManagerProxy(const scoped_refptr<dbus::Bus>& bus,
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
        kBiodBiometricsManagerInterface,
        "EnrollScanDone",
        base::Bind(&BiometricsManagerProxy::OnEnrollScanDone,
                   base::Unretained(this)),
        base::Bind(&BiometricsManagerProxy::OnSignalConnected,
                   base::Unretained(this)));
    proxy_->ConnectToSignal(
        kBiodBiometricsManagerInterface,
        "AuthScanDone",
        base::Bind(&BiometricsManagerProxy::OnAuthScanDone,
                   base::Unretained(this)),
        base::Bind(&BiometricsManagerProxy::OnSignalConnected,
                   base::Unretained(this)));
    proxy_->ConnectToSignal(
        kBiodBiometricsManagerInterface,
        "SessionFailed",
        base::Bind(&BiometricsManagerProxy::OnSessionFailed,
                   base::Unretained(this)),
        base::Bind(&BiometricsManagerProxy::OnSignalConnected,
                   base::Unretained(this)));
  }

  const dbus::ObjectPath& path() const { return proxy_->object_path(); }
  BiometricsManagerType type() const { return type_; }

  void SetFinishHandler(const FinishCallback& on_finish) {
    on_finish_ = on_finish;
  }

  dbus::ObjectProxy* StartEnrollSession(const std::string& user_id,
                                        const std::string& label) {
    dbus::MethodCall method_call(kBiodBiometricsManagerInterface,
                                 "StartEnrollSession");
    dbus::MessageWriter method_writer(&method_call);
    method_writer.AppendString(user_id);
    method_writer.AppendString(label);

    std::unique_ptr<dbus::Response> response =
        proxy_->CallMethodAndBlock(&method_call, kDbusTimeoutMs);
    if (!response)
      return nullptr;

    dbus::MessageReader response_reader(response.get());
    dbus::ObjectPath enroll_session_path;
    CHECK(response_reader.PopObjectPath(&enroll_session_path));
    dbus::ObjectProxy* enroll_session_proxy =
        bus_->GetObjectProxy(kBiodServiceName, enroll_session_path);
    return enroll_session_proxy;
  }

  dbus::ObjectProxy* StartAuthSession() {
    dbus::MethodCall method_call(kBiodBiometricsManagerInterface,
                                 "StartAuthSession");

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

  bool DestroyAllRecords() {
    dbus::MethodCall method_call(kBiodBiometricsManagerInterface,
                                 "DestroyAllRecords");
    return !!proxy_->CallMethodAndBlock(&method_call, kDbusTimeoutMs);
  }

  std::vector<RecordProxy> GetRecordsForUser(const std::string& user_id) const {
    dbus::MethodCall method_call(kBiodBiometricsManagerInterface,
                                 "GetRecordsForUser");
    dbus::MessageWriter method_writer(&method_call);
    method_writer.AppendString(user_id);

    std::unique_ptr<dbus::Response> response =
        proxy_->CallMethodAndBlock(&method_call, kDbusTimeoutMs);
    CHECK(response);

    std::vector<RecordProxy> records;
    dbus::MessageReader response_reader(response.get());
    dbus::MessageReader records_reader(nullptr);
    CHECK(response_reader.PopArray(&records_reader));
    while (records_reader.HasMoreData()) {
      dbus::ObjectPath record_path;
      CHECK(records_reader.PopObjectPath(&record_path));
      records.emplace_back(bus_, record_path);
    }
    return records;
  }

 private:
  void OnFinish(bool success) {
    if (!on_finish_.is_null())
      on_finish_.Run(success);
  }

  void OnEnrollScanDone(dbus::Signal* signal) {
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

  void OnAuthScanDone(dbus::Signal* signal) {
    dbus::MessageReader signal_reader(signal);

    ScanResult scan_result;
    dbus::MessageReader matches_reader(nullptr);
    std::vector<std::string> user_ids;

    CHECK(signal_reader.PopUint32(reinterpret_cast<uint32_t*>(&scan_result)));
    LOG(INFO) << "Authentication: " << ToString(scan_result);

    CHECK(signal_reader.PopArray(&matches_reader));
    while (matches_reader.HasMoreData()) {
      dbus::MessageReader entry_reader(nullptr);
      CHECK(matches_reader.PopDictEntry(&entry_reader));

      std::string user_id;
      CHECK(entry_reader.PopString(&user_id));

      dbus::MessageReader record_object_paths_reader(nullptr);
      CHECK(entry_reader.PopArray(&record_object_paths_reader));
      std::stringstream record_object_paths_joined;
      while (record_object_paths_reader.HasMoreData()) {
        dbus::ObjectPath record_object_path;
        CHECK(record_object_paths_reader.PopObjectPath(&record_object_path));
        record_object_paths_joined << " \"" << record_object_path.value()
                                   << "\"";
      }

      LOG(INFO) << "Recognized user ID \"" << user_id
                << "\" with record object paths"
                << record_object_paths_joined.str();
    }
  }

  void OnSessionFailed(dbus::Signal* signal) {
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

  BiometricsManagerType type_;
  std::vector<RecordProxy> records_;

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
    CHECK(objects_msg) << "Failed to retrieve biometrics managers.";

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
        if (interface_name == kBiodBiometricsManagerInterface) {
          biometrics_managers_.emplace_back(bus_, object_path, &pset_reader);
        }
      }
    }
  }

  BiometricsManagerProxy* GetBiometricsManager(base::StringPiece path) {
    bool short_path =
        !base::StartsWith(path, "/", base::CompareCase::SENSITIVE);

    for (BiometricsManagerProxy& biometrics_manager : biometrics_managers_) {
      const std::string& biometrics_manager_path =
          biometrics_manager.path().value();
      if (short_path) {
        if (base::EndsWith(
                biometrics_manager_path, path, base::CompareCase::SENSITIVE)) {
          return &biometrics_manager;
        }
      } else if (biometrics_manager_path == path) {
        return &biometrics_manager;
      }
    }
    return nullptr;
  }

  const std::vector<BiometricsManagerProxy>& biometrics_managers() const {
    return biometrics_managers_;
  }

  int DestroyAllRecords() {
    int ret = 0;
    for (BiometricsManagerProxy& biometrics_manager : biometrics_managers_) {
      if (!biometrics_manager.DestroyAllRecords()) {
        LOG(ERROR) << "Failed to destroy record from BiometricsManager at "
                   << biometrics_manager.path().value();
        ret = 1;
      }
    }
    if (ret)
      LOG(WARNING) << "Not all records were destroyed";
    return ret;
  }

 private:
  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectProxy* proxy_;

  std::vector<BiometricsManagerProxy> biometrics_managers_;
};

void OnFinish(base::RunLoop* run_loop, int* ret_ptr, bool success) {
  *ret_ptr = success ? 0 : 1;
  run_loop->Quit();
}

int DoEnroll(BiometricsManagerProxy* biometrics_manager,
             const std::string& user_id,
             const std::string& label) {
  dbus::ObjectProxy* enroll_session_object =
      biometrics_manager->StartEnrollSession(user_id, label);

  if (enroll_session_object) {
    LOG(INFO) << "Biometric enrollment started";
  } else {
    LOG(ERROR) << "Biometric enrollment failed to start";
    return 1;
  }

  base::RunLoop run_loop;

  int ret = 1;
  biometrics_manager->SetFinishHandler(base::Bind(&OnFinish, &run_loop, &ret));

  run_loop.Run();

  if (ret) {
    LOG(INFO) << "Ending biometric enrollment";
    dbus::MethodCall cancel_call(kBiodEnrollSessionInterface, "Cancel");
    enroll_session_object->CallMethodAndBlock(&cancel_call, kDbusTimeoutMs);
  }

  return ret;
}

int DoAuthenticate(BiometricsManagerProxy* biometrics_manager) {
  dbus::ObjectProxy* auth_session_object =
      biometrics_manager->StartAuthSession();

  if (auth_session_object) {
    LOG(INFO) << "Biometric authentication started";
  } else {
    LOG(ERROR) << "Biometric authentication failed to start";
    return 1;
  }

  base::RunLoop run_loop;

  int ret = 1;
  biometrics_manager->SetFinishHandler(base::Bind(&OnFinish, &run_loop, &ret));

  run_loop.Run();

  if (ret) {
    LOG(INFO) << "Ending biometric authentication";
    dbus::MethodCall end_call(kBiodAuthSessionInterface, "End");
    auth_session_object->CallMethodAndBlock(&end_call, kDbusTimeoutMs);
  }

  return ret;
}

int DoList(BiodProxy* biod, const std::string& user_id) {
  LOG(INFO) << kBiodRootPath << " : BioD Root Object Path";
  for (const BiometricsManagerProxy& biometrics_manager :
       biod->biometrics_managers()) {
    base::StringPiece biometrics_manager_path =
        biometrics_manager.path().value();
    if (base::StartsWith(biometrics_manager_path,
                         kBiodRootPath,
                         base::CompareCase::SENSITIVE)) {
      biometrics_manager_path =
          biometrics_manager_path.substr(sizeof(kBiodRootPath));
    }
    LOG(INFO) << "  " << biometrics_manager_path << " : "
              << ToString(biometrics_manager.type()) << " Biometric";

    biometrics_manager_path = biometrics_manager.path().value();

    if (user_id.empty())
      continue;

    for (const RecordProxy& record :
         biometrics_manager.GetRecordsForUser(user_id)) {
      base::StringPiece record_path(record.path().value());
      if (base::StartsWith(record_path,
                           biometrics_manager_path,
                           base::CompareCase::SENSITIVE)) {
        record_path = record_path.substr(biometrics_manager_path.size() + 1);
      }
      LOG(INFO) << "    " << record_path
                << " : Record Label=" << record.label();
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
    BiometricsManagerProxy* biometrics_manager =
        biod.GetBiometricsManager(args[1]);
    CHECK(biometrics_manager)
        << "Failed to find biometrics manager with given path";
    return DoEnroll(biometrics_manager, args[2], args[3]);
  }

  if (command == "authenticate") {
    if (args.size() < 2) {
      LOG(ERROR) << "Expected 2 parameters for authenticate command.";
      return 1;
    }
    BiometricsManagerProxy* biometrics_manager =
        biod.GetBiometricsManager(args[1]);
    CHECK(biometrics_manager)
        << "Failed to find biometrics manager with given path";
    return DoAuthenticate(biometrics_manager);
  }

  if (command == "list") {
    return DoList(&biod, args.size() < 2 ? "" : args[1]);
  }

  if (command == "unenroll") {
    if (args.size() < 2) {
      LOG(ERROR) << "Expected 1 parameter for unenroll command.";
      return 1;
    }
    RecordProxy record(bus, dbus::ObjectPath(args[1]));
    return record.Remove() ? 0 : 1;
  }

  if (command == "set_label") {
    if (args.size() < 3) {
      LOG(ERROR) << "Expected 2 parameters for set_label command.";
      return 1;
    }
    RecordProxy record(bus, dbus::ObjectPath(args[1]));
    return record.SetLabel(args[2]);
  }

  if (command == "destroy_all") {
    if (args.size() >= 2) {
      BiometricsManagerProxy* biometrics_manager =
          biod.GetBiometricsManager(args[1]);
      CHECK(biometrics_manager)
          << "Failed to find biometrics_manager with given path";
      return biometrics_manager->DestroyAllRecords() ? 0 : 1;
    } else {
      return biod.DestroyAllRecords();
    }
  }

  LOG(ERROR) << "Unrecognized command " << command;
  LOG(INFO) << kHelpText;

  return 1;
}
