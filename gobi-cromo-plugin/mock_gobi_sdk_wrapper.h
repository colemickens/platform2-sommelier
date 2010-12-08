/*===========================================================================
  FILE:
  mock_gobi_sdk_wrapper.h,
  derived from
  gobi_sdk_wrapper.h, QCWWANCMAPI2k.h

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

#include <gtest/gtest.h>

namespace gobi {

// ErrorSdk overrides every QCWWAN API method in gobi::Sdk.  It
// returns an error for every SDK call (except for those that register
// callbacks---they always succeed without doing anything).  Depending
// on the setting of strict_, calls to the ErrorSdk will also make the
// current test fail.

class ErrorSdk : public gobi::Sdk {
 public:
  ErrorSdk() : Sdk(NULL),strict_(false) {}

  virtual ULONG QCWWANEnumerateDevices(
      BYTE *                     pDevicesSize,
      BYTE *                     pDevices) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG QCWWANConnect(
      CHAR *                     pDeviceNode,
      CHAR *                     pDeviceKey) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG QCWWANDisconnect() {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG QCWWANGetConnectedDeviceID(
      ULONG                      deviceNodeSize,
      CHAR *                     pDeviceNode,
      ULONG                      deviceKeySize,
      CHAR *                     pDeviceKey) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetSessionState(ULONG * pState) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetSessionDuration(ULONGLONG * pDuration) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetDormancyState(ULONG * pState) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetAutoconnect(ULONG * pSetting) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG SetAutoconnect(ULONG setting) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG SetDefaultProfile(
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
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetDefaultProfile(
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
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG StartDataSession(
      ULONG *                    pTechnology,
      CHAR *                     pAPNName,
      ULONG *                    pAuthentication,
      CHAR *                     pUsername,
      CHAR *                     pPassword,
      ULONG *                    pSessionId,
      ULONG *                    pFailureReason) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG StopDataSession(ULONG sessionId) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetIPAddress(ULONG * pIPAddress) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetConnectionRate(
      ULONG *                    pCurrentChannelTXRate,
      ULONG *                    pCurrentChannelRXRate,
      ULONG *                    pMaxChannelTXRate,
      ULONG *                    pMaxChannelRXRate) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetPacketStatus(
      ULONG *                    pTXPacketSuccesses,
      ULONG *                    pRXPacketSuccesses,
      ULONG *                    pTXPacketErrors,
      ULONG *                    pRXPacketErrors,
      ULONG *                    pTXPacketOverflows,
      ULONG *                    pRXPacketOverflows) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetByteTotals(
      ULONGLONG *                pTXTotalBytes,
      ULONGLONG *                pRXTotalBytes) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG SetMobileIP(ULONG mode) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetMobileIP(ULONG * pMode) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG SetActiveMobileIPProfile(
      CHAR *                     pSPC,
      BYTE                       index) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetActiveMobileIPProfile(BYTE * pIndex) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG SetMobileIPProfile(
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
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetMobileIPProfile(
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
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG SetMobileIPParameters(
      CHAR *                     pSPC,
      ULONG *                    pMode,
      BYTE *                     pRetryLimit,
      BYTE *                     pRetryInterval,
      BYTE *                     pReRegPeriod,
      BYTE *                     pReRegTraffic,
      BYTE *                     pHAAuthenticator,
      BYTE *                     pHA2002bis) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetMobileIPParameters(
      ULONG *                    pMode,
      BYTE *                     pRetryLimit,
      BYTE *                     pRetryInterval,
      BYTE *                     pReRegPeriod,
      BYTE *                     pReRegTraffic,
      BYTE *                     pHAAuthenticator,
      BYTE *                     pHA2002bis) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetLastMobileIPError(ULONG * pError) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetANAAAAuthenticationStatus(ULONG * pStatus) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetSignalStrengths(
      ULONG *                    pArraySizes,
      INT8 *                     pSignalStrengths,
      ULONG *                    pRadioInterfaces) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetRFInfo(
      BYTE *                     pInstanceSize,
      BYTE *                     pInstances) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG PerformNetworkScan(
      BYTE *                     pInstanceSize,
      BYTE *                     pInstances) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG InitiateNetworkRegistration(
      ULONG                      regType,
      WORD                       mcc,
      WORD                       mnc,
      ULONG                      rat) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG InitiateDomainAttach(ULONG action) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetServingNetwork(
      ULONG *                    pRegistrationState,
      ULONG *                    pRAN,
      BYTE *                     pRadioIfacesSize,
      BYTE *                     pRadioIfaces,
      ULONG *                    pRoaming,
      WORD *                     pMCC,
      WORD *                     pMNC) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetServingNetworkCapabilities(
      BYTE *                     pDataCapsSize,
      BYTE *                     pDataCaps) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetDataBearerTechnology(ULONG * pDataBearer) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetHomeNetwork(
      WORD *                     pMCC,
      WORD *                     pMNC,
      BYTE                       nameSize,
      CHAR *                     pName,
      WORD *                     pSID,
      WORD *                     pNID) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG SetNetworkPreference(
      ULONG                      technologyPref,
      ULONG                      duration) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetNetworkPreference(
      ULONG *                    pTechnologyPref,
      ULONG *                    pDuration,
      ULONG *                    pPersistentTechnologyPref) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG SetCDMANetworkParameters(
      CHAR *                     pSPC,
      BYTE *                     pForceRev0,
      BYTE *                     pCustomSCP,
      ULONG *                    pProtocol,
      ULONG *                    pBroadcast,
      ULONG *                    pApplication,
      ULONG *                    pRoaming) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetCDMANetworkParameters(
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
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetACCOLC(BYTE * pACCOLC) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG SetACCOLC(
      CHAR *                     pSPC,
      BYTE                       accolc) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetDeviceCapabilities(
      ULONG *                    pMaxTXChannelRate,
      ULONG *                    pMaxRXChannelRate,
      ULONG *                    pDataServiceCapability,
      ULONG *                    pSimCapability,
      ULONG *                    pRadioIfacesSize,
      BYTE *                     pRadioIfaces) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetManufacturer(
      BYTE                       stringSize,
      CHAR *                     pString) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetModelID(
      BYTE                       stringSize,
      CHAR *                     pString) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetFirmwareRevision(
      BYTE                       stringSize,
      CHAR *                     pString) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetFirmwareRevisions(
      BYTE                       amssSize,
      CHAR *                     pAMSSString,
      BYTE                       bootSize,
      CHAR *                     pBootString,
      BYTE                       priSize,
      CHAR *                     pPRIString) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetFirmwareInfo(
      ULONG *                    pFirmwareID,
      ULONG *                    pTechnology,
      ULONG *                    pCarrier,
      ULONG *                    pRegion,
      ULONG *                    pGPSCapability) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetVoiceNumber(
      BYTE                       voiceNumberSize,
      CHAR *                     pVoiceNumber,
      BYTE                       minSize,
      CHAR *                     pMIN) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetIMSI(
      BYTE                       stringSize,
      CHAR *                     pString) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetSerialNumbers(
      BYTE                       esnSize,
      CHAR *                     pESNString,
      BYTE                       imeiSize,
      CHAR *                     pIMEIString,
      BYTE                       meidSize,
      CHAR *                     pMEIDString) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG SetLock(
      ULONG                      state,
      CHAR *                     pCurrentPIN) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG QueryLock(ULONG * pState) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG ChangeLockPIN(
      CHAR *                     pCurrentPIN,
      CHAR *                     pDesiredPIN) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetHardwareRevision(
      BYTE                       stringSize,
      CHAR *                     pString) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetPRLVersion(WORD * pPRLVersion) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetERIFile(
      ULONG *                    pFileSize,
      BYTE *                     pFile) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG ActivateAutomatic(const CHAR * pActivationCode) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG ActivateManual(
      const CHAR *               pSPC,
      WORD                       sid,
      const CHAR *               pMDN,
      const CHAR *               pMIN,
      ULONG                      prlSize,
      BYTE *                     pPRL,
      const CHAR *               pMNHA,
      const CHAR *               pMNAAA) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG ResetToFactoryDefaults(CHAR * pSPC) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetActivationState(ULONG * pActivationState) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG SetPower(ULONG powerMode) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetPower(ULONG * pPowerMode) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetOfflineReason(
      ULONG *                    pReasonMask,
      ULONG *                    pbPlatform) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetNetworkTime(
      ULONGLONG *                pTimeCount,
      ULONG *                    pTimeSource) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG ValidateSPC(CHAR * pSPC) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG DeleteSMS(
      ULONG                      storageType,
      ULONG *                    pMessageIndex,
      ULONG *                    pMessageTag) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetSMSList(
      ULONG                      storageType,
      ULONG *                    pRequestedTag,
      ULONG *                    pMessageListSize,
      BYTE *                     pMessageList) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetSMS(
      ULONG                      storageType,
      ULONG                      messageIndex,
      ULONG *                    pMessageTag,
      ULONG *                    pMessageFormat,
      ULONG *                    pMessageSize,
      BYTE *                     pMessage) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG ModifySMSStatus(
      ULONG                      storageType,
      ULONG                      messageIndex,
      ULONG                      messageTag) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG SaveSMS(
      ULONG                      storageType,
      ULONG                      messageFormat,
      ULONG                      messageSize,
      BYTE *                     pMessage,
      ULONG *                    pMessageIndex) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG SendSMS(
      ULONG                      messageFormat,
      ULONG                      messageSize,
      BYTE *                     pMessage,
      ULONG *                    pMessageFailureCode) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetSMSCAddress(
      BYTE                       addressSize,
      CHAR *                     pSMSCAddress,
      BYTE                       typeSize,
      CHAR *                     pSMSCType) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG SetSMSCAddress(
      CHAR *                     pSMSCAddress,
      CHAR *                     pSMSCType) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG UIMSetPINProtection(
      ULONG                      id,
      ULONG                      bEnable,
      CHAR *                     pValue,
      ULONG *                    pVerifyRetriesLeft,
      ULONG *                    pUnblockRetriesLeft) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG UIMVerifyPIN(
      ULONG                      id,
      CHAR *                     pValue,
      ULONG *                    pVerifyRetriesLeft,
      ULONG *                    pUnblockRetriesLeft) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG UIMUnblockPIN(
      ULONG                      id,
      CHAR *                     pPUKValue,
      CHAR *                     pNewValue,
      ULONG *                    pVerifyRetriesLeft,
      ULONG *                    pUnblockRetriesLeft) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG UIMChangePIN(
      ULONG                      id,
      CHAR *                     pOldValue,
      CHAR *                     pNewValue,
      ULONG *                    pVerifyRetriesLeft,
      ULONG *                    pUnblockRetriesLeft) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG UIMGetPINStatus(
      ULONG                      id,
      ULONG *                    pStatus,
      ULONG *                    pVerifyRetriesLeft,
      ULONG *                    pUnblockRetriesLeft) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG UIMGetICCID(
      BYTE                       stringSize,
      CHAR *                     pString) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }
  virtual ULONG UIMGetControlKeyStatus(
      ULONG                      id,
      ULONG *                    pStatus,
      ULONG *                    pVerifyRetriesLeft,
      ULONG *                    pUnblockRetriesLeft) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG UIMSetControlKeyProtection(
      ULONG                      id,
      ULONG                      status,
      CHAR *                     pValue,
      ULONG *                    pVerifyRetriesLeft) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG UIMUnblockControlKey(
      ULONG                      id,
      CHAR *                     pValue,
      ULONG *                    pUnblockRetriesLeft) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetPDSState(
      ULONG *                    pEnabled,
      ULONG *                    pTracking) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG SetPDSState(ULONG enable) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG PDSInjectTimeReference(
      ULONGLONG                  systemTime,
      USHORT                     systemDiscontinuities) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetPDSDefaults(
      ULONG *                    pOperation,
      BYTE *                     pTimeout,
      ULONG *                    pInterval,
      ULONG *                    pAccuracy) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG SetPDSDefaults(
      ULONG                      operation,
      BYTE                       timeout,
      ULONG                      interval,
      ULONG                      accuracy) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetXTRAAutomaticDownload(
      ULONG *                    pbEnabled,
      USHORT *                   pInterval) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG SetXTRAAutomaticDownload(
      ULONG                      bEnabled,
      USHORT                     interval) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetXTRANetwork(ULONG * pPreference) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG SetXTRANetwork(ULONG preference) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetXTRAValidity(
      USHORT *                   pGPSWeek,
      USHORT *                   pGPSWeekOffset,
      USHORT *                   pDuration) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG ForceXTRADownload() {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetAGPSConfig(
      ULONG *                    pServerAddress,
      ULONG *                    pServerPort) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG SetAGPSConfig(
      ULONG                      serverAddress,
      ULONG                      serverPort) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << __PRETTY_FUNCTION__;
    }
    return kGeneralError;
  }

  virtual ULONG GetServiceAutomaticTracking(ULONG * pbAuto) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << "GetServiceAutomaticTracking";
    }
    return kGeneralError;
  }

  virtual ULONG SetServiceAutomaticTracking(ULONG bAuto) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << "SetServiceAutomaticTracking";
    }
    return kGeneralError;
  }

  virtual ULONG GetPortAutomaticTracking(ULONG * pbAuto) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << "GetPortAutomaticTracking";
    }
    return kGeneralError;
  }

  virtual ULONG SetPortAutomaticTracking(ULONG bAuto) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << "SetPortAutomaticTracking";
    }
    return kGeneralError;
  }

  virtual ULONG ResetPDSData(
      ULONG *                    pGPSDataMask,
      ULONG *                    pCellDataMask) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << "ResetPDSData";
    }
    return kGeneralError;
  }

  virtual ULONG CATSendTerminalResponse(
      ULONG                      refID,
      ULONG                      dataLen,
      BYTE *                     pData) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << "CATSendTerminalResponse";
    }
    return kGeneralError;
  }

  virtual ULONG CATSendEnvelopeCommand(
      ULONG                      cmdID,
      ULONG                      dataLen,
      BYTE *                     pData) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << "CATSendEnvelopeCommand";
    }
    return kGeneralError;
  }

  virtual ULONG GetSMSWake(
      ULONG *                    pbEnabled,
      ULONG *                    pWakeMask) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << "GetSMSWake";
    }
    return kGeneralError;
  }

  virtual ULONG SetSMSWake(
      ULONG                      bEnable,
      ULONG                      wakeMask) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << "SetSMSWake";
    }
    return kGeneralError;
  }

  virtual ULONG OMADMStartSession(ULONG sessionType) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << "OMADMStartSession";
    }
    return kGeneralError;
  }

  virtual ULONG OMADMCancelSession() {
    if (strict_) {
      ADD_FAILURE() << kBadCall << "OMADMCancelSession";
    }
    return kGeneralError;
  }

  virtual ULONG OMADMGetSessionInfo(
      ULONG *                    pSessionState,
      ULONG *                    pSessionType,
      ULONG *                    pFailureReason,
      BYTE *                     pRetryCount,
      WORD *                     pSessionPause,
      WORD *                     pTimeRemaining) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << "OMADMGetSessionInfo";
    }
    return kGeneralError;
  }

  virtual ULONG OMADMGetPendingNIA(
      ULONG *                    pSessionType,
      USHORT *                   pSessionID) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << "OMADMGetPendingNIA";
    }
    return kGeneralError;
  }

  virtual ULONG OMADMSendSelection(
      ULONG                      selection,
      USHORT                     sessionID) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << "OMADMSendSelection";
    }
    return kGeneralError;
  }

  virtual ULONG OMADMGetFeatureSettings(
      ULONG *                    pbProvisioning,
      ULONG *                    pbPRLUpdate) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << "OMADMGetFeatureSettings";
    }
    return kGeneralError;
  }

  virtual ULONG OMADMSetProvisioningFeature(
      ULONG                      bProvisioning) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << "OMADMSetProvisioningFeature";
    }
    return kGeneralError;
  }

  virtual ULONG OMADMSetPRLUpdateFeature(
      ULONG                      bPRLUpdate) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << "OMADMSetPRLUpdateFeature";
    }
    return kGeneralError;
  }

  virtual ULONG UpgradeFirmware(CHAR * pDestinationPath) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << "UpgradeFirmware";
    }
    return kGeneralError;
  }

  virtual ULONG GetImageInfo(
      CHAR *                     pPath,
      ULONG *                    pFirmwareID,
      ULONG *                    pTechnology,
      ULONG *                    pCarrier,
      ULONG *                    pRegion,
      ULONG *                    pGPSCapability) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << "GetImageInfo";
    }
    return kGeneralError;
  }

  virtual ULONG GetImageStore(
      WORD                       pathSize,
      CHAR *                     pImageStorePath) {
    if (strict_) {
      ADD_FAILURE() << kBadCall << "GetImageStore";
    }
    return kGeneralError;
  }

  // By default, we don't want to care which callbacks have been
  // registered, so we return success and do not register errors in
  // these cases.

  virtual ULONG SetSessionStateCallback(tFNSessionState pCallback) {
    return 0;
  }

  virtual ULONG SetByteTotalsCallback(
      tFNByteTotals              pCallback,
      BYTE                       interval) {
    return 0;
  }

  virtual ULONG SetDataCapabilitiesCallback(
      tFNDataCapabilities        pCallback) {
    return 0;
  }

  virtual ULONG SetDataBearerCallback(tFNDataBearer pCallback) {
    return 0;
  }

  virtual ULONG SetDormancyStatusCallback(
      tFNDormancyStatus          pCallback) {
    return 0;
  }

  virtual ULONG SetMobileIPStatusCallback(
      tFNMobileIPStatus          pCallback) {
    return 0;
  }

  virtual ULONG SetActivationStatusCallback(
      tFNActivationStatus        pCallback) {
    return 0;
  }

  virtual ULONG SetPowerCallback(tFNPower pCallback) {
    return 0;
  }

  virtual ULONG SetRoamingIndicatorCallback(
      tFNRoamingIndicator        pCallback) {
    return 0;
  }

  virtual ULONG SetSignalStrengthCallback(
      tFNSignalStrength          pCallback,
      BYTE                       thresholdsSize,
      INT8 *                     pThresholds) {
    return 0;
  }

  virtual ULONG SetRFInfoCallback(tFNRFInfo pCallback) {
    return 0;
  }

  virtual ULONG SetLURejectCallback(tFNLUReject pCallback) {
    return 0;
  }

  virtual ULONG SetNewSMSCallback(tFNNewSMS pCallback) {
    return 0;
  }

  virtual ULONG SetNMEACallback(tFNNewNMEA pCallback) {
    return 0;
  }

  virtual ULONG SetNMEAPlusCallback(tFNNewNMEAPlus pCallback) {
    return 0;
  }

  virtual ULONG SetPDSStateCallback(tFNPDSState pCallback) {
    return 0;
  }

  virtual ULONG SetCATEventCallback(
      tFNCATEvent                pCallback,
      ULONG                      eventMask,
      ULONG *                    pErrorMask) {
    return 0;
  }

  virtual ULONG SetOMADMAlertCallback(tFNOMADMAlert pCallback) {
    return 0;
  }

  virtual ULONG SetOMADMStateCallback(tFNOMADMState pCallback) {
    return 0;
  }

  void set_strict(bool value) {
    strict_ = value;
  }

 protected:
  bool strict_;
  static const char *kBadCall;
};

// Only expect one mock_gobi_sdk_wrapper.h per executable
const char *ErrorSdk::kBadCall = "Unexpected sdk call to: ";

// Just enough SDK to instantiate a modem object.
class BootstrapSdk : public ErrorSdk {
  virtual ULONG QCWWANConnect(
      CHAR *                     pDeviceNode,
      CHAR *                     pDeviceKey) {
    return 0;
  }

  virtual ULONG QCWWANDisconnect() {
    return 0;
  }

  virtual ULONG GetSerialNumbers(
      BYTE                       esnSize,
      CHAR *                     pESNString,
      BYTE                       imeiSize,
      CHAR *                     pIMEIString,
      BYTE                       meidSize,
      CHAR *                     pMEIDString) {
    strncpy(pESNString, "FFFFFF", esnSize);
    strncpy(pIMEIString, "980000000100000", imeiSize);
    strncpy(pMEIDString, "A1000000000000", meidSize);
    return 0;
  }
};

}  // namespace gobi
