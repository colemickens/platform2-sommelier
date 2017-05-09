// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_CRYPTOHOME_METRICS_H_
#define CRYPTOHOME_CRYPTOHOME_METRICS_H_

#include "cryptohome/tpm.h"

namespace cryptohome {

enum CryptohomeError {
  kTpmFail = 1,
  kTcsKeyLoadFailed = 2,
  kTpmDefendLockRunning = 3,
  kDecryptAttemptButTpmKeyMissing = 4,
  kDecryptAttemptButTpmNotOwned = 5,
  kDecryptAttemptButTpmNotAvailable = 6,
  kDecryptAttemptButTpmKeyMismatch = 7,
  kDecryptAttemptWithTpmKeyFailed = 8,
  kCannotLoadTpmSrk = 9,
  kCannotReadTpmSrkPublic = 10,
  kCannotLoadTpmKey = 11,
  kCannotReadTpmPublicKey = 12,
  kTpmBadKeyProperty = 13,
  kLoadPkcs11TokenFailed = 14,
  kEncryptWithTpmFailed = 15,
  kTssCommunicationFailure = 16,
  kTssInvalidHandle = 17,
  kBothTpmAndScryptWrappedKeyset = 18,
  kCryptohomeErrorNumBuckets,  // Must be the last entry.
};

enum TimerType {
  kAsyncMountTimer,
  kSyncMountTimer,
  kAsyncGuestMountTimer,
  kSyncGuestMountTimer,
  kTpmTakeOwnershipTimer,
  kPkcs11InitTimer,
  kMountExTimer,
  kDircryptoMigrationTimer,
  kNumTimerTypes  // For the number of timer types.
};

enum DictionaryAttackResetStatus {
  kResetNotNecessary,
  kResetAttemptSucceeded,
  kResetAttemptFailed,
  kDelegateNotAllowed,
  kDelegateNotAvailable,
  kCounterQueryFailed,
  kDictionaryAttackResetStatusNumBuckets
};

enum ChecksumStatus {
  kChecksumOK,
  kChecksumDoesNotExist,
  kChecksumReadError,
  kChecksumMismatch,
  kChecksumOutOfSync,
  kChecksumStatusNumBuckets
};

enum DircryptoMigrationStartStatus {
  kMigrationStarted = 1,
  kMigrationResumed = 2,
  kMigrationStartStatusNumBuckets
};

enum DircryptoMigrationEndStatus {
  kNewMigrationFailedGeneric = 1,
  kNewMigrationFinished = 2,
  kResumedMigrationFailedGeneric = 3,
  kResumedMigrationFinished = 4,
  kNewMigrationFailedLowDiskSpace = 5,
  kResumedMigrationFailedLowDiskSpace = 6,
  // The detail of the "FileError" failures (the failed file operation,
  // error code, and the rough classification of the failed path) will be
  // reported in separate metrics, too. Since there's no good way to relate the
  // multi-dimensional metric however, we treat some combinations as special
  // cases and distinguish them here as well.
  kNewMigrationFailedFileError = 7,
  kResumedMigrationFailedFileError = 8,
  kNewMigrationFailedFileErrorOpenEIO = 9,
  kResumedMigrationFailedFileErrorOpenEIO = 10,
  kMigrationEndStatusNumBuckets
};

enum DircryptoMigrationFailedOperationType {
  kMigrationFailedAtOtherOperation = 1,
  kMigrationFailedAtOpenSourceFile = 2,
  kMigrationFailedAtOpenDestinationFile = 3,
  kMigrationFailedOperationTypeNumBuckets
};

enum DircryptoMigrationFailedPathType {
  kMigrationFailedUnderOther = 1,
  kMigrationFailedUnderAndroidOther = 2,
  kMigrationFailedUnderAndroidCache = 3,
  kMigrationFailedUnderDownloads = 4,
  kMigrationFailedUnderCache = 5,
  kMigrationFailedUnderGcache = 6,
  kMigrationFailedPathTypeNumBuckets
};

enum class HomedirEncryptionType {
  kEcryptfs = 1,
  kDircrypto = 2,
  kHomedirEncryptionTypeNumBuckets
};

// Cros events emitted by cryptohome.
const char kAttestationOriginSpecificIdentifiersExhausted[] =
    "Attestation.OriginSpecificExhausted";

// Initializes cryptohome metrics. If this is not called, all calls to Report*
// will have no effect.
void InitializeMetrics();

// Cleans up and returns cryptohome metrics to an uninitialized state.
void TearDownMetrics();

// The |error| value is reported to the "Cryptohome.Errors" enum histogram.
void ReportCryptohomeError(CryptohomeError error);

// The |result| value is reported to the "Cryptohome.TpmResults" enum histogram.
void ReportTpmResult(TpmReturnCode result);

// Cros events are translated to an enum and reported to the generic
// "Platform.CrOSEvent" enum histogram. The |event| string must be registered in
// metrics/metrics_library.cc:kCrosEventNames.
void ReportCrosEvent(const char* event);

// Starts a timer for the given |timer_type|.
void ReportTimerStart(TimerType timer_type);

// Stops a timer and reports in milliseconds. Timers are reported to the
// "Cryptohome.TimeTo*" histograms.
void ReportTimerStop(TimerType timer_type);

// Reports a status value on the "Platform.TPM.DictionaryAttackResetStatus"
// histogram.
void ReportDictionaryAttackResetStatus(DictionaryAttackResetStatus status);

// Reports a dictionary attack counter value to the
// "Platform.TPM.DictionaryAttackCounter" histogram.
void ReportDictionaryAttackCounter(int counter);

void ReportChecksum(ChecksumStatus status);

// Reports removed GCache size by cryptohome to the
// "Cryptohome.FreedGCacheDiskSpaceInMb" histogram.
void ReportFreedGCacheDiskSpaceInMb(int mb);

// The |status| value is reported to the
// "Cryptohome.DircryptoMigrationStartStatus"
// enum histogram.
void ReportDircryptoMigrationStartStatus(DircryptoMigrationStartStatus status);

// The |status| value is reported to the
// "Cryptohome.DircryptoMigrationEndStatus"
// enum histogram.
void ReportDircryptoMigrationEndStatus(DircryptoMigrationEndStatus status);

// The |type| value is reported to the "Cryptohome.HomedirEncryptionType" enum
// histogram.
void ReportHomedirEncryptionType(HomedirEncryptionType type);

// Initialization helper.
class ScopedMetricsInitializer {
 public:
  ScopedMetricsInitializer() { InitializeMetrics(); }
  ~ScopedMetricsInitializer() { TearDownMetrics(); }
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_CRYPTOHOME_METRICS_H_
