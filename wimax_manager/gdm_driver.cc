// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/gdm_driver.h"

#include <base/file_util.h>
#include <base/logging.h>
#include <base/stringprintf.h>
#include <base/utf_string_conversions.h>

#include "wimax_manager/gdm_device.h"
#include "wimax_manager/network.h"

using std::string;
using std::vector;
using std::wstring;

namespace wimax_manager {

namespace {

const size_t kMaxNumberOfDevices = 256;
const size_t kMaxNumberOfProfiles = 8;
const size_t kMaxNumberOfNetworks = 16;

const char kLogDirectory[] = "/var/log/gct";
const char kNonVolatileDirectory[] = "/var/cache/gct";
const char *const InitalDirectoriesToCreate[] = {
  kLogDirectory, kNonVolatileDirectory
};

string GetDeviceStatusDescription(WIMAX_API_DEVICE_STATUS device_status) {
  switch (device_status) {
    case WIMAX_API_DEVICE_STATUS_UnInitialized:
      return "Uninitialized";
    case WIMAX_API_DEVICE_STATUS_RF_OFF_HW_SW:
      return "RF off (both H/W and S/W)";
    case WIMAX_API_DEVICE_STATUS_RF_OFF_HW:
      return "RF off (via H/W switch)";
    case WIMAX_API_DEVICE_STATUS_RF_OFF_SW:
      return "RF off (via S/W switch)";
    case WIMAX_API_DEVICE_STATUS_Ready:
      return "Ready";
    case WIMAX_API_DEVICE_STATUS_Scanning:
      return "Scanning";
    case WIMAX_API_DEVICE_STATUS_Connecting:
      return "Connecting";
    case WIMAX_API_DEVICE_STATUS_Data_Connected:
      return "Connected";
    default:
      return "Invalid";
  }
}

DeviceStatus ConvertDeviceStatus(WIMAX_API_DEVICE_STATUS device_status) {
  switch (device_status) {
    case WIMAX_API_DEVICE_STATUS_RF_OFF_HW_SW:
    case WIMAX_API_DEVICE_STATUS_RF_OFF_HW:
    case WIMAX_API_DEVICE_STATUS_RF_OFF_SW:
      return kDeviceStatusDisabled;
    case WIMAX_API_DEVICE_STATUS_Ready:
      return kDeviceStatusReady;
    case WIMAX_API_DEVICE_STATUS_Scanning:
      return kDeviceStatusScanning;
    case WIMAX_API_DEVICE_STATUS_Connecting:
      return kDeviceStatusConnecting;
    case WIMAX_API_DEVICE_STATUS_Data_Connected:
      return kDeviceStatusConnected;
    default:
      return kDeviceStatusUninitialized;
  }
}

string GetConnectionProgressDescription(
    WIMAX_API_CONNECTION_PROGRESS_INFO connection_progress) {
  switch (connection_progress) {
    case WIMAX_API_DEVICE_CONNECTION_PROGRESS_Ranging:
      return "Ranging";
    case WIMAX_API_DEVICE_CONNECTION_PROGRESS_SBC:
      return "SBC";
    case WIMAX_API_DEVICE_CONNECTION_PROGRESS_EAP_authentication_Device:
      return "Device authentication via EAP";
    case WIMAX_API_DEVICE_CONNECTION_PROGRESS_EAP_authentication_User:
      return "User authentication via EAP";
    case WIMAX_API_DEVICE_CONNECTION_PROGRESS_3_way_handshake:
      return "3-way handshake";
    case WIMAX_API_DEVICE_CONNECTION_PROGRESS_Registration:
      return "Registration";
    case WIMAX_API_DEVICE_CONNECTION_PROGRESS_De_registration:
      return "De-registration";
    case WIMAX_API_DEVICE_CONNECTION_PROGRESS_Registered:
      return "Registered";
    case WIMAX_API_DEVICE_CONNECTION_PROGRESS_Registration_DSX:
      return "Registration DSX";
    default:
      return "Invalid";
  }
}

string GetNetworkTypeDescription(NetworkType network_type) {
  switch (network_type) {
    case kNetworkHome:
      return "Home";
    case kNetworkPartner:
      return "Partner";
    case kNetworkRoamingParnter:
      return "Roaming partner";
    default:
      return "Unknown";
  }
}

NetworkType ConvertNetworkType(WIMAX_API_NETWORK_TYPE network_type) {
  switch (network_type) {
    case WIMAX_API_HOME:
      return kNetworkHome;
    case WIMAX_API_PARTNER:
      return kNetworkPartner;
    case WIMAX_API_ROAMING_PARTNER:
      return kNetworkRoamingParnter;
    default:
      return kNetworkUnknown;
  }
}

template <size_t N>
bool ConvertWideCharacterArrayToUTF8String(const wchar_t (&wide_char_array)[N],
                                           std::string *utf8_string) {
  // Check if the wide character array is NULL-terminated.
  if (wmemchr(wide_char_array, L'\0', N) == NULL)
    return false;

  size_t wide_string_length = wcslen(wide_char_array);
  CHECK_LT(wide_string_length, N);

  return WideToUTF8(wide_char_array, wide_string_length, utf8_string);
}

}  // namespace

GdmDriver::GdmDriver() : api_handle_(NULL) {
}

GdmDriver::~GdmDriver() {
  Finalize();
}

bool GdmDriver::Initialize() {
  CHECK(!api_handle_);

  LOG(INFO) << "Initializing GDM driver";

  if (!CreateInitialDirectories())
    return false;

  GCT_WIMAX_API_PARAM api_param;
  strcpy(api_param.nonvolatile_dir, kNonVolatileDirectory);
  strcpy(api_param.log_path, kLogDirectory);
  api_param.log_level = 1;
  GCT_API_RET ret =
      GAPI_Initialize(GCT_WIMAX_SDK_EMBEDDED_EAP_ENABLED, &api_param);
  if (ret != GCT_API_RET_SUCCESS)
    return false;

  ret = GAPI_WiMaxAPIOpen(&api_handle_, GCT_WIMAX_API_OPEN_MODE_NORMAL);
  if (ret != GCT_API_RET_SUCCESS) {
    GAPI_DeInitialize();
    api_handle_ = NULL;
    return false;
  }

  return true;
}

bool GdmDriver::Finalize() {
  if (!api_handle_)
    return true;

  LOG(INFO) << "Finalizing GDM driver";

  bool success = true;
  GAPI_SetDebugLevel(api_handle_, GAPI_LOG_FLUSH_LEVEL, NULL);
  GCT_API_RET ret = GAPI_WiMaxAPIClose(api_handle_);
  api_handle_ = NULL;

  if (ret != GCT_API_RET_SUCCESS)
    success = false;

  ret = GAPI_DeInitialize();
  if (ret != GCT_API_RET_SUCCESS)
    success = false;

  return success;
}

bool GdmDriver::GetDevices(vector<Device *> *devices) {
  CHECK(devices);

  WIMAX_API_HW_DEVICE_ID device_list[kMaxNumberOfDevices];
  uint32 num_devices = static_cast<uint32>(arraysize(device_list));
  GCT_API_RET ret = GAPI_GetListDevice(api_handle_, device_list, &num_devices);
  if (ret != GCT_API_RET_SUCCESS)
    return false;

  LOG(INFO) << "Number of devices: " << num_devices;
  for (size_t i = 0; i < num_devices; ++i) {
    uint8 device_index = device_list[i].deviceIndex;
    string device_name;
    if (!ConvertWideCharacterArrayToUTF8String(device_list[i].deviceName,
                                               &device_name)) {
      LOG(ERROR) << base::StringPrintf(
          "Ignoring device with index %d due to invalid device name",
          device_index);
      continue;
    }

    LOG(INFO) << base::StringPrintf("Found device '%s': index = %d",
                                    device_name.c_str(),
                                    device_index);

    GdmDevice *device =
        new(std::nothrow) GdmDevice(device_index, device_name, AsWeakPtr());
    CHECK(device);
    devices->push_back(device);
  }
  return true;
}

bool GdmDriver::OpenDevice(GdmDevice *device) {
  GDEV_ID device_id = GetDeviceId(device);
  GCT_API_RET ret = GAPI_WiMaxDeviceOpen(&device_id);
  if (ret != GCT_API_RET_SUCCESS)
    return false;

  WIMAX_API_DEVICE_INFO device_info;
  ret = GAPI_GetDeviceInformation(&device_id, &device_info);
  if (ret != GCT_API_RET_SUCCESS)
    return CloseDevice(device);

  device->set_mac_address(device_info.macAddress);

  LOG(INFO) << "Opened device '" << device->name()
            << "': MAC address = "
            << base::StringPrintf("%2x:%02x:%02x:%02x:%02x:%02x",
                                  device_info.macAddress[0],
                                  device_info.macAddress[1],
                                  device_info.macAddress[2],
                                  device_info.macAddress[3],
                                  device_info.macAddress[4],
                                  device_info.macAddress[5]);
  return true;
}

bool GdmDriver::CloseDevice(GdmDevice *device) {
  GDEV_ID device_id = GetDeviceId(device);
  GCT_API_RET ret = GAPI_WiMaxDeviceClose(&device_id);
  return ret == GCT_API_RET_SUCCESS;
}

bool GdmDriver::GetDeviceStatus(GdmDevice *device) {
  GDEV_ID device_id = GetDeviceId(device);
  WIMAX_API_DEVICE_STATUS device_status;
  WIMAX_API_CONNECTION_PROGRESS_INFO connection_progress;
  GCT_API_RET ret =
      GAPI_GetDeviceStatus(&device_id, &device_status, &connection_progress);
  if (ret != GCT_API_RET_SUCCESS)
    return false;

  device->SetStatus(ConvertDeviceStatus(device_status));
  device->set_connection_progress(connection_progress);

  LOG(INFO) << "Device '" << device->name()
            << "': status = '" << GetDeviceStatusDescription(device_status)
            << "', connection progress = '"
            << GetConnectionProgressDescription(connection_progress) << "'";
  return true;
}

bool GdmDriver::SetDeviceEAPParameters(GdmDevice *device,
                                       GCT_API_EAP_PARAM *eap_parameters) {
  GDEV_ID device_id = GetDeviceId(device);
  GCT_API_RET ret = GAPI_SetEap(&device_id, eap_parameters);
  return ret == GCT_API_RET_SUCCESS;
}

bool GdmDriver::AutoSelectProfileForDevice(GdmDevice *device) {
  GDEV_ID device_id = GetDeviceId(device);
  WIMAX_API_PROFILE_INFO profile_list[kMaxNumberOfProfiles];
  uint32 num_profiles = static_cast<uint32>(arraysize(profile_list));
  GCT_API_RET ret =
      GAPI_GetSelectProfileList(&device_id, profile_list, &num_profiles);
  if (ret != GCT_API_RET_SUCCESS)
    return false;

  LOG(INFO) << "Number of profiles: " << num_profiles;
  for (size_t i = 0; i < num_profiles; ++i) {
    string profile_name;
    if (ConvertWideCharacterArrayToUTF8String(profile_list[i].profileName,
                                              &profile_name)) {
      LOG(INFO) << base::StringPrintf("Found profile '%s': id = %d",
                profile_name.c_str(),
                profile_list[i].profileID);
    }
  }

  if (num_profiles == 0)
    return false;

  ret = GAPI_SetProfile(&device_id, profile_list[0].profileID);
  return ret == GCT_API_RET_SUCCESS;
}

bool GdmDriver::PowerOnDeviceRF(GdmDevice *device) {
  GDEV_ID device_id = GetDeviceId(device);
  GCT_API_RET ret = GAPI_CmdControlPowerManagement(&device_id, WIMAX_API_RF_ON);
  return ret == GCT_API_RET_SUCCESS;
}

bool GdmDriver::PowerOffDeviceRF(GdmDevice *device) {
  GDEV_ID device_id = GetDeviceId(device);
  GCT_API_RET ret =
      GAPI_CmdControlPowerManagement(&device_id, WIMAX_API_RF_OFF);
  return ret == GCT_API_RET_SUCCESS;
}

bool GdmDriver::GetNetworksForDevice(GdmDevice *device,
                                     vector<NetworkRefPtr> *networks) {
  CHECK(networks);

  GDEV_ID device_id = GetDeviceId(device);
  WIMAX_API_NSP_INFO network_list[kMaxNumberOfNetworks];
  uint32 num_networks = static_cast<uint32>(arraysize(network_list));
  GCT_API_RET ret = GAPI_GetNetworkList(&device_id, network_list,
                                        &num_networks);
  if (ret != GCT_API_RET_SUCCESS)
    return false;

  LOG(INFO) << "Number of networks: " << num_networks;
  for (size_t i = 0; i < num_networks; ++i) {
    uint32 network_id = network_list[i].NSPid;
    string network_name;
    if (!ConvertWideCharacterArrayToUTF8String(network_list[i].NSPName,
                                               &network_name)) {
      LOG(ERROR) << base::StringPrintf(
          "Ignoring network with identifer %08x due to invalid network name",
          network_id);
      continue;
    }

    wstring network_name_wcs;
    if (!UTF8ToWide(network_name.c_str(), network_name.size(),
                    &network_name_wcs) ||
        (wcscmp(network_name_wcs.c_str(), network_list[i].NSPName) != 0)) {
      LOG(ERROR) << base::StringPrintf(
          "Ignoring network with identifer %08x "
          "due to conversation error of network name",
          network_id);
      continue;
    }

    NetworkType network_type = ConvertNetworkType(network_list[i].networkType);
    int network_cinr = network_list[i].CINR + Network::kMinCINR;
    int network_rssi = network_list[i].RSSI + Network::kMinRSSI;
    LOG(INFO) << base::StringPrintf(
        "Found network '%s': type = '%s', id = %08x, CINR = %d, RSSI = %d",
        network_name.c_str(), GetNetworkTypeDescription(network_type).c_str(),
        network_id, network_cinr, network_rssi);

    NetworkRefPtr network = new(std::nothrow) Network(
        network_id, network_name, network_type, network_cinr, network_rssi);
    CHECK(network);
    networks->push_back(network);
  }
  return true;
}

bool GdmDriver::ConnectDeviceToNetwork(GdmDevice *device,
                                       const Network &network) {
  GDEV_ID device_id = GetDeviceId(device);
  wstring network_name_wcs;
  if (!UTF8ToWide(network.name().c_str(), network.name().size(),
                  &network_name_wcs)) {
    return false;
  }

  GCT_API_RET ret =
      GAPI_CmdConnectToNetwork(&device_id,
                               const_cast<wchar_t *>(network_name_wcs.c_str()),
                               0);
  return ret == GCT_API_RET_SUCCESS;
}

bool GdmDriver::DisconnectDeviceFromNetwork(GdmDevice *device) {
  GDEV_ID device_id = GetDeviceId(device);
  GCT_API_RET ret = GAPI_CmdDisconnectFromNetwork(&device_id);
  return ret == GCT_API_RET_SUCCESS;
}

bool GdmDriver::CreateInitialDirectories() const {
  for (size_t i = 0; i < arraysize(InitalDirectoriesToCreate); ++i) {
    const char *directory = InitalDirectoriesToCreate[i];
    if (!file_util::CreateDirectory(FilePath(directory))) {
      LOG(ERROR) << "Failed to create directory '" << directory << "'";
      return false;
    }
  }
  return true;
}

GDEV_ID GdmDriver::GetDeviceId(const GdmDevice *device) const {
  CHECK(device);

  GDEV_ID device_id;
  device_id.apiHandle = api_handle_;
  device_id.deviceIndex = device->index();
  return device_id;
}

}  // namespace wimax_manager
