/*===========================================================================
  FILE:
  gobi_sdk_wrapper.cc,
  derived from
  QCWWANCMAPI2k.h
  which has the following copyright notice:

  DESCRIPTION:
  QUALCOMM Wireless WAN Connection Manager API for Gobi 2000

  Copyright (C) 2009 QUALCOMM Incorporated. All rights reserved.
  QUALCOMM Proprietary/GTDR

  All data and information contained in or disclosed by this document is
  confidential and proprietary information of QUALCOMM Incorporated and all
  rights therein are expressly reserved.  By accepting this material the
  recipient agrees that this material and the information contained therein
  is held in confidence and in trust and will not be used, copied, reproduced
  in whole or in part, nor its contents revealed in any manner to others
  without the express written permission of QUALCOMM Incorporated.
  ==========================================================================*/

#include "gobi_sdk_wrapper.h"

#include <glog/logging.h>

#include "QCWWANCMAPI2k.h"

namespace gobi {

static const char *kServiceMapping[] = {
  // Entries starting with "+" define a new service.  The rest are
  // function names that belong to that service.  These names are
  // taken from the tables at the beginning of service in the CMAPI
  // document.

  // Base is special--it must be the first group and it is
  // special-cased so that we cannot use any other function if we are
  // using a function in Base.  See also Sdk::GetServiceBound().
  "+Base",
  "QCWWANConnect",
  "QCWWANDisconnect",

  "+DeviceConnectivity",
  "QCWWANEnumerateDevices",
  "QCWWANGetConnectedDeviceID",
  "QCWWANCancel",

  "+WirelessData",
  "GetSessionState",
  "StartDataSession",
  "CancelDataSession",
  "StopDataSession",
  "GetIPAddress",
  "GetConnectionRate",
  "GetPacketStatus",
  "SetMobileIP",
  "GetMobileIP",
  "SetActiveMobileIPProfile",
  "GetActiveMobileIPProfile",
  "SetMobileIPProfile",
  "GetMobileIPProfile",
  "SetMobileIPParameters",
  "GetMobileIPParameters",
  "GetLastMobileIPError",
  "GetAutoconnect",
  "SetAutoconnect",
  "SetDefaultProfile",
  "GetDefaultProfile",
  "GetDormancyState",
  "GetDataBearerTechnology",
  "GetByteTotals",
  "GetSessionDuration",

  "+NetworkAccess",
  "GetSignalStrengths",
  "GetRFInfo",
  "PerformNetworkScan",
  "InitiateNetworkRegistration",
  "InitiateDomainAttach",
  "GetServingNetwork",
  "GetServingNetworkCapabilities",
  "GetHomeNetwork",
  "GetNetworkPreference",
  "SetNetworkPreference",
  "SetCDMANetworkParameters",
  "GetCDMANetworkParameters",
  "GetACCOLC",
  "SetACCOLC",
  "GetANAAAAuthenticationStatus",

  "+DeviceManagement",
  "GetDeviceCapabilities",
  "GetManufacturer",
  "GetModelID",
  "GetFirmwareRevision",
  "GetFirmwareRevisions",
  "GetVoiceNumber",
  "GetIMSI",
  "GetSerialNumbers",
  "GetHardwareRevision",
  "GetPRLVersion",
  "GetERIFile",
  "ActivateAutomatic",
  "ActivateManual",
  "GetActivationState",
  "SetPower",
  "GetPower",
  "GetOfflineReason",
  "GetNetworkTime",
  "UIMSetPINProtection",
  "UIMVerifyPIN",
  "UIMUnblockPIN",
  "UIMChangePIN",
  "UIMGetPINStatus",
  "UIMGetICCID",
  "UIMGetControlKeyStatus",
  "UIMGetControlKeyBlockingStatus",
  "UIMSetControlKeyProtection",
  "UIMUnblockControlKey",
  "ResetToFactoryDefaults",
  "ValidateSPC",

  "+SMS",
  "DeleteSMS",
  "GetSMSList",
  "GetSMS",
  "ModifySMSStatus",
  "SaveSMS",
  "SendSMS",
  "GetSMSCAddress",
  "SetSMSCAddress",
  "GetSMSRoutes",
  "SetSMSRoutes",

  "+Firmware",
  "UpgradeFirmware",
  "GetImageStore",
  "GetImageInfo",
  "GetFirmwareInfo",

  "+PositionDetermination",
  "GetPDSState",
  "SetPDSState",
  "PDSInjectTimeReference",
  "GetPDSDefaults",
  "SetPDSDefaults",
  "GetXTRAAutomaticDownload",
  "SetXTRAAutomaticDownload",
  "GetXTRANetwork",
  "SetXTRANetwork",
  "GetXTRAValidity",
  "ForceXTRADownload",
  "GetAGPSConfig",
  "SetAGPSConfig",
  "GetServiceAutomaticTracking",
  "SetServiceAutomaticTracking",
  "GetPortAutomaticTracking",
  "SetPortAutomaticTracking",
  "ResetPDSData",

  "+CardApplication",
  "CATSendTerminalResponse",
  "CATSendEnvelopeCommand",

  "+RemoteManagement"
  "GetSMSWake",
  "SetSMSWake",

  "+OMADM"
  "OMADMStartSession",
  "OMADMCancelSession",
  "OMADMGetSessionInfo",
  "OMADMGetPendingNIA",
  "OMADMSendSelection",
  "OMADMGetFeatureSettings",
  "OMADMSetProvisioningFeature",
  "OMADMSetPRLUpdateFeature",

  "+Callback",
  "SetSessionStateCallback",
  "SetDataBearerCallback",
  "SetDormancyStatusCallback",
  "SetMobileIPStatusCallback",
  "SetActivationStatusCallback",
  "SetPowerCallback",
  "SetRoamingIndicatorCallback",
  "SetSignalStrengthCallback",
  "SetRFInfoCallback",
  "SetLURejectCallback",
  "SetNMEACallback",
  "SetNMEAPlusCallback",
  "SetPDSStateCallback",
  "SetNewSMSCallback",
  "SetDataCapabilitiesCallback",
  "SetByteTotalsCallback",
  "SetCATEventCallback",
  "SetOMADMAlertCallback",
  "SetOMADMStateCallback",

  NULL
};

// TODO(rochberg):  Fix the crasher that this code is masking
static int CALL_COUNT = 0;

struct ReentrancyPreventer {
  ReentrancyPreventer(Sdk *sdk, const char *name)
      : sdk_(sdk),
        function_name_(name) {
    if ((CALL_COUNT++ % 16) == 0) {
      LOG(WARNING) << "SDK reentrancy checking disabled: "
                   << "mail rochberg@chromium.org if you see this message "
                   << "after noon PDT 11 August 2010";
    }
    // sdk_->EnterSdk(function_name_);
  }
  ~ReentrancyPreventer() {
    // sdk_->LeaveSdk(function_name_);
  }
  Sdk *sdk_;
  const char *function_name_;
 private:
  DISALLOW_COPY_AND_ASSIGN(ReentrancyPreventer);
};

void Sdk::Init() {
  InitGetServiceFromName(kServiceMapping);
  service_to_function_.insert(
      service_to_function_.end(),
      service_index_upper_bound_,
      static_cast<const char *>(NULL));
  pthread_mutex_init(&service_to_function_mutex_, NULL);
}

void Sdk::InitGetServiceFromName(const char *services[]) {
  int service_index = -1;
  for (int i = 0; services[i]; ++i) {
    const char *name = services[i];
    CHECK(*name != 0) << "Empty servicename: " << i;
    if (name[0] == '+') {
      service_index++;
      index_to_service_name_[service_index] = name + 1;
    } else {
      CHECK(service_index >= 0);
      name_to_service_[std::string(name)] = service_index;
    }
  }

  service_index_upper_bound_ = service_index + 1;
}

int Sdk::GetServiceFromName(const char *name) {
  std::map<std::string, int>::iterator i =
      name_to_service_.find(std::string(name));
  CHECK(i != name_to_service_.end()) << "Invalid function name: " << name;
  return i->second;
}

int Sdk::GetServiceBound(int service) {
  if (service == 0) {
    // Base service: we should prevent all other services from being
    // used
    return service_index_upper_bound_;
  } else {
    return service + 1;
  }
}

void Sdk::EnterSdk(const char *function_name) {
  int rc = pthread_mutex_lock(&service_to_function_mutex_);
  CHECK(rc == 0) << "lock failed: rc = " << rc;

  int service = GetServiceFromName(function_name);
  for (int i = service; i < GetServiceBound(service); ++i) {
    if (service_to_function_[i]) {
      LOG(FATAL) << "Reentrant SDK access detected:  Called "
                 << function_name << " while already in call to "
                 << service_to_function_[i];
    }
    service_to_function_[i] = function_name;
  }
  rc = pthread_mutex_unlock(&service_to_function_mutex_);
  CHECK(rc == 0) << "rc = " << rc;

}

void Sdk::LeaveSdk(const char *function_name) {
  int rc = pthread_mutex_lock(&service_to_function_mutex_);
  CHECK(rc == 0) << "lock failed: rc = " << rc;

  int service = GetServiceFromName(function_name);
  for (int i = service; i < GetServiceBound(service); ++i) {
    if (!service_to_function_[i]) {
      LOG(FATAL) << "Improperly exiting sdk function: "
                 << function_name;
    }
    service_to_function_[i] = NULL;
  }
  rc = pthread_mutex_unlock(&service_to_function_mutex_);
  CHECK(rc == 0) << "rc = " << rc;
}

ULONG Sdk::QCWWANEnumerateDevices(
    BYTE *                     pDevicesSize,
    BYTE *                     pDevices) {
  ReentrancyPreventer rp(this, "QCWWANEnumerateDevices");
  return ::QCWWANEnumerateDevices(pDevicesSize,
                                  pDevices);
}

ULONG Sdk::QCWWANConnect(
    CHAR *                     pDeviceNode,
    CHAR *                     pDeviceKey) {
  ReentrancyPreventer rp(this, "QCWWANConnect");
  return ::QCWWANConnect(pDeviceNode,
                         pDeviceKey);
}

ULONG Sdk::QCWWANDisconnect() {
  ReentrancyPreventer rp(this, "QCWWANDisconnect");
  return ::QCWWANDisconnect();
}

ULONG Sdk::QCWWANGetConnectedDeviceID(
    ULONG                      deviceNodeSize,
    CHAR *                     pDeviceNode,
    ULONG                      deviceKeySize,
    CHAR *                     pDeviceKey) {
  ReentrancyPreventer rp(this, "QCWWANGetConnectedDeviceID");
  return ::QCWWANGetConnectedDeviceID(
      deviceNodeSize,
      pDeviceNode,
      deviceKeySize,
      pDeviceKey);
}

ULONG Sdk::GetSessionState(ULONG * pState) {
  ReentrancyPreventer rp(this, "GetSessionState");
  return ::GetSessionState(pState);
}

ULONG Sdk::GetSessionDuration(ULONGLONG * pDuration) {
  ReentrancyPreventer rp(this, "GetSessionDuration");
  return ::GetSessionDuration(pDuration);
}

ULONG Sdk::GetDormancyState(ULONG * pState) {
  ReentrancyPreventer rp(this, "GetDormancyState");
  return ::GetDormancyState(pState);
}

ULONG Sdk::GetAutoconnect(ULONG * pSetting) {
  ReentrancyPreventer rp(this, "GetAutoconnect");
  return ::GetAutoconnect(pSetting);
}

ULONG Sdk::SetAutoconnect(ULONG setting) {
  ReentrancyPreventer rp(this, "SetAutoconnect");
  return ::SetAutoconnect(setting);
}

ULONG Sdk::SetDefaultProfile(
    ULONG                      profileType,
    ULONG *                    pPDPType,
    ULONG *                    pIPAddress,
    ULONG *                    pPrimaryDNS,
    ULONG *                    pSecondaryDNS,
    ULONG *                    pAuthentication,
    CHAR *                     pName,
    CHAR *                     pAPNName,
    CHAR *                     pUsername,
    CHAR *                     pPassword) {
  ReentrancyPreventer rp(this, "SetDefaultProfile");
  return ::SetDefaultProfile(
      profileType,
      pPDPType,
      pIPAddress,
      pPrimaryDNS,
      pSecondaryDNS,
      pAuthentication,
      pName,
      pAPNName,
      pUsername,
      pPassword);
}

ULONG Sdk::GetDefaultProfile(
    ULONG                      profileType,
    ULONG *                    pPDPType,
    ULONG *                    pIPAddress,
    ULONG *                    pPrimaryDNS,
    ULONG *                    pSecondaryDNS,
    ULONG *                    pAuthentication,
    BYTE                       nameSize,
    CHAR *                     pName,
    BYTE                       apnSize,
    CHAR *                     pAPNName,
    BYTE                       userSize,
    CHAR *                     pUsername) {
  ReentrancyPreventer rp(this, "GetDefaultProfile");
  return ::GetDefaultProfile(
      profileType,
      pPDPType,
      pIPAddress,
      pPrimaryDNS,
      pSecondaryDNS,
      pAuthentication,
      nameSize,
      pName,
      apnSize,
      pAPNName,
      userSize,
      pUsername);
}

ULONG Sdk::StartDataSession(
    ULONG *                    pTechnology,
    CHAR *                     pAPNName,
    ULONG *                    pAuthentication,
    CHAR *                     pUsername,
    CHAR *                     pPassword,
    ULONG *                    pSessionId,
    ULONG *                    pFailureReason
                            ) {
  ReentrancyPreventer rp(this, "StartDataSession");
  return ::StartDataSession(
      pTechnology,
      NULL,
      NULL,
      NULL,
      NULL,
      pAPNName,
      NULL,
      pAuthentication,
      pUsername,
      pPassword,
      pSessionId,
      pFailureReason);
}

ULONG Sdk::StopDataSession(ULONG sessionId) {
  ReentrancyPreventer rp(this, "StopDataSession");
  return ::StopDataSession(sessionId);
}

ULONG Sdk::GetIPAddress(ULONG * pIPAddress) {
  ReentrancyPreventer rp(this, "GetIPAddress");
  return ::GetIPAddress(pIPAddress);
}

ULONG Sdk::GetConnectionRate(
    ULONG *                    pCurrentChannelTXRate,
    ULONG *                    pCurrentChannelRXRate,
    ULONG *                    pMaxChannelTXRate,
    ULONG *                    pMaxChannelRXRate) {
  ReentrancyPreventer rp(this, "GetConnectionRate");
  return ::GetConnectionRate(
      pCurrentChannelTXRate,
      pCurrentChannelRXRate,
      pMaxChannelTXRate,
      pMaxChannelRXRate);
}

ULONG Sdk::GetPacketStatus(
    ULONG *                    pTXPacketSuccesses,
    ULONG *                    pRXPacketSuccesses,
    ULONG *                    pTXPacketErrors,
    ULONG *                    pRXPacketErrors,
    ULONG *                    pTXPacketOverflows,
    ULONG *                    pRXPacketOverflows) {
  ReentrancyPreventer rp(this, "GetPacketStatus");
  return ::GetPacketStatus(
      pTXPacketSuccesses,
      pRXPacketSuccesses,
      pTXPacketErrors,
      pRXPacketErrors,
      pTXPacketOverflows,
      pRXPacketOverflows);
}

ULONG Sdk::GetByteTotals(
    ULONGLONG *                pTXTotalBytes,
    ULONGLONG *                pRXTotalBytes) {
  ReentrancyPreventer rp(this, "GetByteTotals");
  return ::GetByteTotals(
      pTXTotalBytes,
      pRXTotalBytes);
}

ULONG Sdk::SetMobileIP(ULONG mode) {
  ReentrancyPreventer rp(this, "SetMobileIP");
  return ::SetMobileIP(mode);
}

ULONG Sdk::GetMobileIP(ULONG * pMode) {
  ReentrancyPreventer rp(this, "GetMobileIP");
  return ::GetMobileIP(pMode);
}

ULONG Sdk::SetActiveMobileIPProfile(
    CHAR *                     pSPC,
    BYTE                       index) {
  ReentrancyPreventer rp(this, "SetActiveMobileIPProfile");
  return ::SetActiveMobileIPProfile(
      pSPC,
      index);
}

ULONG Sdk::GetActiveMobileIPProfile(BYTE * pIndex) {
  ReentrancyPreventer rp(this, "GetActiveMobileIPProfile");
  return ::GetActiveMobileIPProfile(pIndex);
}

ULONG Sdk::SetMobileIPProfile(
    CHAR *                     pSPC,
    BYTE                       index,
    BYTE *                     pEnabled,
    ULONG *                    pAddress,
    ULONG *                    pPrimaryHA,
    ULONG *                    pSecondaryHA,
    BYTE *                     pRevTunneling,
    CHAR *                     pNAI,
    ULONG *                    pHASPI,
    ULONG *                    pAAASPI,
    CHAR *                     pMNHA,
    CHAR *                     pMNAAA) {
  ReentrancyPreventer rp(this, "SetMobileIPProfile");
  return ::SetMobileIPProfile(
      pSPC,
      index,
      pEnabled,
      pAddress,
      pPrimaryHA,
      pSecondaryHA,
      pRevTunneling,
      pNAI,
      pHASPI,
      pAAASPI,
      pMNHA,
      pMNAAA);
}

ULONG Sdk::GetMobileIPProfile(
    BYTE                       index,
    BYTE *                     pEnabled,
    ULONG *                    pAddress,
    ULONG *                    pPrimaryHA,
    ULONG *                    pSecondaryHA,
    BYTE *                     pRevTunneling,
    BYTE                       naiSize,
    CHAR *                     pNAI,
    ULONG *                    pHASPI,
    ULONG *                    pAAASPI,
    ULONG *                    pHAState,
    ULONG *                    pAAAState) {
  ReentrancyPreventer rp(this, "GetMobileIPProfile");
  return ::GetMobileIPProfile(
      index,
      pEnabled,
      pAddress,
      pPrimaryHA,
      pSecondaryHA,
      pRevTunneling,
      naiSize,
      pNAI,
      pHASPI,
      pAAASPI,
      pHAState,
      pAAAState);
}

ULONG Sdk::SetMobileIPParameters(
    CHAR *                     pSPC,
    ULONG *                    pMode,
    BYTE *                     pRetryLimit,
    BYTE *                     pRetryInterval,
    BYTE *                     pReRegPeriod,
    BYTE *                     pReRegTraffic,
    BYTE *                     pHAAuthenticator,
    BYTE *                     pHA2002bis) {
  ReentrancyPreventer rp(this, "SetMobileIPParameters");
  return ::SetMobileIPParameters(
      pSPC,
      pMode,
      pRetryLimit,
      pRetryInterval,
      pReRegPeriod,
      pReRegTraffic,
      pHAAuthenticator,
      pHA2002bis);
}

ULONG Sdk::GetMobileIPParameters(
    ULONG *                    pMode,
    BYTE *                     pRetryLimit,
    BYTE *                     pRetryInterval,
    BYTE *                     pReRegPeriod,
    BYTE *                     pReRegTraffic,
    BYTE *                     pHAAuthenticator,
    BYTE *                     pHA2002bis) {
  ReentrancyPreventer rp(this, "GetMobileIPParameters");
  return ::GetMobileIPParameters(
      pMode,
      pRetryLimit,
      pRetryInterval,
      pReRegPeriod,
      pReRegTraffic,
      pHAAuthenticator,
      pHA2002bis);
}

ULONG Sdk::GetLastMobileIPError(ULONG * pError) {
  ReentrancyPreventer rp(this, "GetLastMobileIPError");
  return ::GetLastMobileIPError(pError);
}

ULONG Sdk::GetANAAAAuthenticationStatus(ULONG * pStatus) {
  ReentrancyPreventer rp(this, "GetANAAAAuthenticationStatus");
  return ::GetANAAAAuthenticationStatus(pStatus);
}

ULONG Sdk::GetSignalStrengths(
    ULONG *                    pArraySizes,
    INT8 *                     pSignalStrengths,
    ULONG *                    pRadioInterfaces) {
  ReentrancyPreventer rp(this, "GetSignalStrengths");
  return ::GetSignalStrengths(
      pArraySizes,
      pSignalStrengths,
      pRadioInterfaces);
}

ULONG Sdk::GetRFInfo(
    BYTE *                     pInstanceSize,
    BYTE *                     pInstances) {
  ReentrancyPreventer rp(this, "GetRFInfo");
  return ::GetRFInfo(
      pInstanceSize,
      pInstances);
}

ULONG Sdk::PerformNetworkScan(
    BYTE *                     pInstanceSize,
    BYTE *                     pInstances) {
  ReentrancyPreventer rp(this, "PerformNetworkScan");
  return ::PerformNetworkScan(
      pInstanceSize,
      pInstances);
}

ULONG Sdk::InitiateNetworkRegistration(
    ULONG                      regType,
    WORD                       mcc,
    WORD                       mnc,
    ULONG                      rat) {
  ReentrancyPreventer rp(this, "InitiateNetworkRegistration");
  return ::InitiateNetworkRegistration(
      regType,
      mcc,
      mnc,
      rat);
}

ULONG Sdk::InitiateDomainAttach(ULONG action) {
  ReentrancyPreventer rp(this, "InitiateDomainAttach");
  return ::InitiateDomainAttach(action);
}

ULONG Sdk::GetServingNetwork(
    ULONG *                    pRegistrationState,
    BYTE *                     pRadioIfacesSize,
    BYTE *                     pRadioIfaces,
    ULONG *                    pRoaming) {
  CHAR netName[32];
  ULONG l1, l2, l3;
  WORD w4, w5;
  ReentrancyPreventer rp(this, "GetServingNetwork");
  return ::GetServingNetwork(
      pRegistrationState,
      &l1,              // CS domain
      &l2,              // PS domain
      &l3,              // radio access network type
      pRadioIfacesSize,
      pRadioIfaces,
      pRoaming,
      &w4,              // mobile country code
      &w5,              // mobile network code
      sizeof(netName),  // size of name array
      netName           // network name
                             );
}

ULONG Sdk::GetServingNetworkCapabilities(
    BYTE *                     pDataCapsSize,
    BYTE *                     pDataCaps) {
  ReentrancyPreventer rp(this, "GetServingNetworkCapabilities");
  return ::GetServingNetworkCapabilities(
      pDataCapsSize,
      pDataCaps);
}

ULONG Sdk::GetDataBearerTechnology(ULONG * pDataBearer) {
  ReentrancyPreventer rp(this, "GetDataBearerTechnology");
  return ::GetDataBearerTechnology(pDataBearer);
}

ULONG Sdk::GetHomeNetwork(
    WORD *                     pMCC,
    WORD *                     pMNC,
    BYTE                       nameSize,
    CHAR *                     pName,
    WORD *                     pSID,
    WORD *                     pNID) {
  ReentrancyPreventer rp(this, "GetHomeNetwork");
  return ::GetHomeNetwork(
      pMCC,
      pMNC,
      nameSize,
      pName,
      pSID,
      pNID);
}

ULONG Sdk::SetNetworkPreference(
    ULONG                      technologyPref,
    ULONG                      duration) {
  ReentrancyPreventer rp(this, "SetNetworkPreference");
  return ::SetNetworkPreference(
      technologyPref,
      duration);
}

ULONG Sdk::GetNetworkPreference(
    ULONG *                    pTechnologyPref,
    ULONG *                    pDuration,
    ULONG *                    pPersistentTechnologyPref) {
  ReentrancyPreventer rp(this, "GetNetworkPreference");
  return ::GetNetworkPreference(
      pTechnologyPref,
      pDuration,
      pPersistentTechnologyPref);
}

ULONG Sdk::SetCDMANetworkParameters(
    CHAR *                     pSPC,
    BYTE *                     pForceRev0,
    BYTE *                     pCustomSCP,
    ULONG *                    pProtocol,
    ULONG *                    pBroadcast,
    ULONG *                    pApplication,
    ULONG *                    pRoaming) {
  ReentrancyPreventer rp(this, "SetCDMANetworkParameters");
  return ::SetCDMANetworkParameters(
      pSPC,
      pForceRev0,
      pCustomSCP,
      pProtocol,
      pBroadcast,
      pApplication,
      pRoaming);
}

ULONG Sdk::GetCDMANetworkParameters(
    BYTE *                     pSCI,
    BYTE *                     pSCM,
    BYTE *                     pRegHomeSID,
    BYTE *                     pRegForeignSID,
    BYTE *                     pRegForeignNID,
    BYTE *                     pForceRev0,
    BYTE *                     pCustomSCP,
    ULONG *                    pProtocol,
    ULONG *                    pBroadcast,
    ULONG *                    pApplication,
    ULONG *                    pRoaming) {
  ReentrancyPreventer rp(this, "GetCDMANetworkParameters");
  return ::GetCDMANetworkParameters(
      pSCI,
      pSCM,
      pRegHomeSID,
      pRegForeignSID,
      pRegForeignNID,
      pForceRev0,
      pCustomSCP,
      pProtocol,
      pBroadcast,
      pApplication,
      pRoaming);
}

ULONG Sdk::GetACCOLC(BYTE * pACCOLC) {
  ReentrancyPreventer rp(this, "GetACCOLC");
  return ::GetACCOLC(pACCOLC);
}

ULONG Sdk::SetACCOLC(
    CHAR *                     pSPC,
    BYTE                       accolc) {
  ReentrancyPreventer rp(this, "SetACCOLC");
  return ::SetACCOLC(
      pSPC,
      accolc);
}

ULONG Sdk::GetDeviceCapabilities(
    ULONG *                    pMaxTXChannelRate,
    ULONG *                    pMaxRXChannelRate,
    ULONG *                    pDataServiceCapability,
    ULONG *                    pSimCapability,
    ULONG *                    pRadioIfacesSize,
    BYTE *                     pRadioIfaces) {
  ReentrancyPreventer rp(this, "GetDeviceCapabilities");
  return ::GetDeviceCapabilities(
      pMaxTXChannelRate,
      pMaxRXChannelRate,
      pDataServiceCapability,
      pSimCapability,
      pRadioIfacesSize,
      pRadioIfaces);
}

ULONG Sdk::GetManufacturer(
    BYTE                       stringSize,
    CHAR *                     pString) {
  ReentrancyPreventer rp(this, "GetManufacturer");
  return ::GetManufacturer(
      stringSize,
      pString);
}

ULONG Sdk::GetModelID(
    BYTE                       stringSize,
    CHAR *                     pString) {
  ReentrancyPreventer rp(this, "GetModelID");
  return ::GetModelID(
      stringSize,
      pString);
}

ULONG Sdk::GetFirmwareRevision(
    BYTE                       stringSize,
    CHAR *                     pString) {
  ReentrancyPreventer rp(this, "GetFirmwareRevision");
  return ::GetFirmwareRevision(
      stringSize,
      pString);
}

ULONG Sdk::GetFirmwareRevisions(
    BYTE                       amssSize,
    CHAR *                     pAMSSString,
    BYTE                       bootSize,
    CHAR *                     pBootString,
    BYTE                       priSize,
    CHAR *                     pPRIString) {
  ReentrancyPreventer rp(this, "GetFirmwareRevisions");
  return ::GetFirmwareRevisions(
      amssSize,
      pAMSSString,
      bootSize,
      pBootString,
      priSize,
      pPRIString);
}

ULONG Sdk::GetFirmwareInfo(
    ULONG *                    pFirmwareID,
    ULONG *                    pTechnology,
    ULONG *                    pCarrier,
    ULONG *                    pRegion,
    ULONG *                    pGPSCapability) {
  ReentrancyPreventer rp(this, "GetFirmwareInfo");
  return ::GetFirmwareInfo(
      pFirmwareID,
      pTechnology,
      pCarrier,
      pRegion,
      pGPSCapability);
}

ULONG Sdk::GetVoiceNumber(
    BYTE                       voiceNumberSize,
    CHAR *                     pVoiceNumber,
    BYTE                       minSize,
    CHAR *                     pMIN) {
  ReentrancyPreventer rp(this, "GetVoiceNumber");
  return ::GetVoiceNumber(
      voiceNumberSize,
      pVoiceNumber,
      minSize,
      pMIN);
}

ULONG Sdk::GetIMSI(
    BYTE                       stringSize,
    CHAR *                     pString) {
  ReentrancyPreventer rp(this, "GetIMSI");
  return ::GetIMSI(
      stringSize,
      pString);
}

ULONG Sdk::GetSerialNumbers(
    BYTE                       esnSize,
    CHAR *                     pESNString,
    BYTE                       imeiSize,
    CHAR *                     pIMEIString,
    BYTE                       meidSize,
    CHAR *                     pMEIDString) {
  ReentrancyPreventer rp(this, "GetSerialNumbers");
  return ::GetSerialNumbers(
      esnSize,
      pESNString,
      imeiSize,
      pIMEIString,
      meidSize,
      pMEIDString);
}

ULONG Sdk::SetLock(
    ULONG                      state,
    CHAR *                     pCurrentPIN) {
  ReentrancyPreventer rp(this, "SetLock");
  return ::SetLock(
      state,
      pCurrentPIN);
}

ULONG Sdk::QueryLock(ULONG * pState) {
  ReentrancyPreventer rp(this, "QueryLock");
  return ::QueryLock(pState);
}

ULONG Sdk::ChangeLockPIN(
    CHAR *                     pCurrentPIN,
    CHAR *                     pDesiredPIN) {
  ReentrancyPreventer rp(this, "ChangeLockPIN");
  return ::ChangeLockPIN(
      pCurrentPIN,
      pDesiredPIN);
}

ULONG Sdk::GetHardwareRevision(
    BYTE                       stringSize,
    CHAR *                     pString) {
  ReentrancyPreventer rp(this, "GetHardwareRevision");
  return ::GetHardwareRevision(
      stringSize,
      pString);
}

ULONG Sdk::GetPRLVersion(WORD * pPRLVersion) {
  ReentrancyPreventer rp(this, "GetPRLVersion");
  return ::GetPRLVersion(pPRLVersion);
}

ULONG Sdk::GetERIFile(
    ULONG *                    pFileSize,
    BYTE *                     pFile) {
  ReentrancyPreventer rp(this, "GetERIFile");
  return ::GetERIFile(
      pFileSize,
      pFile);
}

// NB: CHROMIUM: modified this to const after investigating QC code
ULONG Sdk::ActivateAutomatic(const CHAR * pActivationCode) {
  ReentrancyPreventer rp(this, "ActivateAutomatic");
  return ::ActivateAutomatic(const_cast<CHAR *>(pActivationCode));
}

ULONG Sdk::ActivateManual(
    CHAR *                     pSPC,
    WORD                       sid,
    CHAR *                     pMDN,
    CHAR *                     pMIN,
    ULONG                      prlSize,
    BYTE *                     pPRL,
    CHAR *                     pMNHA,
    CHAR *                     pMNAAA) {
  ReentrancyPreventer rp(this, "ActivateManual");
  return ::ActivateManual(
      pSPC,
      sid,
      pMDN,
      pMIN,
      prlSize,
      pPRL,
      pMNHA,
      pMNAAA);
}

ULONG Sdk::ResetToFactoryDefaults(CHAR * pSPC) {
  ReentrancyPreventer rp(this, "ResetToFactoryDefaults");
  return ::ResetToFactoryDefaults(pSPC);
}

ULONG Sdk::GetActivationState(ULONG * pActivationState) {
  ReentrancyPreventer rp(this, "GetActivationState");
  return ::GetActivationState(pActivationState);
}

ULONG Sdk::SetPower(ULONG powerMode) {
  ReentrancyPreventer rp(this, "SetPower");
  return ::SetPower(powerMode);
}

ULONG Sdk::GetPower(ULONG * pPowerMode) {
  ReentrancyPreventer rp(this, "GetPower");
  return ::GetPower(pPowerMode);
}

ULONG Sdk::GetOfflineReason(
    ULONG *                    pReasonMask,
    ULONG *                    pbPlatform) {
  ReentrancyPreventer rp(this, "GetOfflineReason");
  return ::GetOfflineReason(
      pReasonMask,
      pbPlatform);
}

ULONG Sdk::GetNetworkTime(
    ULONGLONG *                pTimeCount,
    ULONG *                    pTimeSource) {
  ReentrancyPreventer rp(this, "GetNetworkTime");
  return ::GetNetworkTime(
      pTimeCount,
      pTimeSource);
}

ULONG Sdk::ValidateSPC(CHAR * pSPC) {
  ReentrancyPreventer rp(this, "ValidateSPC");
  return ::ValidateSPC(pSPC);
}

ULONG Sdk::DeleteSMS(
    ULONG                      storageType,
    ULONG *                    pMessageIndex,
    ULONG *                    pMessageTag) {
  ReentrancyPreventer rp(this, "DeleteSMS");
  return ::DeleteSMS(
      storageType,
      pMessageIndex,
      pMessageTag);
}

ULONG Sdk::GetSMSList(
    ULONG                      storageType,
    ULONG *                    pRequestedTag,
    ULONG *                    pMessageListSize,
    BYTE *                     pMessageList) {
  ReentrancyPreventer rp(this, "GetSMSList");
  return ::GetSMSList(
      storageType,
      pRequestedTag,
      pMessageListSize,
      pMessageList);
}

ULONG Sdk::GetSMS(
    ULONG                      storageType,
    ULONG                      messageIndex,
    ULONG *                    pMessageTag,
    ULONG *                    pMessageFormat,
    ULONG *                    pMessageSize,
    BYTE *                     pMessage) {
  ReentrancyPreventer rp(this, "GetSMS");
  return ::GetSMS(
      storageType,
      messageIndex,
      pMessageTag,
      pMessageFormat,
      pMessageSize,
      pMessage);
}

ULONG Sdk::ModifySMSStatus(
    ULONG                      storageType,
    ULONG                      messageIndex,
    ULONG                      messageTag) {
  ReentrancyPreventer rp(this, "ModifySMSStatus");
  return ::ModifySMSStatus(
      storageType,
      messageIndex,
      messageTag);
}

ULONG Sdk::SaveSMS(
    ULONG                      storageType,
    ULONG                      messageFormat,
    ULONG                      messageSize,
    BYTE *                     pMessage,
    ULONG *                    pMessageIndex) {
  ReentrancyPreventer rp(this, "SaveSMS");
  return ::SaveSMS(
      storageType,
      messageFormat,
      messageSize,
      pMessage,
      pMessageIndex);
}

ULONG Sdk::SendSMS(
    ULONG                      messageFormat,
    ULONG                      messageSize,
    BYTE *                     pMessage,
    ULONG *                    pMessageFailureCode) {
  ReentrancyPreventer rp(this, "SendSMS");
  return ::SendSMS(
      messageFormat,
      messageSize,
      pMessage,
      pMessageFailureCode);
}

ULONG Sdk::GetSMSCAddress(
    BYTE                       addressSize,
    CHAR *                     pSMSCAddress,
    BYTE                       typeSize,
    CHAR *                     pSMSCType) {
  ReentrancyPreventer rp(this, "GetSMSCAddress");
  return ::GetSMSCAddress(
      addressSize,
      pSMSCAddress,
      typeSize,
      pSMSCType);
}

ULONG Sdk::SetSMSCAddress(
    CHAR *                     pSMSCAddress,
    CHAR *                     pSMSCType) {
  ReentrancyPreventer rp(this, "SetSMSCAddress");
  return ::SetSMSCAddress(
      pSMSCAddress,
      pSMSCType);
}

ULONG Sdk::UIMSetPINProtection(
    ULONG                      id,
    ULONG                      bEnable,
    CHAR *                     pValue,
    ULONG *                    pVerifyRetriesLeft,
    ULONG *                    pUnblockRetriesLeft) {
  ReentrancyPreventer rp(this, "UIMSetPINProtection");
  return ::UIMSetPINProtection(
      id,
      bEnable,
      pValue,
      pVerifyRetriesLeft,
      pUnblockRetriesLeft);
}

ULONG Sdk::UIMVerifyPIN(
    ULONG                      id,
    CHAR *                     pValue,
    ULONG *                    pVerifyRetriesLeft,
    ULONG *                    pUnblockRetriesLeft) {
  ReentrancyPreventer rp(this, "UIMVerifyPIN");
  return ::UIMVerifyPIN(
      id,
      pValue,
      pVerifyRetriesLeft,
      pUnblockRetriesLeft);
}

ULONG Sdk::UIMUnblockPIN(
    ULONG                      id,
    CHAR *                     pPUKValue,
    CHAR *                     pNewValue,
    ULONG *                    pVerifyRetriesLeft,
    ULONG *                    pUnblockRetriesLeft) {
  ReentrancyPreventer rp(this, "UIMUnblockPIN");
  return ::UIMUnblockPIN(
      id,
      pPUKValue,
      pNewValue,
      pVerifyRetriesLeft,
      pUnblockRetriesLeft);
}

ULONG Sdk::UIMChangePIN(
    ULONG                      id,
    CHAR *                     pOldValue,
    CHAR *                     pNewValue,
    ULONG *                    pVerifyRetriesLeft,
    ULONG *                    pUnblockRetriesLeft) {
  ReentrancyPreventer rp(this, "UIMChangePIN");
  return ::UIMChangePIN(
      id,
      pOldValue,
      pNewValue,
      pVerifyRetriesLeft,
      pUnblockRetriesLeft);
}

ULONG Sdk::UIMGetPINStatus(
    ULONG                      id,
    ULONG *                    pStatus,
    ULONG *                    pVerifyRetriesLeft,
    ULONG *                    pUnblockRetriesLeft) {
  ReentrancyPreventer rp(this, "UIMGetPINStatus");
  return ::UIMGetPINStatus(
      id,
      pStatus,
      pVerifyRetriesLeft,
      pUnblockRetriesLeft);
}

ULONG Sdk::UIMGetICCID(
    BYTE                       stringSize,
    CHAR *                     pString) {
  ReentrancyPreventer rp(this, "UIMGetICCID");
  return ::UIMGetICCID(
      stringSize,
      pString);
}
ULONG Sdk::UIMGetControlKeyStatus(
    ULONG                      id,
    ULONG *                    pStatus,
    ULONG *                    pVerifyRetriesLeft,
    ULONG *                    pUnblockRetriesLeft) {
  ReentrancyPreventer rp(this, "UIMGetControlKeyStatus");
  return ::UIMGetControlKeyStatus(
      id,
      pStatus,
      pVerifyRetriesLeft,
      pUnblockRetriesLeft);
}

ULONG Sdk::UIMSetControlKeyProtection(
    ULONG                      id,
    ULONG                      status,
    CHAR *                     pValue,
    ULONG *                    pVerifyRetriesLeft) {
  ReentrancyPreventer rp(this, "UIMSetControlKeyProtection");
  return ::UIMSetControlKeyProtection(
      id,
      status,
      pValue,
      pVerifyRetriesLeft);
}

ULONG Sdk::UIMUnblockControlKey(
    ULONG                      id,
    CHAR *                     pValue,
    ULONG *                    pUnblockRetriesLeft) {
  ReentrancyPreventer rp(this, "UIMUnblockControlKey");
  return ::UIMUnblockControlKey(
      id,
      pValue,
      pUnblockRetriesLeft);
}

ULONG Sdk::GetPDSState(
    ULONG *                    pEnabled,
    ULONG *                    pTracking) {
  ReentrancyPreventer rp(this, "GetPDSState");
  return ::GetPDSState(
      pEnabled,
      pTracking);
}

ULONG Sdk::SetPDSState(ULONG enable) {
  ReentrancyPreventer rp(this, "SetPDSState");
  return ::SetPDSState(enable);
}

ULONG Sdk::PDSInjectTimeReference(
    ULONGLONG                  systemTime,
    USHORT                     systemDiscontinuities) {
  ReentrancyPreventer rp(this, "PDSInjectTimeReference");
  return ::PDSInjectTimeReference(
      systemTime,
      systemDiscontinuities);
}

ULONG Sdk::GetPDSDefaults(
    ULONG *                    pOperation,
    BYTE *                     pTimeout,
    ULONG *                    pInterval,
    ULONG *                    pAccuracy) {
  ReentrancyPreventer rp(this, "GetPDSDefaults");
  return ::GetPDSDefaults(
      pOperation,
      pTimeout,
      pInterval,
      pAccuracy);
}

ULONG Sdk::SetPDSDefaults(
    ULONG                      operation,
    BYTE                       timeout,
    ULONG                      interval,
    ULONG                      accuracy) {
  ReentrancyPreventer rp(this, "SetPDSDefaults");
  return ::SetPDSDefaults(
      operation,
      timeout,
      interval,
      accuracy);
}

ULONG Sdk::GetXTRAAutomaticDownload(
    ULONG *                    pbEnabled,
    USHORT *                   pInterval) {
  ReentrancyPreventer rp(this, "GetXTRAAutomaticDownload");
  return ::GetXTRAAutomaticDownload(
      pbEnabled,
      pInterval);
}

ULONG Sdk::SetXTRAAutomaticDownload(
    ULONG                      bEnabled,
    USHORT                     interval) {
  ReentrancyPreventer rp(this, "SetXTRAAutomaticDownload");
  return ::SetXTRAAutomaticDownload(
      bEnabled,
      interval);
}

ULONG Sdk::GetXTRANetwork(ULONG * pPreference) {
  ReentrancyPreventer rp(this, "GetXTRANetwork");
  return ::GetXTRANetwork(pPreference);
}

ULONG Sdk::SetXTRANetwork(ULONG preference) {
  ReentrancyPreventer rp(this, "SetXTRANetwork");
  return ::SetXTRANetwork(preference);
}

ULONG Sdk::GetXTRAValidity(
    USHORT *                   pGPSWeek,
    USHORT *                   pGPSWeekOffset,
    USHORT *                   pDuration) {
  ReentrancyPreventer rp(this, "GetXTRAValidity");
  return ::GetXTRAValidity(
      pGPSWeek,
      pGPSWeekOffset,
      pDuration);
}

ULONG Sdk::ForceXTRADownload() {
  ReentrancyPreventer rp(this, "ForceXTRADownload");
  return ::ForceXTRADownload();
}

ULONG Sdk::GetAGPSConfig(
    ULONG *                    pServerAddress,
    ULONG *                    pServerPort) {
  ReentrancyPreventer rp(this, "GetAGPSConfig");
  return ::GetAGPSConfig(
      pServerAddress,
      pServerPort);
}

ULONG Sdk::SetAGPSConfig(
    ULONG                      serverAddress,
    ULONG                      serverPort) {
  ReentrancyPreventer rp(this, "SetAGPSConfig");
  return ::SetAGPSConfig(
      serverAddress,
      serverPort);
}

ULONG Sdk::GetServiceAutomaticTracking(ULONG * pbAuto) {
  ReentrancyPreventer rp(this, "GetServiceAutomaticTracking");
  return ::GetServiceAutomaticTracking(pbAuto);
}

ULONG Sdk::SetServiceAutomaticTracking(ULONG bAuto) {
  ReentrancyPreventer rp(this, "SetServiceAutomaticTracking");
  return ::SetServiceAutomaticTracking(bAuto);
}

ULONG Sdk::GetPortAutomaticTracking(ULONG * pbAuto) {
  ReentrancyPreventer rp(this, "GetPortAutomaticTracking");
  return ::GetPortAutomaticTracking(pbAuto);
}

ULONG Sdk::SetPortAutomaticTracking(ULONG bAuto) {
  ReentrancyPreventer rp(this, "SetPortAutomaticTracking");
  return ::SetPortAutomaticTracking(bAuto);
}

ULONG Sdk::ResetPDSData(
    ULONG *                    pGPSDataMask,
    ULONG *                    pCellDataMask) {
  ReentrancyPreventer rp(this, "ResetPDSData");
  return ::ResetPDSData(
      pGPSDataMask,
      pCellDataMask);
}

ULONG Sdk::CATSendTerminalResponse(
    ULONG                      refID,
    ULONG                      dataLen,
    BYTE *                     pData) {
  ReentrancyPreventer rp(this, "CATSendTerminalResponse");
  return ::CATSendTerminalResponse(
      refID,
      dataLen,
      pData);
}

ULONG Sdk::CATSendEnvelopeCommand(
    ULONG                      cmdID,
    ULONG                      dataLen,
    BYTE *                     pData) {
  ReentrancyPreventer rp(this, "CATSendEnvelopeCommand");
  return ::CATSendEnvelopeCommand(
      cmdID,
      dataLen,
      pData);
}

ULONG Sdk::GetSMSWake(
    ULONG *                    pbEnabled,
    ULONG *                    pWakeMask) {
  ReentrancyPreventer rp(this, "GetSMSWake");
  return ::GetSMSWake(
      pbEnabled,
      pWakeMask);
}

ULONG Sdk::SetSMSWake(
    ULONG                      bEnable,
    ULONG                      wakeMask) {
  ReentrancyPreventer rp(this, "SetSMSWake");
  return ::SetSMSWake(
      bEnable,
      wakeMask);
}

ULONG Sdk::OMADMStartSession(ULONG sessionType) {
  ReentrancyPreventer rp(this, "OMADMStartSession");
  return ::OMADMStartSession(sessionType);
}

ULONG Sdk::OMADMCancelSession() {
  ReentrancyPreventer rp(this, "OMADMCancelSession");
  return ::OMADMCancelSession();
}

ULONG Sdk::OMADMGetSessionInfo(
    ULONG *                    pSessionState,
    ULONG *                    pSessionType,
    ULONG *                    pFailureReason,
    BYTE *                     pRetryCount,
    WORD *                     pSessionPause,
    WORD *                     pTimeRemaining) {
  ReentrancyPreventer rp(this, "OMADMGetSessionInfo");
  return ::OMADMGetSessionInfo(
      pSessionState,
      pSessionType,
      pFailureReason,
      pRetryCount,
      pSessionPause,
      pTimeRemaining);
}

ULONG Sdk::OMADMGetPendingNIA(
    ULONG *                    pSessionType,
    USHORT *                   pSessionID) {
  ReentrancyPreventer rp(this, "OMADMGetPendingNIA");
  return ::OMADMGetPendingNIA(
      pSessionType,
      pSessionID);
}

ULONG Sdk::OMADMSendSelection(
    ULONG                      selection,
    USHORT                     sessionID) {
  ReentrancyPreventer rp(this, "OMADMSendSelection");
  return ::OMADMSendSelection(
      selection,
      sessionID);
}

ULONG Sdk::OMADMGetFeatureSettings(
    ULONG *                    pbProvisioning,
    ULONG *                    pbPRLUpdate) {
  ReentrancyPreventer rp(this, "OMADMGetFeatureSettings");
  return ::OMADMGetFeatureSettings(
      pbProvisioning,
      pbPRLUpdate);
}

ULONG Sdk::OMADMSetProvisioningFeature(
    ULONG                      bProvisioning) {
  ReentrancyPreventer rp(this, "OMADMSetProvisioningFeature");
  return ::OMADMSetProvisioningFeature(
      bProvisioning);
}

ULONG Sdk::OMADMSetPRLUpdateFeature(
    ULONG                      bPRLUpdate) {
  ReentrancyPreventer rp(this, "OMADMSetPRLUpdateFeature");
  return ::OMADMSetPRLUpdateFeature(
      bPRLUpdate);
}

ULONG Sdk::UpgradeFirmware(CHAR * pDestinationPath) {
  ReentrancyPreventer rp(this, "UpgradeFirmware");
  return ::UpgradeFirmware(pDestinationPath);
}

ULONG Sdk::GetImageInfo(
    CHAR *                     pPath,
    ULONG *                    pFirmwareID,
    ULONG *                    pTechnology,
    ULONG *                    pCarrier,
    ULONG *                    pRegion,
    ULONG *                    pGPSCapability) {
  ReentrancyPreventer rp(this, "GetImageInfo");
  return ::GetImageInfo(
      pPath,
      pFirmwareID,
      pTechnology,
      pCarrier,
      pRegion,
      pGPSCapability);
}

ULONG Sdk::GetImageStore(
    WORD                       pathSize,
    CHAR *                     pImageStorePath) {
  ReentrancyPreventer rp(this, "GetImageStore");
  return ::GetImageStore(
      pathSize,
      pImageStorePath);
}

ULONG Sdk::SetSessionStateCallback(tFNSessionState pCallback) {
  ReentrancyPreventer rp(this, "SetSessionStateCallback");
  return ::SetSessionStateCallback(pCallback);
}

ULONG Sdk::SetByteTotalsCallback(
    tFNByteTotals              pCallback,
    BYTE                       interval) {
  ReentrancyPreventer rp(this, "SetByteTotalsCallback");
  return ::SetByteTotalsCallback(
      pCallback,
      interval);
}

ULONG Sdk::SetDataCapabilitiesCallback(
    tFNDataCapabilities        pCallback) {
  ReentrancyPreventer rp(this, "SetDataCapabilitiesCallback");
  return ::SetDataCapabilitiesCallback(
      pCallback);
}

ULONG Sdk::SetDataBearerCallback(tFNDataBearer pCallback) {
  ReentrancyPreventer rp(this, "SetDataBearerCallback");
  return ::SetDataBearerCallback(pCallback);
}

ULONG Sdk::SetDormancyStatusCallback(
    tFNDormancyStatus          pCallback) {
  ReentrancyPreventer rp(this, "SetDormancyStatusCallback");
  return ::SetDormancyStatusCallback(
      pCallback);
}

ULONG Sdk::SetMobileIPStatusCallback(
    tFNMobileIPStatus          pCallback) {
  ReentrancyPreventer rp(this, "SetMobileIPStatusCallback");
  return ::SetMobileIPStatusCallback(
      pCallback);
}

ULONG Sdk::SetActivationStatusCallback(
    tFNActivationStatus        pCallback) {
  ReentrancyPreventer rp(this, "SetActivationStatusCallback");
  return ::SetActivationStatusCallback(
      pCallback);
}

ULONG Sdk::SetPowerCallback(tFNPower pCallback) {
  ReentrancyPreventer rp(this, "SetPowerCallback");
  return ::SetPowerCallback(pCallback);
}
ULONG Sdk::SetRoamingIndicatorCallback(
    tFNRoamingIndicator        pCallback) {
  ReentrancyPreventer rp(this, "SetRoamingIndicatorCallback");
  return ::SetRoamingIndicatorCallback(
      pCallback);
}

ULONG Sdk::SetSignalStrengthCallback(
    tFNSignalStrength          pCallback,
    BYTE                       thresholdsSize,
    INT8 *                     pThresholds) {
  ReentrancyPreventer rp(this, "SetSignalStrengthCallback");
  return ::SetSignalStrengthCallback(
      pCallback,
      thresholdsSize,
      pThresholds);
}

ULONG Sdk::SetRFInfoCallback(tFNRFInfo pCallback) {
  ReentrancyPreventer rp(this, "SetRFInfoCallback");
  return ::SetRFInfoCallback(pCallback);
}

ULONG Sdk::SetLURejectCallback(tFNLUReject pCallback) {
  ReentrancyPreventer rp(this, "SetLURejectCallback");
  return ::SetLURejectCallback(pCallback);
}

ULONG Sdk::SetNewSMSCallback(tFNNewSMS pCallback) {
  ReentrancyPreventer rp(this, "SetNewSMSCallback");
  return ::SetNewSMSCallback(pCallback);
}

ULONG Sdk::SetNMEACallback(tFNNewNMEA pCallback) {
  ReentrancyPreventer rp(this, "SetNMEACallback");
  return ::SetNMEACallback(pCallback);
}

ULONG Sdk::SetNMEAPlusCallback(tFNNewNMEAPlus pCallback) {
  ReentrancyPreventer rp(this, "SetNMEAPlusCallback");
  return ::SetNMEAPlusCallback(pCallback);
}

ULONG Sdk::SetPDSStateCallback(tFNPDSState pCallback) {
  ReentrancyPreventer rp(this, "SetPDSStateCallback");
  return ::SetPDSStateCallback(pCallback);
}

ULONG Sdk::SetCATEventCallback(
    tFNCATEvent                pCallback,
    ULONG                      eventMask,
    ULONG *                    pErrorMask) {
  ReentrancyPreventer rp(this, "SetCATEventCallback");
  return ::SetCATEventCallback(
      pCallback,
      eventMask,
      pErrorMask);
}

ULONG Sdk::SetOMADMAlertCallback(tFNOMADMAlert pCallback) {
  ReentrancyPreventer rp(this, "SetOMADMAlertCallback");
  return ::SetOMADMAlertCallback(pCallback);
}

ULONG Sdk::SetOMADMStateCallback(tFNOMADMState pCallback) {
  ReentrancyPreventer rp(this, "SetOMADMStateCallback");
  return ::SetOMADMStateCallback(pCallback);
}

}   // Namespace Gobi
