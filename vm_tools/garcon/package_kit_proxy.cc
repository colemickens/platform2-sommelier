// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/garcon/package_kit_proxy.h"

#include <memory>
#include <string>
#include <utility>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/threading/thread_task_runner_handle.h>
#include <dbus/message.h>
#include <dbus/property.h>

#include "container_guest.grpc.pb.h"  // NOLINT(build/include)

namespace {

// Constants for the PackageKit D-Bus service.
// See:
// https://github.com/hughsie/PackageKit/blob/master/src/org.freedesktop.PackageKit.Transaction.xml
constexpr char kPackageKitInterface[] = "org.freedesktop.PackageKit";
constexpr char kPackageKitServicePath[] = "/org/freedesktop/PackageKit";
constexpr char kPackageKitServiceName[] = "org.freedesktop.PackageKit";
constexpr char kPackageKitTransactionInterface[] =
    "org.freedesktop.PackageKit.Transaction";
constexpr char kSetHintsMethod[] = "SetHints";
constexpr char kCreateTransactionMethod[] = "CreateTransaction";
constexpr char kGetDetailsLocalMethod[] = "GetDetailsLocal";
constexpr char kInstallFilesMethod[] = "InstallFiles";
constexpr char kErrorCodeSignal[] = "ErrorCode";
constexpr char kFinishedSignal[] = "Finished";
constexpr char kDetailsSignal[] = "Details";

// Key names for the Details signal from PackageKit.
constexpr char kDetailsKeyPackageId[] = "package-id";
constexpr char kDetailsKeyLicense[] = "license";
constexpr char kDetailsKeyDescription[] = "description";
constexpr char kDetailsKeyUrl[] = "url";
constexpr char kDetailsKeySize[] = "size";
constexpr char kDetailsKeySummary[] = "summary";

// See:
// https://www.freedesktop.org/software/PackageKit/gtk-doc/PackageKit-Enumerations.html#PkExitEnum
constexpr uint32_t kPackageKitExitCodeSuccess = 1;
// See:
// https://www.freedesktop.org/software/PackageKit/gtk-doc/PackageKit-Enumerations.html#PkStatusEnum
constexpr uint32_t kPackageKitStatusDownload = 8;
constexpr uint32_t kPackageKitStatusInstall = 9;

// Timeout for when we are querying for package information in case PackageKit
// dies.
constexpr int kGetLinuxPackageInfoTimeoutSeconds = 5;
constexpr base::TimeDelta kGetLinuxPackageInfoTimeout =
    base::TimeDelta::FromSeconds(kGetLinuxPackageInfoTimeoutSeconds);

}  // namespace

namespace vm_tools {
namespace garcon {

// Handles the property changed signals that come back from PackageKit.
struct PackageKitProxy::PackageKitTransactionProperties
    : public dbus::PropertySet {
  // These are the only 2 properties we care about.
  dbus::Property<uint32_t> status;
  dbus::Property<uint32_t> percentage;
  PackageKitTransactionProperties(dbus::ObjectProxy* object_proxy,
                                  const PropertyChangedCallback callback)
      : dbus::PropertySet(
            object_proxy, kPackageKitTransactionInterface, callback) {
    RegisterProperty("Status", &status);
    RegisterProperty("Percentage", &percentage);
  }
};

PackageKitProxy::PackageInfoTransactionData::PackageInfoTransactionData(
    const base::FilePath& file_path_in,
    std::shared_ptr<LinuxPackageInfo> pkg_info_in)
    : file_path(file_path_in),
      event(false /*manual_reset*/, false /*initially_signaled*/),
      pkg_info(pkg_info_in) {}

// static
std::unique_ptr<PackageKitProxy> PackageKitProxy::Create(
    base::WeakPtr<PackageKitObserver> observer) {
  if (!observer)
    return nullptr;
  auto pk_proxy = base::WrapUnique(new PackageKitProxy(std::move(observer)));
  if (!pk_proxy->Init()) {
    pk_proxy.reset();
  }
  return pk_proxy;
}

PackageKitProxy::PackageKitProxy(base::WeakPtr<PackageKitObserver> observer)
    : observer_(observer),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this) {}

PackageKitProxy::~PackageKitProxy() = default;

bool PackageKitProxy::Init() {
  dbus::Bus::Options opts;
  opts.bus_type = dbus::Bus::SYSTEM;
  bus_ = new dbus::Bus(std::move(opts));
  if (!bus_->Connect()) {
    LOG(ERROR) << "Failed to connect to system bus";
    return false;
  }
  packagekit_service_proxy_ = bus_->GetObjectProxy(
      kPackageKitServiceName, dbus::ObjectPath(kPackageKitServicePath));
  if (!packagekit_service_proxy_) {
    LOG(ERROR) << "Failed to get PackageKit D-Bus proxy";
    return false;
  }
  packagekit_service_proxy_->WaitForServiceToBeAvailable(
      base::Bind(&PackageKitProxy::OnPackageKitServiceAvailable,
                 weak_ptr_factory_.GetWeakPtr()));
  return true;
}

bool PackageKitProxy::GetLinuxPackageInfo(
    const base::FilePath& file_path,
    std::shared_ptr<LinuxPackageInfo> out_pkg_info,
    std::string* out_error) {
  CHECK(out_error);
  // We use another var for the error message into the D-Bus thread call so we
  // don't have contention with that var in the case of a timeout since we want
  // to set the error in a timeout, but not the pkg_info. Shared pointers are
  // used so that if the call times out the pointers are still valid on the
  // D-Bus thread.
  std::shared_ptr<PackageInfoTransactionData> data =
      std::make_shared<PackageInfoTransactionData>(file_path, out_pkg_info);
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&PackageKitProxy::GetLinuxPackageInfoOnDBusThread,
                            weak_ptr_factory_.GetWeakPtr(), data));

  bool result;
  if (!data->event.TimedWait(kGetLinuxPackageInfoTimeout)) {
    LOG(ERROR) << "Timeout waiting on Linux package info";
    out_error->assign("Timeout");
    result = false;
  } else {
    out_error->assign(data->error);
    result = data->result;
  }

  // Delete the D-Bus proxy on the D-Bus thread, this'll clean up all the
  // callbacks and release the shared_ptr that we allocated above.
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PackageKitProxy::RemoveObjectProxyOnDBusThread,
                 weak_ptr_factory_.GetWeakPtr(), data->info_transaction_path));
  return result;
}

int PackageKitProxy::InstallLinuxPackage(const base::FilePath& file_path,
                                         std::string* out_error) {
  base::WaitableEvent event(false /*manual_reset*/,
                            false /*initially_signaled*/);
  int status = vm_tools::container::InstallLinuxPackageResponse::FAILED;
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&PackageKitProxy::InstallLinuxPackageOnDBusThread,
                            weak_ptr_factory_.GetWeakPtr(), file_path, &event,
                            &status, out_error));
  event.Wait();
  return status;
}

void PackageKitProxy::GetLinuxPackageInfoOnDBusThread(
    std::shared_ptr<PackageInfoTransactionData> data) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  LOG(INFO) << "Getting information on local Linux package";
  // Create a transaction with PackageKit for performing the query.
  dbus::MethodCall method_call(kPackageKitInterface, kCreateTransactionMethod);
  dbus::MessageWriter writer(&method_call);
  std::unique_ptr<dbus::Response> dbus_response =
      packagekit_service_proxy_->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  data->result = false;
  if (!dbus_response) {
    data->error.assign("Failure calling CreateTransaction");
    LOG(ERROR) << data->error;
    data->event.Signal();
    return;
  }
  // CreateTransaction returns the object path for the transaction session we
  // have created.
  dbus::MessageReader reader(dbus_response.get());
  if (!reader.PopObjectPath(&data->info_transaction_path)) {
    data->error.assign("Failure reading object path from transaction result");
    LOG(ERROR) << data->error;
    data->event.Signal();
    return;
  }
  dbus::ObjectProxy* transaction_proxy =
      bus_->GetObjectProxy(kPackageKitServiceName, data->info_transaction_path);
  if (!transaction_proxy) {
    data->error.assign("Failed to get proxy for transaction");
    LOG(ERROR) << data->error;
    data->event.Signal();
    return;
  }

  // Hook up all the necessary signals to PackageKit for monitoring the
  // transaction. After these are all hooked up, we will initiate the info
  // request.
  transaction_proxy->ConnectToSignal(
      kPackageKitTransactionInterface, kErrorCodeSignal,
      base::Bind(&PackageKitProxy::OnPackageKitInfoError,
                 weak_ptr_factory_.GetWeakPtr(), data),
      base::Bind(&PackageKitProxy::OnErrorSignalConnectedForInfo,
                 weak_ptr_factory_.GetWeakPtr(), transaction_proxy, data));
}

void PackageKitProxy::InstallLinuxPackageOnDBusThread(
    const base::FilePath& file_path,
    base::WaitableEvent* event,
    int* status,
    std::string* out_error) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  CHECK(event);
  CHECK(status);
  CHECK(out_error);
  // Make sure we don't already have one in progress.
  if (transaction_properties_) {
    *status = vm_tools::container::InstallLinuxPackageResponse::
        INSTALL_ALREADY_ACTIVE;
    *out_error = "Install is already active";
    LOG(ERROR) << *out_error;
    event->Signal();
    return;
  }
  // Create a transaction with PackageKit for performing the installation.
  dbus::MethodCall method_call(kPackageKitInterface, kCreateTransactionMethod);
  dbus::MessageWriter writer(&method_call);
  std::unique_ptr<dbus::Response> dbus_response =
      packagekit_service_proxy_->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!dbus_response) {
    *status = vm_tools::container::InstallLinuxPackageResponse::FAILED;
    *out_error = "Failure calling CreateTransaction";
    LOG(ERROR) << *out_error;
    event->Signal();
    return;
  }
  // CreateTransaction returns the object path for the transaction session we
  // have created.
  dbus::MessageReader reader(dbus_response.get());
  if (!reader.PopObjectPath(&install_transaction_path_)) {
    *status = vm_tools::container::InstallLinuxPackageResponse::FAILED;
    *out_error = "Failure reading object path from transaction result";
    LOG(ERROR) << *out_error;
    event->Signal();
    return;
  }
  dbus::ObjectProxy* transaction_proxy =
      bus_->GetObjectProxy(kPackageKitServiceName, install_transaction_path_);
  if (!transaction_proxy) {
    *status = vm_tools::container::InstallLinuxPackageResponse::FAILED;
    *out_error = "Failed to get proxy for transaction";
    LOG(ERROR) << *out_error;
    event->Signal();
    return;
  }

  // Set the hint that we don't support interactive installs. I haven't seen a
  // case of this yet, but it seems like a good idea to set it if it does occur.
  dbus::MethodCall sethints_call(kPackageKitTransactionInterface,
                                 kSetHintsMethod);
  dbus::MessageWriter sethints_writer(&sethints_call);
  sethints_writer.AppendArrayOfStrings({"interactive=false"});
  dbus_response = transaction_proxy->CallMethodAndBlock(
      &sethints_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!dbus_response) {
    // Don't propagate a failure, this was just a hint.
    LOG(ERROR) << "Failure calling SetHints";
  }

  // Hook up all the necessary signals to PackageKit for monitoring the
  // transaction. After these are all hooked up, we will initiate the install.
  transaction_properties_ = std::make_unique<PackageKitTransactionProperties>(
      transaction_proxy,
      base::Bind(&PackageKitProxy::OnPackageKitPropertyChanged,
                 weak_ptr_factory_.GetWeakPtr()));
  transaction_properties_->ConnectSignals();
  transaction_properties_->GetAll();
  transaction_proxy->ConnectToSignal(
      kPackageKitTransactionInterface, kErrorCodeSignal,
      base::Bind(&PackageKitProxy::OnPackageKitInstallError,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&PackageKitProxy::OnErrorSignalConnectedForInstall,
                 weak_ptr_factory_.GetWeakPtr(), transaction_proxy, file_path));

  *status = vm_tools::container::InstallLinuxPackageResponse::STARTED;
  *out_error = "";
  event->Signal();
}

void PackageKitProxy::OnErrorSignalConnectedForInstall(
    dbus::ObjectProxy* transaction_proxy,
    const base::FilePath& file_path,
    const std::string& interface_name,
    const std::string& signal_name,
    bool is_connected) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (!is_connected) {
    // Any failures in signal hookups mean we should abort.
    HandleInstallCompletion(false,
                            "Failed to hookup " + signal_name + " signal");
    return;
  }
  DCHECK_EQ(signal_name, kErrorCodeSignal);
  // This is the first signal we hook up, then we hook up the Finished one
  // next.
  transaction_proxy->ConnectToSignal(
      kPackageKitTransactionInterface, kFinishedSignal,
      base::Bind(&PackageKitProxy::OnPackageKitInstallFinished,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&PackageKitProxy::OnFinishedSignalConnectedForInstall,
                 weak_ptr_factory_.GetWeakPtr(), transaction_proxy, file_path));
}

void PackageKitProxy::OnFinishedSignalConnectedForInstall(
    dbus::ObjectProxy* transaction_proxy,
    const base::FilePath& file_path,
    const std::string& interface_name,
    const std::string& signal_name,
    bool is_connected) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (!is_connected) {
    // Any failures in signal hookups mean we should abort.
    HandleInstallCompletion(false,
                            "Failed to hookup " + signal_name + " signal");
    return;
  }
  DCHECK_EQ(signal_name, kFinishedSignal);
  // Now we invoke the call for performing the actual installation since all
  // of our signals are hooked up.
  dbus::MethodCall method_call(kPackageKitTransactionInterface,
                               kInstallFilesMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendUint64(0);  // Allow installing untrusted files.
  writer.AppendArrayOfStrings({file_path.value()});
  std::unique_ptr<dbus::Response> dbus_response =
      transaction_proxy->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!dbus_response) {
    LOG(ERROR) << "Failure calling InstallFiles";
    HandleInstallCompletion(false, "Failure calling InstallFiles");
  }
}

void PackageKitProxy::OnErrorSignalConnectedForInfo(
    dbus::ObjectProxy* transaction_proxy,
    std::shared_ptr<PackageInfoTransactionData> data,
    const std::string& interface_name,
    const std::string& signal_name,
    bool is_connected) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (!is_connected) {
    // Any failures in signal hookups mean we should abort.
    data->result = false;
    data->error.assign("Failed to hookup " + signal_name + " signal");
    data->event.Signal();
    return;
  }
  CHECK(transaction_proxy);
  DCHECK_EQ(signal_name, kErrorCodeSignal);
  // This is the first signal we hook up, then we hook up the Finished one
  // next.
  transaction_proxy->ConnectToSignal(
      kPackageKitTransactionInterface, kFinishedSignal,
      base::Bind(&PackageKitProxy::OnPackageKitInfoFinished,
                 weak_ptr_factory_.GetWeakPtr(), data),
      base::Bind(&PackageKitProxy::OnFinishedSignalConnectedForInfo,
                 weak_ptr_factory_.GetWeakPtr(), transaction_proxy, data));
}

void PackageKitProxy::OnFinishedSignalConnectedForInfo(
    dbus::ObjectProxy* transaction_proxy,
    std::shared_ptr<PackageInfoTransactionData> data,
    const std::string& interface_name,
    const std::string& signal_name,
    bool is_connected) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (!is_connected) {
    // Any failures in signal hookups mean we should abort.
    data->result = false;
    data->error.assign("Failed to hookup " + signal_name + " signal");
    data->event.Signal();
    return;
  }
  CHECK(transaction_proxy);
  DCHECK_EQ(signal_name, kFinishedSignal);
  // This is the second signal we hook up, then we hook up the Details one
  // next.
  transaction_proxy->ConnectToSignal(
      kPackageKitTransactionInterface, kDetailsSignal,
      base::Bind(&PackageKitProxy::OnPackageKitInfoDetails,
                 weak_ptr_factory_.GetWeakPtr(), data),
      base::Bind(&PackageKitProxy::OnDetailsSignalConnectedForInfo,
                 weak_ptr_factory_.GetWeakPtr(), transaction_proxy, data));
}

void PackageKitProxy::OnDetailsSignalConnectedForInfo(
    dbus::ObjectProxy* transaction_proxy,
    std::shared_ptr<PackageInfoTransactionData> data,
    const std::string& interface_name,
    const std::string& signal_name,
    bool is_connected) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (!is_connected) {
    // Any failures in signal hookups mean we should abort.
    data->result = false;
    data->error.assign("Failed to hookup " + signal_name + " signal");
    data->event.Signal();
    return;
  }
  CHECK(transaction_proxy);
  DCHECK_EQ(signal_name, kDetailsSignal);
  // Now we invoke the call for performing the actual query since all
  // of our signals are hooked up.
  dbus::MethodCall method_call(kPackageKitTransactionInterface,
                               kGetDetailsLocalMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfStrings({data->file_path.value()});
  std::unique_ptr<dbus::Response> dbus_response =
      transaction_proxy->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!dbus_response) {
    data->error.assign("Failure calling GetDetailsLocal");
    LOG(ERROR) << data->error;
    data->result = false;
    data->event.Signal();
  }
}

void PackageKitProxy::OnPackageKitInstallError(dbus::Signal* signal) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  CHECK(signal);
  dbus::MessageReader reader(signal);
  uint32_t code;
  std::string details;
  if (!reader.PopUint32(&code) || !reader.PopString(&details)) {
    HandleInstallCompletion(false, "Failure parsing PackageKit error signal");
    return;
  }
  LOG(ERROR) << "Failure installing Linux package of: " << details;
  HandleInstallCompletion(false, details);
}

void PackageKitProxy::OnPackageKitInstallFinished(dbus::Signal* signal) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  CHECK(signal);
  dbus::MessageReader reader(signal);
  uint32_t exit_code;
  if (!reader.PopUint32(&exit_code)) {
    // We really don't know if this succeeded or failed, but it should be
    // considered over and we will treat it as a failure. This is a really ugly
    // error case to be in but shouldn't happen.
    HandleInstallCompletion(false,
                            "Failure parsing PackageKit finished signal");
    return;
  }
  LOG(INFO) << "Finished installing Linux package result: " << exit_code;
  HandleInstallCompletion(kPackageKitExitCodeSuccess == exit_code,
                          "Exit Code: " + base::IntToString(exit_code));
}

void PackageKitProxy::OnPackageKitInfoError(
    std::shared_ptr<PackageInfoTransactionData> data, dbus::Signal* signal) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  CHECK(signal);
  // Check if we've already indicated we are done.
  if (data->event.IsSignaled())
    return;
  dbus::MessageReader reader(signal);
  uint32_t code;
  std::string details;
  if (!reader.PopUint32(&code) || !reader.PopString(&details)) {
    data->error.assign("Failure parsing PackageKit error signal");
    LOG(ERROR) << data->error;
    // There's something wrong with the D-Bus data, so terminate this operation.
    data->result = false;
    data->event.Signal();
    return;
  }
  LOG(ERROR) << "Failure querying Linux package of: " << details;
  // We will still get a Finished signal where we finalize everything.
  data->error.assign(details);
}

void PackageKitProxy::OnPackageKitInfoFinished(
    std::shared_ptr<PackageInfoTransactionData> data, dbus::Signal* signal) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  CHECK(signal);
  // Check if we've already indicated we are done.
  if (data->event.IsSignaled())
    return;
  dbus::MessageReader reader(signal);
  uint32_t exit_code;
  if (!reader.PopUint32(&exit_code)) {
    // We really don't know if this succeeded or failed, but it should be
    // considered over and we will treat it as a failure. This is a really ugly
    // error case to be in but shouldn't happen.
    data->error.assign("Failure parsing PackageKit finished signal");
    LOG(ERROR) << data->error;
    data->result = false;
    data->event.Signal();
    return;
  }
  LOG(INFO) << "Finished with query for Linux package info";
  // If this is a failure, the error message should have already been set via
  // that callback.
  data->result = kPackageKitExitCodeSuccess == exit_code;
  data->event.Signal();
}

void PackageKitProxy::OnPackageKitInfoDetails(
    std::shared_ptr<PackageInfoTransactionData> data, dbus::Signal* signal) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  CHECK(signal);
  // Check if we've already indicated we are done.
  if (data->event.IsSignaled())
    return;
  dbus::MessageReader reader(signal);
  // Read all of the details on the package and set that in the structure. This
  // is an array of dict entries with string keys and variant values.
  dbus::MessageReader array_reader(nullptr);
  if (!reader.PopArray(&array_reader)) {
    data->error.assign("Failure parsing PackageKit Details signal");
    LOG(ERROR) << data->error;
    // There's something wrong with the D-Bus data, so terminate this operation.
    data->result = false;
    data->event.Signal();
    return;
  }
  while (array_reader.HasMoreData()) {
    dbus::MessageReader dict_entry_reader(nullptr);
    if (array_reader.PopDictEntry(&dict_entry_reader)) {
      dbus::MessageReader value_reader(nullptr);
      std::string name;
      if (!dict_entry_reader.PopString(&name) ||
          !dict_entry_reader.PopVariant(&value_reader)) {
        LOG(WARNING) << "Error popping dictionary entry from D-Bus message";
        continue;
      }
      if (name == kDetailsKeyPackageId) {
        if (!value_reader.PopString(&data->pkg_info->package_id)) {
          LOG(WARNING) << "Error popping package_id from details";
        }
      } else if (name == kDetailsKeyLicense) {
        if (!value_reader.PopString(&data->pkg_info->license)) {
          LOG(WARNING) << "Error popping license from details";
        }
      } else if (name == kDetailsKeyDescription) {
        if (!value_reader.PopString(&data->pkg_info->description)) {
          LOG(WARNING) << "Error popping description from details";
        }
      } else if (name == kDetailsKeyUrl) {
        if (!value_reader.PopString(&data->pkg_info->project_url)) {
          LOG(WARNING) << "Error popping url from details";
        }
      } else if (name == kDetailsKeySize) {
        if (!value_reader.PopUint64(&data->pkg_info->size)) {
          LOG(WARNING) << "Error popping size from details";
        }
      } else if (name == kDetailsKeySummary) {
        if (!value_reader.PopString(&data->pkg_info->summary)) {
          LOG(WARNING) << "Error popping summary from details";
        }
      }
    }
  }
}

void PackageKitProxy::OnPackageKitPropertyChanged(const std::string& name) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (!transaction_properties_)
    return;
  // There's only 2 progress states we actually care about which are logical to
  // report to the user. These are downloading and installing, which correspond
  // to similar experiences in Android and elsewhere. There are various other
  // phases this goes through, but they happen rather quickly and would not be
  // worth informing the user of.
  if (name != transaction_properties_->percentage.name()) {
    // We only want to see progress percentage changes and then we filter these
    // below based on the current status.
    return;
  }
  vm_tools::container::InstallLinuxPackageProgressInfo::Status status;
  switch (transaction_properties_->status.value()) {
    case kPackageKitStatusDownload:
      status =
          vm_tools::container::InstallLinuxPackageProgressInfo::DOWNLOADING;
      break;
    case kPackageKitStatusInstall:
      status = vm_tools::container::InstallLinuxPackageProgressInfo::INSTALLING;
      break;
    default:
      // Not a status state we care about.
      return;
  }
  int percentage = transaction_properties_->percentage.value();
  // PackageKit uses 101 for the percent when it doesn't know, treat that as
  // zero because you see this at the beginning of phases.
  if (percentage == 101)
    percentage = 0;
  observer_->OnInstallProgress(status, percentage);
}

void PackageKitProxy::HandleInstallCompletion(
    bool success, const std::string& failure_reason) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  // If we've already cleared the transaction then don't send a message, we may
  // end up seeing a failure come across multiple ways.
  if (!transaction_properties_)
    return;

  // Destroy our property listener for the transaction to indicate it is done
  // and remove the transaction proxy object from the bus.
  transaction_properties_.reset();
  RemoveObjectProxyOnDBusThread(install_transaction_path_);
  observer_->OnInstallCompletion(success, failure_reason);
}

void PackageKitProxy::OnPackageKitNameOwnerChanged(
    const std::string& old_owner, const std::string& new_owner) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (new_owner.empty() && transaction_properties_) {
    LOG(ERROR) << "Detected PackageKit D-Bus service going down during an "
               << "install, send a failure event";
    HandleInstallCompletion(false, "PackageKit service exited unexpectedly");
  }
}

void PackageKitProxy::OnPackageKitServiceAvailable(bool service_is_available) {
  if (service_is_available) {
    packagekit_service_proxy_->SetNameOwnerChangedCallback(
        base::Bind(&PackageKitProxy::OnPackageKitNameOwnerChanged,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void PackageKitProxy::RemoveObjectProxyOnDBusThread(
    const dbus::ObjectPath& object_path) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  bus_->RemoveObjectProxy(kPackageKitServiceName, object_path,
                          base::Bind(&base::DoNothing));
}

}  // namespace garcon
}  // namespace vm_tools
