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

  "+RemoteManagement",
  "GetSMSWake",
  "SetSMSWake",

  "+OMADM",
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

struct CallWrapper {
  CallWrapper(Sdk *sdk, const char *name)
      : sdk_(sdk),
        function_name_(name) {
    sdk_->EnterSdk(function_name_);
  }

  ULONG CheckReturn(ULONG rc) {
    if (rc == kErrorSendingQmiRequest ||
        rc == kErrorReceivingQmiRequest ||
        rc == kErrorNeedsReset) {
      // SetOMADM...Callback returns error kErrorSendingQmiRequest when
      // run on an image without OMADM-capable firmware.  This error code
      // normally means "You have lost synch with the modem and must reset
      // it", but in this case we don't want to reset it.
      //    http://code.google.com/p/chromium-os/issues/detail?id=9372
      // tracks removing this workaround
      if (strstr(function_name_, "OMADM") == NULL)  {
        sdk_->sdk_error_sink_(sdk_->current_modem_path_,
                              function_name_,
                              rc);
      } else {
        LOG(ERROR) << "Did not notify on function " << function_name_;
      }
    }
    return rc;
  }

  ~CallWrapper() {
    sdk_->LeaveSdk(function_name_);
  }
  Sdk *sdk_;
  const char *function_name_;
 private:
  DISALLOW_COPY_AND_ASSIGN(CallWrapper);
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
  CallWrapper cw(this, "QCWWANEnumerateDevices");
  return cw.CheckReturn(::QCWWANEnumerateDevices(pDevicesSize,
                                                 pDevices));
}

ULONG Sdk::QCWWANConnect(
    CHAR *                     pDeviceNode,
    CHAR *                     pDeviceKey) {
  CallWrapper cw(this, "QCWWANConnect");
  return cw.CheckReturn(::QCWWANConnect(pDeviceNode,
                                        pDeviceKey));
}

ULONG Sdk::QCWWANDisconnect() {
  CallWrapper cw(this, "QCWWANDisconnect");
  return cw.CheckReturn(::QCWWANDisconnect());
}

ULONG Sdk::QCWWANGetConnectedDeviceID(
    ULONG                      deviceNodeSize,
    CHAR *                     pDeviceNode,
    ULONG                      deviceKeySize,
    CHAR *                     pDeviceKey) {
  CallWrapper cw(this, "QCWWANGetConnectedDeviceID");
  return cw.CheckReturn(::QCWWANGetConnectedDeviceID(
      deviceNodeSize,
      pDeviceNode,
      deviceKeySize,
      pDeviceKey));
}

ULONG Sdk::GetSessionState(ULONG * pState) {
  CallWrapper cw(this, "GetSessionState");
  return cw.CheckReturn(::GetSessionState(pState));
}

ULONG Sdk::GetSessionDuration(ULONGLONG * pDuration) {
  CallWrapper cw(this, "GetSessionDuration");
  return cw.CheckReturn(::GetSessionDuration(pDuration));
}

ULONG Sdk::GetDormancyState(ULONG * pState) {
  CallWrapper cw(this, "GetDormancyState");
  return cw.CheckReturn(::GetDormancyState(pState));
}

ULONG Sdk::GetAutoconnect(ULONG * pSetting) {
  CallWrapper cw(this, "GetAutoconnect");
  return cw.CheckReturn(::GetAutoconnect(pSetting));
}

ULONG Sdk::SetAutoconnect(ULONG setting) {
  CallWrapper cw(this, "SetAutoconnect");
  return cw.CheckReturn(::SetAutoconnect(setting));
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
  CallWrapper cw(this, "SetDefaultProfile");
  return cw.CheckReturn(::SetDefaultProfile(
      profileType,
      pPDPType,
      pIPAddress,
      pPrimaryDNS,
      pSecondaryDNS,
      pAuthentication,
      pName,
      pAPNName,
      pUsername,
      pPassword));
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
  CallWrapper cw(this, "GetDefaultProfile");
  return cw.CheckReturn(::GetDefaultProfile(
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
      pUsername));
}

ULONG Sdk::StartDataSession(
    ULONG *                    pTechnology,
    const CHAR *               pAPNName,
    ULONG *                    pAuthentication,
    const CHAR *               pUsername,
    const CHAR *               pPassword,
    ULONG *                    pSessionId,
    ULONG *                    pFailureReason
                            ) {
  TemporaryCopier mutableAPNName(pAPNName);
  TemporaryCopier mutableUsername(pUsername);
  TemporaryCopier mutablePassword(pPassword);
  CallWrapper cw(this, "StartDataSession");
  return cw.CheckReturn(::StartDataSession(
      pTechnology,
      NULL,
      NULL,
      NULL,
      NULL,
      mutableAPNName.get(),
      NULL,
      pAuthentication,
      mutableUsername.get(),
      mutablePassword.get(),
      pSessionId,
      pFailureReason));
}

ULONG Sdk::StopDataSession(ULONG sessionId) {
  CallWrapper cw(this, "StopDataSession");
  return cw.CheckReturn(::StopDataSession(sessionId));
}

ULONG Sdk::GetIPAddress(ULONG * pIPAddress) {
  CallWrapper cw(this, "GetIPAddress");
  return cw.CheckReturn(::GetIPAddress(pIPAddress));
}

ULONG Sdk::GetConnectionRate(
    ULONG *                    pCurrentChannelTXRate,
    ULONG *                    pCurrentChannelRXRate,
    ULONG *                    pMaxChannelTXRate,
    ULONG *                    pMaxChannelRXRate) {
  CallWrapper cw(this, "GetConnectionRate");
  return cw.CheckReturn(::GetConnectionRate(
      pCurrentChannelTXRate,
      pCurrentChannelRXRate,
      pMaxChannelTXRate,
      pMaxChannelRXRate));
}

ULONG Sdk::GetPacketStatus(
    ULONG *                    pTXPacketSuccesses,
    ULONG *                    pRXPacketSuccesses,
    ULONG *                    pTXPacketErrors,
    ULONG *                    pRXPacketErrors,
    ULONG *                    pTXPacketOverflows,
    ULONG *                    pRXPacketOverflows) {
  CallWrapper cw(this, "GetPacketStatus");
  return cw.CheckReturn(::GetPacketStatus(
      pTXPacketSuccesses,
      pRXPacketSuccesses,
      pTXPacketErrors,
      pRXPacketErrors,
      pTXPacketOverflows,
      pRXPacketOverflows));
}

ULONG Sdk::GetByteTotals(
    ULONGLONG *                pTXTotalBytes,
    ULONGLONG *                pRXTotalBytes) {
  CallWrapper cw(this, "GetByteTotals");
  return cw.CheckReturn(::GetByteTotals(
      pTXTotalBytes,
      pRXTotalBytes));
}

ULONG Sdk::SetMobileIP(ULONG mode) {
  CallWrapper cw(this, "SetMobileIP");
  return cw.CheckReturn(::SetMobileIP(mode));
}

ULONG Sdk::GetMobileIP(ULONG * pMode) {
  CallWrapper cw(this, "GetMobileIP");
  return cw.CheckReturn(::GetMobileIP(pMode));
}

ULONG Sdk::SetActiveMobileIPProfile(
    CHAR *                     pSPC,
    BYTE                       index) {
  CallWrapper cw(this, "SetActiveMobileIPProfile");
  return cw.CheckReturn(::SetActiveMobileIPProfile(
      pSPC,
      index));
}

ULONG Sdk::GetActiveMobileIPProfile(BYTE * pIndex) {
  CallWrapper cw(this, "GetActiveMobileIPProfile");
  return cw.CheckReturn(::GetActiveMobileIPProfile(pIndex));
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
  CallWrapper cw(this, "SetMobileIPProfile");
  return cw.CheckReturn(::SetMobileIPProfile(
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
      pMNAAA));
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
  CallWrapper cw(this, "GetMobileIPProfile");
  return cw.CheckReturn(::GetMobileIPProfile(
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
      pAAAState));
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
  CallWrapper cw(this, "SetMobileIPParameters");
  return cw.CheckReturn(::SetMobileIPParameters(
      pSPC,
      pMode,
      pRetryLimit,
      pRetryInterval,
      pReRegPeriod,
      pReRegTraffic,
      pHAAuthenticator,
      pHA2002bis));
}

ULONG Sdk::GetMobileIPParameters(
    ULONG *                    pMode,
    BYTE *                     pRetryLimit,
    BYTE *                     pRetryInterval,
    BYTE *                     pReRegPeriod,
    BYTE *                     pReRegTraffic,
    BYTE *                     pHAAuthenticator,
    BYTE *                     pHA2002bis) {
  CallWrapper cw(this, "GetMobileIPParameters");
  return cw.CheckReturn(::GetMobileIPParameters(
      pMode,
      pRetryLimit,
      pRetryInterval,
      pReRegPeriod,
      pReRegTraffic,
      pHAAuthenticator,
      pHA2002bis));
}

ULONG Sdk::GetLastMobileIPError(ULONG * pError) {
  CallWrapper cw(this, "GetLastMobileIPError");
  return cw.CheckReturn(::GetLastMobileIPError(pError));
}

ULONG Sdk::GetANAAAAuthenticationStatus(ULONG * pStatus) {
  CallWrapper cw(this, "GetANAAAAuthenticationStatus");
  return cw.CheckReturn(::GetANAAAAuthenticationStatus(pStatus));
}

ULONG Sdk::GetSignalStrengths(
    ULONG *                    pArraySizes,
    INT8 *                     pSignalStrengths,
    ULONG *                    pRadioInterfaces) {
  CallWrapper cw(this, "GetSignalStrengths");
  return cw.CheckReturn(::GetSignalStrengths(
      pArraySizes,
      pSignalStrengths,
      pRadioInterfaces));
}

ULONG Sdk::GetRFInfo(
    BYTE *                     pInstanceSize,
    BYTE *                     pInstances) {
  CallWrapper cw(this, "GetRFInfo");
  return cw.CheckReturn(::GetRFInfo(
      pInstanceSize,
      pInstances));
}

ULONG Sdk::PerformNetworkScan(
    BYTE *                     pInstanceSize,
    BYTE *                     pInstances) {
  CallWrapper cw(this, "PerformNetworkScan");
  return cw.CheckReturn(::PerformNetworkScan(
      pInstanceSize,
      pInstances));
}

ULONG Sdk::InitiateNetworkRegistration(
    ULONG                      regType,
    WORD                       mcc,
    WORD                       mnc,
    ULONG                      rat) {
  CallWrapper cw(this, "InitiateNetworkRegistration");
  return cw.CheckReturn(::InitiateNetworkRegistration(
      regType,
      mcc,
      mnc,
      rat));
}

ULONG Sdk::InitiateDomainAttach(ULONG action) {
  CallWrapper cw(this, "InitiateDomainAttach");
  return cw.CheckReturn(::InitiateDomainAttach(action));
}

ULONG Sdk::GetServingNetwork(
    ULONG *                    pRegistrationState,
    ULONG *                    pRAN,
    BYTE *                     pRadioIfacesSize,
    BYTE *                     pRadioIfaces,
    ULONG *                    pRoaming,
    WORD *                     pMCC,
    WORD *                     pMNC,
    BYTE                       nameSize,
    CHAR *                     pName) {
  ULONG l1, l2;
  CallWrapper cw(this, "GetServingNetwork");
  return cw.CheckReturn(::GetServingNetwork(
      pRegistrationState,
      &l1,              // CS domain
      &l2,              // PS domain
      pRAN,
      pRadioIfacesSize,
      pRadioIfaces,
      pRoaming,
      pMCC,
      pMNC,
      nameSize,
      pName));
}

ULONG Sdk::GetServingNetworkCapabilities(
    BYTE *                     pDataCapsSize,
    BYTE *                     pDataCaps) {
  CallWrapper cw(this, "GetServingNetworkCapabilities");
  return cw.CheckReturn(::GetServingNetworkCapabilities(
      pDataCapsSize,
      pDataCaps));
}

ULONG Sdk::GetDataBearerTechnology(ULONG * pDataBearer) {
  CallWrapper cw(this, "GetDataBearerTechnology");
  return cw.CheckReturn(::GetDataBearerTechnology(pDataBearer));
}

ULONG Sdk::GetHomeNetwork(
    WORD *                     pMCC,
    WORD *                     pMNC,
    BYTE                       nameSize,
    CHAR *                     pName,
    WORD *                     pSID,
    WORD *                     pNID) {
  CallWrapper cw(this, "GetHomeNetwork");
  return cw.CheckReturn(::GetHomeNetwork(
      pMCC,
      pMNC,
      nameSize,
      pName,
      pSID,
      pNID));
}

ULONG Sdk::SetNetworkPreference(
    ULONG                      technologyPref,
    ULONG                      duration) {
  CallWrapper cw(this, "SetNetworkPreference");
  return cw.CheckReturn(::SetNetworkPreference(
      technologyPref,
      duration));
}

ULONG Sdk::GetNetworkPreference(
    ULONG *                    pTechnologyPref,
    ULONG *                    pDuration,
    ULONG *                    pPersistentTechnologyPref) {
  CallWrapper cw(this, "GetNetworkPreference");
  return cw.CheckReturn(::GetNetworkPreference(
      pTechnologyPref,
      pDuration,
      pPersistentTechnologyPref));
}

ULONG Sdk::SetCDMANetworkParameters(
    CHAR *                     pSPC,
    BYTE *                     pForceRev0,
    BYTE *                     pCustomSCP,
    ULONG *                    pProtocol,
    ULONG *                    pBroadcast,
    ULONG *                    pApplication,
    ULONG *                    pRoaming) {
  CallWrapper cw(this, "SetCDMANetworkParameters");
  return cw.CheckReturn(::SetCDMANetworkParameters(
      pSPC,
      pForceRev0,
      pCustomSCP,
      pProtocol,
      pBroadcast,
      pApplication,
      pRoaming));
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
  CallWrapper cw(this, "GetCDMANetworkParameters");
  return cw.CheckReturn(::GetCDMANetworkParameters(
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
      pRoaming));
}

ULONG Sdk::GetACCOLC(BYTE * pACCOLC) {
  CallWrapper cw(this, "GetACCOLC");
  return cw.CheckReturn(::GetACCOLC(pACCOLC));
}

ULONG Sdk::SetACCOLC(
    CHAR *                     pSPC,
    BYTE                       accolc) {
  CallWrapper cw(this, "SetACCOLC");
  return cw.CheckReturn(::SetACCOLC(
      pSPC,
      accolc));
}

ULONG Sdk::GetDeviceCapabilities(
    ULONG *                    pMaxTXChannelRate,
    ULONG *                    pMaxRXChannelRate,
    ULONG *                    pDataServiceCapability,
    ULONG *                    pSimCapability,
    ULONG *                    pRadioIfacesSize,
    BYTE *                     pRadioIfaces) {
  CallWrapper cw(this, "GetDeviceCapabilities");
  return cw.CheckReturn(::GetDeviceCapabilities(
      pMaxTXChannelRate,
      pMaxRXChannelRate,
      pDataServiceCapability,
      pSimCapability,
      pRadioIfacesSize,
      pRadioIfaces));
}

ULONG Sdk::GetManufacturer(
    BYTE                       stringSize,
    CHAR *                     pString) {
  CallWrapper cw(this, "GetManufacturer");
  return cw.CheckReturn(::GetManufacturer(
      stringSize,
      pString));
}

ULONG Sdk::GetModelID(
    BYTE                       stringSize,
    CHAR *                     pString) {
  CallWrapper cw(this, "GetModelID");
  return cw.CheckReturn(::GetModelID(
      stringSize,
      pString));
}

ULONG Sdk::GetFirmwareRevision(
    BYTE                       stringSize,
    CHAR *                     pString) {
  CallWrapper cw(this, "GetFirmwareRevision");
  return cw.CheckReturn(::GetFirmwareRevision(
      stringSize,
      pString));
}

ULONG Sdk::GetFirmwareRevisions(
    BYTE                       amssSize,
    CHAR *                     pAMSSString,
    BYTE                       bootSize,
    CHAR *                     pBootString,
    BYTE                       priSize,
    CHAR *                     pPRIString) {
  CallWrapper cw(this, "GetFirmwareRevisions");
  return cw.CheckReturn(::GetFirmwareRevisions(
      amssSize,
      pAMSSString,
      bootSize,
      pBootString,
      priSize,
      pPRIString));
}

ULONG Sdk::GetFirmwareInfo(
    ULONG *                    pFirmwareID,
    ULONG *                    pTechnology,
    ULONG *                    pCarrier,
    ULONG *                    pRegion,
    ULONG *                    pGPSCapability) {
  CallWrapper cw(this, "GetFirmwareInfo");
  return cw.CheckReturn(::GetFirmwareInfo(
      pFirmwareID,
      pTechnology,
      pCarrier,
      pRegion,
      pGPSCapability));
}

ULONG Sdk::GetVoiceNumber(
    BYTE                       voiceNumberSize,
    CHAR *                     pVoiceNumber,
    BYTE                       minSize,
    CHAR *                     pMIN) {
  CallWrapper cw(this, "GetVoiceNumber");
  return cw.CheckReturn(::GetVoiceNumber(
      voiceNumberSize,
      pVoiceNumber,
      minSize,
      pMIN));
}

ULONG Sdk::GetIMSI(
    BYTE                       stringSize,
    CHAR *                     pString) {
  CallWrapper cw(this, "GetIMSI");
  return cw.CheckReturn(::GetIMSI(
      stringSize,
      pString));
}

ULONG Sdk::GetSerialNumbers(
    BYTE                       esnSize,
    CHAR *                     pESNString,
    BYTE                       imeiSize,
    CHAR *                     pIMEIString,
    BYTE                       meidSize,
    CHAR *                     pMEIDString) {
  CallWrapper cw(this, "GetSerialNumbers");
  return cw.CheckReturn(::GetSerialNumbers(
      esnSize,
      pESNString,
      imeiSize,
      pIMEIString,
      meidSize,
      pMEIDString));
}

ULONG Sdk::SetLock(
    ULONG                      state,
    CHAR *                     pCurrentPIN) {
  CallWrapper cw(this, "SetLock");
  return cw.CheckReturn(::SetLock(
      state,
      pCurrentPIN));
}

ULONG Sdk::QueryLock(ULONG * pState) {
  CallWrapper cw(this, "QueryLock");
  return cw.CheckReturn(::QueryLock(pState));
}

ULONG Sdk::ChangeLockPIN(
    CHAR *                     pCurrentPIN,
    CHAR *                     pDesiredPIN) {
  CallWrapper cw(this, "ChangeLockPIN");
  return cw.CheckReturn(::ChangeLockPIN(
      pCurrentPIN,
      pDesiredPIN));
}

ULONG Sdk::GetHardwareRevision(
    BYTE                       stringSize,
    CHAR *                     pString) {
  CallWrapper cw(this, "GetHardwareRevision");
  return cw.CheckReturn(::GetHardwareRevision(
      stringSize,
      pString));
}

ULONG Sdk::GetPRLVersion(WORD * pPRLVersion) {
  CallWrapper cw(this, "GetPRLVersion");
  return cw.CheckReturn(::GetPRLVersion(pPRLVersion));
}

ULONG Sdk::GetERIFile(
    ULONG *                    pFileSize,
    BYTE *                     pFile) {
  CallWrapper cw(this, "GetERIFile");
  return cw.CheckReturn(::GetERIFile(
      pFileSize,
      pFile));
}

ULONG Sdk::ActivateAutomatic(const CHAR * pActivationCode) {
  TemporaryCopier mutableActivationCode(pActivationCode);
  CallWrapper cw(this, "ActivateAutomatic");
  return cw.CheckReturn(::ActivateAutomatic(mutableActivationCode.get()));
}

ULONG Sdk::ActivateManual(
    const CHAR *                pSPC,
    WORD                        sid,
    const CHAR *                pMDN,
    const CHAR *                pMIN,
    ULONG                       prlSize,
    BYTE *                      pPRL,
    const CHAR *                pMNHA,
    const CHAR *                pMNAAA) {
  TemporaryCopier mutableSPC(pSPC);
  TemporaryCopier mutableMDN(pMDN);
  TemporaryCopier mutableMIN(pMIN);
  TemporaryCopier mutableMNHA(pMNHA);
  TemporaryCopier mutableMNAAA(pMNAAA);

  CallWrapper cw(this, "ActivateManual");
  return cw.CheckReturn(::ActivateManual(
      mutableSPC.get(),
      sid,
      mutableMDN.get(),
      mutableMIN.get(),
      prlSize,
      pPRL,
      mutableMNHA.get(),
      mutableMNAAA.get()));
}

ULONG Sdk::ResetToFactoryDefaults(CHAR * pSPC) {
  CallWrapper cw(this, "ResetToFactoryDefaults");
  return cw.CheckReturn(::ResetToFactoryDefaults(pSPC));
}

ULONG Sdk::GetActivationState(ULONG * pActivationState) {
  CallWrapper cw(this, "GetActivationState");
  return cw.CheckReturn(::GetActivationState(pActivationState));
}

ULONG Sdk::SetPower(ULONG powerMode) {
  CallWrapper cw(this, "SetPower");
  return cw.CheckReturn(::SetPower(powerMode));
}

ULONG Sdk::GetPower(ULONG * pPowerMode) {
  CallWrapper cw(this, "GetPower");
  return cw.CheckReturn(::GetPower(pPowerMode));
}

ULONG Sdk::GetOfflineReason(
    ULONG *                    pReasonMask,
    ULONG *                    pbPlatform) {
  CallWrapper cw(this, "GetOfflineReason");
  return cw.CheckReturn(::GetOfflineReason(
      pReasonMask,
      pbPlatform));
}

ULONG Sdk::GetNetworkTime(
    ULONGLONG *                pTimeCount,
    ULONG *                    pTimeSource) {
  CallWrapper cw(this, "GetNetworkTime");
  return cw.CheckReturn(::GetNetworkTime(
      pTimeCount,
      pTimeSource));
}

ULONG Sdk::ValidateSPC(CHAR * pSPC) {
  CallWrapper cw(this, "ValidateSPC");
  return cw.CheckReturn(::ValidateSPC(pSPC));
}

ULONG Sdk::DeleteSMS(
    ULONG                      storageType,
    ULONG *                    pMessageIndex,
    ULONG *                    pMessageTag) {
  CallWrapper cw(this, "DeleteSMS");
  return cw.CheckReturn(::DeleteSMS(
      storageType,
      pMessageIndex,
      pMessageTag));
}

ULONG Sdk::GetSMSList(
    ULONG                      storageType,
    ULONG *                    pRequestedTag,
    ULONG *                    pMessageListSize,
    BYTE *                     pMessageList) {
  CallWrapper cw(this, "GetSMSList");
  return cw.CheckReturn(::GetSMSList(
      storageType,
      pRequestedTag,
      pMessageListSize,
      pMessageList));
}

ULONG Sdk::GetSMS(
    ULONG                      storageType,
    ULONG                      messageIndex,
    ULONG *                    pMessageTag,
    ULONG *                    pMessageFormat,
    ULONG *                    pMessageSize,
    BYTE *                     pMessage) {
  CallWrapper cw(this, "GetSMS");
  return cw.CheckReturn(::GetSMS(
      storageType,
      messageIndex,
      pMessageTag,
      pMessageFormat,
      pMessageSize,
      pMessage));
}

ULONG Sdk::ModifySMSStatus(
    ULONG                      storageType,
    ULONG                      messageIndex,
    ULONG                      messageTag) {
  CallWrapper cw(this, "ModifySMSStatus");
  return cw.CheckReturn(::ModifySMSStatus(
      storageType,
      messageIndex,
      messageTag));
}

ULONG Sdk::SaveSMS(
    ULONG                      storageType,
    ULONG                      messageFormat,
    ULONG                      messageSize,
    BYTE *                     pMessage,
    ULONG *                    pMessageIndex) {
  CallWrapper cw(this, "SaveSMS");
  return cw.CheckReturn(::SaveSMS(
      storageType,
      messageFormat,
      messageSize,
      pMessage,
      pMessageIndex));
}

ULONG Sdk::SendSMS(
    ULONG                      messageFormat,
    ULONG                      messageSize,
    BYTE *                     pMessage,
    ULONG *                    pMessageFailureCode) {
  CallWrapper cw(this, "SendSMS");
  return cw.CheckReturn(::SendSMS(
      messageFormat,
      messageSize,
      pMessage,
      pMessageFailureCode));
}

ULONG Sdk::GetSMSCAddress(
    BYTE                       addressSize,
    CHAR *                     pSMSCAddress,
    BYTE                       typeSize,
    CHAR *                     pSMSCType) {
  CallWrapper cw(this, "GetSMSCAddress");
  return cw.CheckReturn(::GetSMSCAddress(
      addressSize,
      pSMSCAddress,
      typeSize,
      pSMSCType));
}

ULONG Sdk::SetSMSCAddress(
    CHAR *                     pSMSCAddress,
    CHAR *                     pSMSCType) {
  CallWrapper cw(this, "SetSMSCAddress");
  return cw.CheckReturn(::SetSMSCAddress(
      pSMSCAddress,
      pSMSCType));
}

ULONG Sdk::UIMSetPINProtection(
    ULONG                      id,
    ULONG                      bEnable,
    CHAR *                     pValue,
    ULONG *                    pVerifyRetriesLeft,
    ULONG *                    pUnblockRetriesLeft) {
  CallWrapper cw(this, "UIMSetPINProtection");
  return cw.CheckReturn(::UIMSetPINProtection(
      id,
      bEnable,
      pValue,
      pVerifyRetriesLeft,
      pUnblockRetriesLeft));
}

ULONG Sdk::UIMVerifyPIN(
    ULONG                      id,
    CHAR *                     pValue,
    ULONG *                    pVerifyRetriesLeft,
    ULONG *                    pUnblockRetriesLeft) {
  CallWrapper cw(this, "UIMVerifyPIN");
  return cw.CheckReturn(::UIMVerifyPIN(
      id,
      pValue,
      pVerifyRetriesLeft,
      pUnblockRetriesLeft));
}

ULONG Sdk::UIMUnblockPIN(
    ULONG                      id,
    CHAR *                     pPUKValue,
    CHAR *                     pNewValue,
    ULONG *                    pVerifyRetriesLeft,
    ULONG *                    pUnblockRetriesLeft) {
  CallWrapper cw(this, "UIMUnblockPIN");
  return cw.CheckReturn(::UIMUnblockPIN(
      id,
      pPUKValue,
      pNewValue,
      pVerifyRetriesLeft,
      pUnblockRetriesLeft));
}

ULONG Sdk::UIMChangePIN(
    ULONG                      id,
    CHAR *                     pOldValue,
    CHAR *                     pNewValue,
    ULONG *                    pVerifyRetriesLeft,
    ULONG *                    pUnblockRetriesLeft) {
  CallWrapper cw(this, "UIMChangePIN");
  return cw.CheckReturn(::UIMChangePIN(
      id,
      pOldValue,
      pNewValue,
      pVerifyRetriesLeft,
      pUnblockRetriesLeft));
}

ULONG Sdk::UIMGetPINStatus(
    ULONG                      id,
    ULONG *                    pStatus,
    ULONG *                    pVerifyRetriesLeft,
    ULONG *                    pUnblockRetriesLeft) {
  CallWrapper cw(this, "UIMGetPINStatus");
  return cw.CheckReturn(::UIMGetPINStatus(
      id,
      pStatus,
      pVerifyRetriesLeft,
      pUnblockRetriesLeft));
}

ULONG Sdk::UIMGetICCID(
    BYTE                       stringSize,
    CHAR *                     pString) {
  CallWrapper cw(this, "UIMGetICCID");
  return cw.CheckReturn(::UIMGetICCID(
      stringSize,
      pString));
}
ULONG Sdk::UIMGetControlKeyStatus(
    ULONG                      id,
    ULONG *                    pStatus,
    ULONG *                    pVerifyRetriesLeft,
    ULONG *                    pUnblockRetriesLeft) {
  CallWrapper cw(this, "UIMGetControlKeyStatus");
  return cw.CheckReturn(::UIMGetControlKeyStatus(
      id,
      pStatus,
      pVerifyRetriesLeft,
      pUnblockRetriesLeft));
}

ULONG Sdk::UIMSetControlKeyProtection(
    ULONG                      id,
    ULONG                      status,
    CHAR *                     pValue,
    ULONG *                    pVerifyRetriesLeft) {
  CallWrapper cw(this, "UIMSetControlKeyProtection");
  return cw.CheckReturn(::UIMSetControlKeyProtection(
      id,
      status,
      pValue,
      pVerifyRetriesLeft));
}

ULONG Sdk::UIMUnblockControlKey(
    ULONG                      id,
    CHAR *                     pValue,
    ULONG *                    pUnblockRetriesLeft) {
  CallWrapper cw(this, "UIMUnblockControlKey");
  return cw.CheckReturn(::UIMUnblockControlKey(
      id,
      pValue,
      pUnblockRetriesLeft));
}

ULONG Sdk::GetPDSState(
    ULONG *                    pEnabled,
    ULONG *                    pTracking) {
  CallWrapper cw(this, "GetPDSState");
  return cw.CheckReturn(::GetPDSState(
      pEnabled,
      pTracking));
}

ULONG Sdk::SetPDSState(ULONG enable) {
  CallWrapper cw(this, "SetPDSState");
  return cw.CheckReturn(::SetPDSState(enable));
}

ULONG Sdk::PDSInjectTimeReference(
    ULONGLONG                  systemTime,
    USHORT                     systemDiscontinuities) {
  CallWrapper cw(this, "PDSInjectTimeReference");
  return cw.CheckReturn(::PDSInjectTimeReference(
      systemTime,
      systemDiscontinuities));
}

ULONG Sdk::GetPDSDefaults(
    ULONG *                    pOperation,
    BYTE *                     pTimeout,
    ULONG *                    pInterval,
    ULONG *                    pAccuracy) {
  CallWrapper cw(this, "GetPDSDefaults");
  return cw.CheckReturn(::GetPDSDefaults(
      pOperation,
      pTimeout,
      pInterval,
      pAccuracy));
}

ULONG Sdk::SetPDSDefaults(
    ULONG                      operation,
    BYTE                       timeout,
    ULONG                      interval,
    ULONG                      accuracy) {
  CallWrapper cw(this, "SetPDSDefaults");
  return cw.CheckReturn(::SetPDSDefaults(
      operation,
      timeout,
      interval,
      accuracy));
}

ULONG Sdk::GetXTRAAutomaticDownload(
    ULONG *                    pbEnabled,
    USHORT *                   pInterval) {
  CallWrapper cw(this, "GetXTRAAutomaticDownload");
  return cw.CheckReturn(::GetXTRAAutomaticDownload(
      pbEnabled,
      pInterval));
}

ULONG Sdk::SetXTRAAutomaticDownload(
    ULONG                      bEnabled,
    USHORT                     interval) {
  CallWrapper cw(this, "SetXTRAAutomaticDownload");
  return cw.CheckReturn(::SetXTRAAutomaticDownload(
      bEnabled,
      interval));
}

ULONG Sdk::GetXTRANetwork(ULONG * pPreference) {
  CallWrapper cw(this, "GetXTRANetwork");
  return cw.CheckReturn(::GetXTRANetwork(pPreference));
}

ULONG Sdk::SetXTRANetwork(ULONG preference) {
  CallWrapper cw(this, "SetXTRANetwork");
  return cw.CheckReturn(::SetXTRANetwork(preference));
}

ULONG Sdk::GetXTRAValidity(
    USHORT *                   pGPSWeek,
    USHORT *                   pGPSWeekOffset,
    USHORT *                   pDuration) {
  CallWrapper cw(this, "GetXTRAValidity");
  return cw.CheckReturn(::GetXTRAValidity(
      pGPSWeek,
      pGPSWeekOffset,
      pDuration));
}

ULONG Sdk::ForceXTRADownload() {
  CallWrapper cw(this, "ForceXTRADownload");
  return cw.CheckReturn(::ForceXTRADownload());
}

ULONG Sdk::GetAGPSConfig(
    ULONG *                    pServerAddress,
    ULONG *                    pServerPort) {
  CallWrapper cw(this, "GetAGPSConfig");
  return cw.CheckReturn(::GetAGPSConfig(
      pServerAddress,
      pServerPort));
}

ULONG Sdk::SetAGPSConfig(
    ULONG                      serverAddress,
    ULONG                      serverPort) {
  CallWrapper cw(this, "SetAGPSConfig");
  return cw.CheckReturn(::SetAGPSConfig(
      serverAddress,
      serverPort));
}

ULONG Sdk::GetServiceAutomaticTracking(ULONG * pbAuto) {
  CallWrapper cw(this, "GetServiceAutomaticTracking");
  return cw.CheckReturn(::GetServiceAutomaticTracking(pbAuto));
}

ULONG Sdk::SetServiceAutomaticTracking(ULONG bAuto) {
  CallWrapper cw(this, "SetServiceAutomaticTracking");
  return cw.CheckReturn(::SetServiceAutomaticTracking(bAuto));
}

ULONG Sdk::GetPortAutomaticTracking(ULONG * pbAuto) {
  CallWrapper cw(this, "GetPortAutomaticTracking");
  return cw.CheckReturn(::GetPortAutomaticTracking(pbAuto));
}

ULONG Sdk::SetPortAutomaticTracking(ULONG bAuto) {
  CallWrapper cw(this, "SetPortAutomaticTracking");
  return cw.CheckReturn(::SetPortAutomaticTracking(bAuto));
}

ULONG Sdk::ResetPDSData(
    ULONG *                    pGPSDataMask,
    ULONG *                    pCellDataMask) {
  CallWrapper cw(this, "ResetPDSData");
  return cw.CheckReturn(::ResetPDSData(
      pGPSDataMask,
      pCellDataMask));
}

ULONG Sdk::CATSendTerminalResponse(
    ULONG                      refID,
    ULONG                      dataLen,
    BYTE *                     pData) {
  CallWrapper cw(this, "CATSendTerminalResponse");
  return cw.CheckReturn(::CATSendTerminalResponse(
      refID,
      dataLen,
      pData));
}

ULONG Sdk::CATSendEnvelopeCommand(
    ULONG                      cmdID,
    ULONG                      dataLen,
    BYTE *                     pData) {
  CallWrapper cw(this, "CATSendEnvelopeCommand");
  return cw.CheckReturn(::CATSendEnvelopeCommand(
      cmdID,
      dataLen,
      pData));
}

ULONG Sdk::GetSMSWake(
    ULONG *                    pbEnabled,
    ULONG *                    pWakeMask) {
  CallWrapper cw(this, "GetSMSWake");
  return cw.CheckReturn(::GetSMSWake(
      pbEnabled,
      pWakeMask));
}

ULONG Sdk::SetSMSWake(
    ULONG                      bEnable,
    ULONG                      wakeMask) {
  CallWrapper cw(this, "SetSMSWake");
  return cw.CheckReturn(::SetSMSWake(
      bEnable,
      wakeMask));
}

ULONG Sdk::OMADMStartSession(ULONG sessionType) {
  CallWrapper cw(this, "OMADMStartSession");
  return cw.CheckReturn(::OMADMStartSession(sessionType));
}

ULONG Sdk::OMADMCancelSession() {
  CallWrapper cw(this, "OMADMCancelSession");
  return cw.CheckReturn(::OMADMCancelSession());
}

ULONG Sdk::OMADMGetSessionInfo(
    ULONG *                    pSessionState,
    ULONG *                    pSessionType,
    ULONG *                    pFailureReason,
    BYTE *                     pRetryCount,
    WORD *                     pSessionPause,
    WORD *                     pTimeRemaining) {
  CallWrapper cw(this, "OMADMGetSessionInfo");
  return cw.CheckReturn(::OMADMGetSessionInfo(
      pSessionState,
      pSessionType,
      pFailureReason,
      pRetryCount,
      pSessionPause,
      pTimeRemaining));
}

ULONG Sdk::OMADMGetPendingNIA(
    ULONG *                    pSessionType,
    USHORT *                   pSessionID) {
  CallWrapper cw(this, "OMADMGetPendingNIA");
  return cw.CheckReturn(::OMADMGetPendingNIA(
      pSessionType,
      pSessionID));
}

ULONG Sdk::OMADMSendSelection(
    ULONG                      selection,
    USHORT                     sessionID) {
  CallWrapper cw(this, "OMADMSendSelection");
  return cw.CheckReturn(::OMADMSendSelection(
      selection,
      sessionID));
}

ULONG Sdk::OMADMGetFeatureSettings(
    ULONG *                    pbProvisioning,
    ULONG *                    pbPRLUpdate) {
  CallWrapper cw(this, "OMADMGetFeatureSettings");
  return cw.CheckReturn(::OMADMGetFeatureSettings(
      pbProvisioning,
      pbPRLUpdate));
}

ULONG Sdk::OMADMSetProvisioningFeature(
    ULONG                      bProvisioning) {
  CallWrapper cw(this, "OMADMSetProvisioningFeature");
  return cw.CheckReturn(::OMADMSetProvisioningFeature(
      bProvisioning));
}

ULONG Sdk::OMADMSetPRLUpdateFeature(
    ULONG                      bPRLUpdate) {
  CallWrapper cw(this, "OMADMSetPRLUpdateFeature");
  return cw.CheckReturn(::OMADMSetPRLUpdateFeature(
      bPRLUpdate));
}

ULONG Sdk::UpgradeFirmware(CHAR * pDestinationPath) {
  CallWrapper cw(this, "UpgradeFirmware");
  return cw.CheckReturn(::UpgradeFirmware(pDestinationPath));
}

ULONG Sdk::GetImageInfo(
    CHAR *                     pPath,
    ULONG *                    pFirmwareID,
    ULONG *                    pTechnology,
    ULONG *                    pCarrier,
    ULONG *                    pRegion,
    ULONG *                    pGPSCapability) {
  CallWrapper cw(this, "GetImageInfo");
  return cw.CheckReturn(::GetImageInfo(
      pPath,
      pFirmwareID,
      pTechnology,
      pCarrier,
      pRegion,
      pGPSCapability));
}

ULONG Sdk::GetImageStore(
    WORD                       pathSize,
    CHAR *                     pImageStorePath) {
  CallWrapper cw(this, "GetImageStore");
  return cw.CheckReturn(::GetImageStore(
      pathSize,
      pImageStorePath));
}

ULONG Sdk::SetSessionStateCallback(tFNSessionState pCallback) {
  CallWrapper cw(this, "SetSessionStateCallback");
  return cw.CheckReturn(::SetSessionStateCallback(pCallback));
}

ULONG Sdk::SetByteTotalsCallback(
    tFNByteTotals              pCallback,
    BYTE                       interval) {
  CallWrapper cw(this, "SetByteTotalsCallback");
  return cw.CheckReturn(::SetByteTotalsCallback(
      pCallback,
      interval));
}

ULONG Sdk::SetDataCapabilitiesCallback(
    tFNDataCapabilities        pCallback) {
  CallWrapper cw(this, "SetDataCapabilitiesCallback");
  return cw.CheckReturn(::SetDataCapabilitiesCallback(
      pCallback));
}

ULONG Sdk::SetDataBearerCallback(tFNDataBearer pCallback) {
  CallWrapper cw(this, "SetDataBearerCallback");
  return cw.CheckReturn(::SetDataBearerCallback(pCallback));
}

ULONG Sdk::SetDormancyStatusCallback(
    tFNDormancyStatus          pCallback) {
  CallWrapper cw(this, "SetDormancyStatusCallback");
  return cw.CheckReturn(::SetDormancyStatusCallback(
      pCallback));
}

ULONG Sdk::SetMobileIPStatusCallback(
    tFNMobileIPStatus          pCallback) {
  CallWrapper cw(this, "SetMobileIPStatusCallback");
  return cw.CheckReturn(::SetMobileIPStatusCallback(
      pCallback));
}

ULONG Sdk::SetActivationStatusCallback(
    tFNActivationStatus        pCallback) {
  CallWrapper cw(this, "SetActivationStatusCallback");
  return cw.CheckReturn(::SetActivationStatusCallback(
      pCallback));
}

ULONG Sdk::SetPowerCallback(tFNPower pCallback) {
  CallWrapper cw(this, "SetPowerCallback");
  return cw.CheckReturn(::SetPowerCallback(pCallback));
}
ULONG Sdk::SetRoamingIndicatorCallback(
    tFNRoamingIndicator        pCallback) {
  CallWrapper cw(this, "SetRoamingIndicatorCallback");
  return cw.CheckReturn(::SetRoamingIndicatorCallback(
      pCallback));
}

ULONG Sdk::SetSignalStrengthCallback(
    tFNSignalStrength          pCallback,
    BYTE                       thresholdsSize,
    INT8 *                     pThresholds) {
  CallWrapper cw(this, "SetSignalStrengthCallback");
  return cw.CheckReturn(::SetSignalStrengthCallback(
      pCallback,
      thresholdsSize,
      pThresholds));
}

ULONG Sdk::SetRFInfoCallback(tFNRFInfo pCallback) {
  CallWrapper cw(this, "SetRFInfoCallback");
  return cw.CheckReturn(::SetRFInfoCallback(pCallback));
}

ULONG Sdk::SetLURejectCallback(tFNLUReject pCallback) {
  CallWrapper cw(this, "SetLURejectCallback");
  return cw.CheckReturn(::SetLURejectCallback(pCallback));
}

ULONG Sdk::SetNewSMSCallback(tFNNewSMS pCallback) {
  CallWrapper cw(this, "SetNewSMSCallback");
  return cw.CheckReturn(::SetNewSMSCallback(pCallback));
}

ULONG Sdk::SetNMEACallback(tFNNewNMEA pCallback) {
  CallWrapper cw(this, "SetNMEACallback");
  return cw.CheckReturn(::SetNMEACallback(pCallback));
}

ULONG Sdk::SetNMEAPlusCallback(tFNNewNMEAPlus pCallback) {
  CallWrapper cw(this, "SetNMEAPlusCallback");
  return cw.CheckReturn(::SetNMEAPlusCallback(pCallback));
}

ULONG Sdk::SetPDSStateCallback(tFNPDSState pCallback) {
  CallWrapper cw(this, "SetPDSStateCallback");
  return cw.CheckReturn(::SetPDSStateCallback(pCallback));
}

ULONG Sdk::SetCATEventCallback(
    tFNCATEvent                pCallback,
    ULONG                      eventMask,
    ULONG *                    pErrorMask) {
  CallWrapper cw(this, "SetCATEventCallback");
  return cw.CheckReturn(::SetCATEventCallback(
      pCallback,
      eventMask,
      pErrorMask));
}

ULONG Sdk::SetOMADMAlertCallback(tFNOMADMAlert pCallback) {
  CallWrapper cw(this, "SetOMADMAlertCallback");
  return cw.CheckReturn(::SetOMADMAlertCallback(pCallback));
}

ULONG Sdk::SetOMADMStateCallback(tFNOMADMState pCallback) {
  CallWrapper cw(this, "SetOMADMStateCallback");
  return cw.CheckReturn(::SetOMADMStateCallback(pCallback));
}

}   // Namespace Gobi
