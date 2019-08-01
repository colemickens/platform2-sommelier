// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/gatt.h"

#include <string>
#include <utility>

namespace bluetooth {

namespace {

constexpr char kGattHumanInterfaceDeviceServiceUuid[] =
    "00001812-0000-1000-8000-00805f9b34fb";

// TODO(b:134425062): Implement profile manager so that the blacklist can be
// removed.
// Some GATT service should be kept only for newblued used due to security
// concern, and this works as a blacklist-filter.
bool IsHiddenGattService(const GattService& service) {
  std::string uuid = service.uuid().value().canonical_value();
  if (uuid.compare(kGattHumanInterfaceDeviceServiceUuid) == 0)
    return true;
  return false;
}

}  // namespace

Gatt::Gatt(Newblue* newblue, DeviceInterfaceHandler* device_interface_handler)
    : newblue_(newblue),
      device_interface_handler_(device_interface_handler),
      weak_ptr_factory_(this) {
  CHECK(newblue_);
  CHECK(device_interface_handler_);

  device_interface_handler_->AddDeviceObserver(this);
}

Gatt::~Gatt() {
  if (device_interface_handler_)
    device_interface_handler_->RemoveDeviceObserver(this);
}

void Gatt::AddGattObserver(GattObserver* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void Gatt::RemoveGattObserver(GattObserver* observer) {
  CHECK(observer);
  observers_.RemoveObserver(observer);
}

void Gatt::OnGattConnected(const std::string& device_address,
                           gatt_client_conn_t conn_id) {
  CHECK(!device_address.empty());

  auto services = remote_services_.find(device_address);
  if (services != remote_services_.end()) {
    LOG(WARNING) << "GATT cache for device " << device_address
                 << " was not cleared, clear it";
    remote_services_.erase(services);
  }

  // Start GATT browsing.
  UniqueId transaction_id = GetNextId();
  ClientOperation operation = {.type = GattClientOperationType::SERVICES_ENUM,
                               .conn_id = conn_id};
  transactions_.emplace(transaction_id, std::move(operation));

  GattClientOperationStatus status = newblue_->GattClientEnumServices(
      conn_id, true, transaction_id,
      base::Bind(&Gatt::OnGattClientEnumServices,
                 weak_ptr_factory_.GetWeakPtr()));
  if (status != GattClientOperationStatus::OK) {
    LOG(ERROR) << "Failed to browse GATT for device " << device_address
               << " with conn ID " << conn_id;
    transactions_.erase(transaction_id);
  }

  VLOG(1) << "Start GATT browsing for device " << device_address
          << ", transaction " << transaction_id;
}

void Gatt::OnGattDisconnected(const std::string& device_address,
                              gatt_client_conn_t conn_id) {
  CHECK(!device_address.empty());

  // TODO(b:137581907): Investigate if the removal of transaction can rely on
  // the callbacks of GATT operations.
  for (std::map<UniqueId, ClientOperation>::iterator transaction =
           transactions_.begin();
       transaction != transactions_.end();) {
    if (transaction->second.conn_id == conn_id) {
      VLOG(2) << "Clear ongoing GATT client transaction " << transaction->first
              << " with device " << device_address;
      transactions_.erase(transaction++);
    } else {
      ++transaction;
    }
  }

  device_interface_handler_->SetGattServicesResolved(device_address,
                                                     false /* resolved */);

  const auto services = remote_services_.find(device_address);
  if (services != remote_services_.end()) {
    VLOG(1) << "Clear the cached GATT services of device " << device_address;
    for (auto& observer : observers_) {
      for (const auto& service : services->second) {
        if (IsHiddenGattService(*service.second))
          continue;

        for (const auto& characteristic : service.second->characteristics()) {
          for (const auto& descriptor : characteristic.second->descriptors())
            observer.OnGattDescriptorRemoved(*descriptor.second);

          observer.OnGattCharacteristicRemoved(*characteristic.second);
        }
        observer.OnGattServiceRemoved(*service.second);
      }
    }
    remote_services_.erase(device_address);
  }
}

void Gatt::TravPrimaryServices(const std::string& device_address,
                               gatt_client_conn_t conn_id) {
  auto services = remote_services_.find(device_address);

  if (services == remote_services_.end()) {
    LOG(WARNING) << "Failed to find remote services associated with device "
                 << device_address;
    return;
  }

  for (const auto& service_entry : services->second) {
    GattService* service = service_entry.second.get();

    if (!service->primary().value())
      continue;

    UniqueId transaction_id = GetNextId();
    ClientOperation operation = {
        .type = GattClientOperationType::PRIMARY_SERVICE_TRAV,
        .conn_id = conn_id};
    transactions_.emplace(transaction_id, std::move(operation));

    GattClientOperationStatus status = newblue_->GattClientTravPrimaryService(
        conn_id, service->uuid().value(), transaction_id,
        base::Bind(&Gatt::OnGattClientTravPrimaryService,
                   weak_ptr_factory_.GetWeakPtr()));
    if (status != GattClientOperationStatus::OK) {
      LOG(ERROR) << "Failed to traverse GATT primary service "
                 << service->uuid().value().canonical_value() << " for device "
                 << device_address << " with conn ID " << conn_id;
      transactions_.erase(transaction_id);
    } else {
      VLOG(1) << "Start traversing GATT primary service "
              << service->uuid().value().canonical_value() << " for device "
              << device_address << ", transaction " << transaction_id;
    }
  }
}

void Gatt::OnGattClientEnumServices(bool finished,
                                    gatt_client_conn_t conn_id,
                                    UniqueId transaction_id,
                                    Uuid uuid,
                                    bool primary,
                                    uint16_t first_handle,
                                    uint16_t num_handles,
                                    GattClientOperationStatus status) {
  auto transaction = transactions_.find(transaction_id);
  CHECK(transaction != transactions_.end());
  CHECK(transaction->second.type == GattClientOperationType::SERVICES_ENUM);

  if (status != GattClientOperationStatus::OK) {
    LOG(ERROR) << "Error GATT client operation, dropping it";
    return;
  }

  // This may be invoked after device is removed, so we check whether the device
  // is still valid.
  std::string device_address =
      device_interface_handler_->GetAddressByConnectionId(conn_id);
  if (device_address.empty()) {
    LOG(WARNING) << "Unknown GATT connection " << conn_id
                 << " for service enumeration result";
    return;
  }

  // Close the transaction when the service enumeration finished.
  if (finished) {
    VLOG(1) << "GATT browsing finished for device " << device_address
            << ", transaction " << transaction_id;
    transactions_.erase(transaction_id);

    // Start primary services traversal.
    TravPrimaryServices(device_address, conn_id);
    return;
  }

  VLOG(2) << "GATT Browsing continues on device " << device_address
          << ", transaction " << transaction_id << ", found "
          << uuid.canonical_value();

  if (!base::ContainsKey(remote_services_, device_address)) {
    std::map<uint16_t, std::unique_ptr<GattService>> services;
    remote_services_.emplace(device_address, std::move(services));
  }

  auto services = remote_services_.find(device_address);
  services->second.emplace(first_handle,
                           std::make_unique<GattService>(
                               device_address, first_handle,
                               first_handle + num_handles - 1, primary, uuid));
  for (auto& observer : observers_) {
    if (IsHiddenGattService(*services->second.at(first_handle)))
      continue;

    observer.OnGattServiceAdded(
        *remote_services_.at(device_address).at(first_handle));
  }
}

void Gatt::OnGattClientTravPrimaryService(
    gatt_client_conn_t conn_id,
    UniqueId transaction_id,
    std::unique_ptr<GattService> service) {
  auto transaction = transactions_.find(transaction_id);
  CHECK(transaction != transactions_.end());
  CHECK(transaction->second.type ==
        GattClientOperationType::PRIMARY_SERVICE_TRAV);

  // This may be invoked after device is removed, so we check whether the device
  // is still valid.
  std::string device_address =
      device_interface_handler_->GetAddressByConnectionId(conn_id);
  if (device_address.empty()) {
    LOG(WARNING) << "Unknown GATT connection " << conn_id
                 << " for primary service traversal result";
    transactions_.erase(transaction_id);
    return;
  }

  if (service == nullptr) {
    LOG(ERROR) << "Primary service traversal failed with device "
               << device_address;
    transactions_.erase(transaction_id);
    return;
  }

  auto services = remote_services_.find(device_address);
  if (services == remote_services_.end()) {
    LOG(WARNING) << "No remote services associated with device "
                 << device_address << ", dropping it";
    transactions_.erase(transaction_id);
    return;
  }

  // If there is service change before the traversal finished where the service
  // is no longer there, we drop the result.
  auto srv = services->second.find(service->first_handle());
  if (srv == services->second.end()) {
    LOG(WARNING) << "Unknown primary service "
                 << service->uuid().value().canonical_value()
                 << ", dropping it";
    transactions_.erase(transaction_id);
    return;
  }

  CHECK(srv->second->device_address().value() == device_address);

  VLOG(2) << "Replacing service " << service->uuid().value().canonical_value()
          << " of device " << device_address
          << " with the traversed one, transaction id " << transaction_id;

  service->SetDeviceAddress(device_address);
  srv->second = std::move(service);
  transactions_.erase(transaction_id);

  // Notify observers on service-changed, characteristic-added and
  // descriptor-added events.
  for (auto& observer : observers_) {
    if (IsHiddenGattService(*srv->second))
      continue;

    observer.OnGattServiceChanged(*srv->second);
    for (const auto& characteristic : srv->second->characteristics()) {
      observer.OnGattCharacteristicAdded(*characteristic.second);

      for (const auto& descriptor : characteristic.second->descriptors())
        observer.OnGattDescriptorAdded(*descriptor.second);
    }
  }

  // Notify device interface handler about the completion of traversal.
  if (IsTravPrimaryServicesCompleted(conn_id)) {
    VLOG(1) << "GATT primary services traversal finished for device "
            << device_address;
    device_interface_handler_->SetGattServicesResolved(device_address,
                                                       true /* resolved */);
  }
}

bool Gatt::IsTravPrimaryServicesCompleted(gatt_client_conn_t conn_id) {
  for (const auto& transaction : transactions_) {
    if (transaction.second.conn_id == conn_id &&
        transaction.second.type ==
            GattClientOperationType::PRIMARY_SERVICE_TRAV) {
      return false;
    }
  }
  return true;
}

}  // namespace bluetooth
