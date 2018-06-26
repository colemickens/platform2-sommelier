/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This tool will attempt to mount or create the encrypted stateful partition,
 * and the various bind mountable subdirectories.
 *
 */
#define _FILE_OFFSET_BITS 64
#define CHROMEOS_ENVIRONMENT

#include <fcntl.h>
#include <sys/time.h>
#include <vboot/crossystem.h>
#include <vboot/tlcl.h>

#include <memory>
#include <string>
#include <vector>

#include <base/files/file_util.h>
#include <base/strings/string_number_conversions.h>

#include <metrics/metrics_library.h>

#include <cryptohome/cryptolib.h>
#include "cryptohome/mount_encrypted.h"
#include "cryptohome/mount_encrypted/encrypted_fs.h"
#include "cryptohome/mount_encrypted/encryption_key.h"
#include "cryptohome/mount_encrypted/tpm.h"
#include "cryptohome/mount_helpers.h"

#define PROP_SIZE 64

constexpr char kBioCryptoInitPath[] = "/usr/bin/bio_crypto_init";
constexpr char kBioTpmSeedSalt[] = "biod";
constexpr char kBioTpmSeedTmpDir[] = "/run/bio_crypto_init";
constexpr char kBioTpmSeedFile[] = "seed";
static const gchar* const kNvramExport = "/tmp/lockbox.nvram";
static const uid_t kBiodUid = 282;
static const gid_t kBiodGid = 282;

#if DEBUG_ENABLED
struct timeval tick = {};
struct timeval tick_start = {};
#endif

static const char kMountEncryptedMetricsPath[] = "/run/metrics.mount-encrypted";

namespace metrics {
const char kSystemKeyStatus[] = "Platform.MountEncrypted.SystemKeyStatus";
const char kEncryptionKeyStatus[] =
    "Platform.MountEncrypted.EncryptionKeyStatus";
}

static result_code get_system_property(const char* prop, char* buf,
                                       size_t length) {
  const char* rc;

  DEBUG("Fetching System Property '%s'", prop);
  rc = VbGetSystemPropertyString(prop, buf, length);
  DEBUG("Got System Property 'mainfw_type': %s", rc ? buf : "FAIL");

  return rc != NULL ? RESULT_SUCCESS : RESULT_FAIL_FATAL;
}

static int has_chromefw(void) {
  static int state = -1;
  char fw[PROP_SIZE];

  /* Cache the state so we don't have to perform the query again. */
  if (state != -1)
    return state;

  if (get_system_property("mainfw_type", fw, sizeof(fw)) != RESULT_SUCCESS)
    state = 0;
  else
    state = strcmp(fw, "nonchrome") != 0;
  return state;
}


// This triggers the live encryption key to be written to disk, encrypted by the
// system key. It is intended to be called by Cryptohome once the TPM is done
// being set up. If the system key is passed as an argument, use it, otherwise
// attempt to query the TPM again.
static result_code finalize_from_cmdline(
    const cryptohome::EncryptedFs& encrypted_fs,
    const base::FilePath& rootdir,
    char* key) {
  // Load the system key.
  brillo::SecureBlob system_key;
  if (!brillo::SecureBlob::HexStringToSecureBlob(std::string(key), &system_key)
      || system_key.size() != DIGEST_LENGTH) {
    LOG(ERROR) << "Failed to parse system key.";
    return RESULT_FAIL_FATAL;
  }

  FixedSystemKeyLoader loader(system_key);
  EncryptionKey key_manager(&loader, rootdir);
  result_code rc = key_manager.SetTpmSystemKey();
  if (rc != RESULT_SUCCESS) {
    return rc;
  }

  // If there already is an encrypted system key on disk, there is nothing to
  // do. This also covers cases where the system key is not derived from the
  // lockbox space contents (e.g. TPM 2.0 devices, TPM 1.2 devices with
  // encrypted stateful space, factory keys, etc.), for which it is not
  // appropriate to replace the system key. For cases where finalization is
  // unfinished, we clear any stale system keys from disk to make sure we pass
  // the check here.
  if (base::PathExists(key_manager.key_path())) {
    return RESULT_SUCCESS;
  }

  // Load the encryption key.
  brillo::SecureBlob encryption_key = encrypted_fs.GetKey();
  if (encryption_key.empty()) {
    ERROR("Could not get mount encryption key");
    return RESULT_FAIL_FATAL;
  }

  // Persist the encryption key to disk.
  key_manager.PersistEncryptionKey(encryption_key);

  return RESULT_SUCCESS;
}

static result_code report_info(const cryptohome::EncryptedFs& encrypted_fs,
                               const base::FilePath& rootdir) {
  Tpm tpm;

  printf("TPM: %s\n", tpm.available() ? "yes" : "no");
  if (tpm.available()) {
    bool owned = false;
    printf("TPM Owned: %s\n", tpm.IsOwned(&owned) == RESULT_SUCCESS
                                  ? (owned ? "yes" : "no")
                                  : "fail");
  }
  printf("ChromeOS: %s\n", has_chromefw() ? "yes" : "no");
  printf("TPM2: %s\n", tpm.is_tpm2() ? "yes" : "no");
  if (has_chromefw()) {
    brillo::SecureBlob system_key;
    auto loader = SystemKeyLoader::Create(&tpm, rootdir);
    result_code rc = loader->Load(&system_key);
    if (rc != RESULT_SUCCESS) {
      printf("NVRAM: missing.\n");
    } else {
      printf("NVRAM: available.\n");
    }
  } else {
    printf("NVRAM: not present\n");
  }
  // Report info from the encrypted mount.
  encrypted_fs.ReportInfo();

  return RESULT_SUCCESS;
}

/* Exports NVRAM contents to tmpfs for use by install attributes */
void nvram_export(const brillo::SecureBlob& contents) {
  int fd;
  DEBUG("Export NVRAM contents");
  fd = open(kNvramExport, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
  if (fd < 0) {
    perror("open(nvram_export)");
    return;
  }
  if (write(fd, contents.data(), contents.size()) != contents.size()) {
    /* Don't leave broken files around */
    unlink(kNvramExport);
  }
  close(fd);
}

template <typename Enum>
void RecordEnumeratedHistogram(MetricsLibrary* metrics,
                               const char* name,
                               Enum val) {
  metrics->SendEnumToUMA(name, static_cast<int>(val),
                         static_cast<int>(Enum::kCount));
}

/*
 * Send a secret derived from the system key to the biometric managers, if
 * available, via a tmpfs file which will be read by bio_crypto_init.
 */
bool SendSecretToBiodTmpFile(const EncryptionKey& key) {
  /* If there isn't a bio-sensor, don't bother. */
  if (!base::PathExists(base::FilePath(kBioCryptoInitPath))) {
    LOG(INFO) << "There is no biod, so skip sending TPM seed.";
    return true;
  }

  brillo::SecureBlob tpm_seed =
      key.GetDerivedSystemKey(std::string(kBioTpmSeedSalt));
  if (tpm_seed.empty()) {
    LOG(ERROR) << "TPM Seed provided is empty, not writing to tmpfs.";
    return false;
  }

  auto dirname = base::FilePath(kBioTpmSeedTmpDir);
  if (!base::CreateDirectory(dirname)) {
    LOG(ERROR) << "Failed to create dir for TPM seed.";
    return false;
  }

  if (chown(kBioTpmSeedTmpDir, kBiodUid, kBiodGid) == -1) {
    PLOG(ERROR) << "Failed to change ownership of biod tmpfs dir.";
    return false;
  }

  auto filename = dirname.Append(kBioTpmSeedFile);
  if (base::WriteFile(filename, tpm_seed.char_data(), tpm_seed.size()) !=
      tpm_seed.size()) {
    LOG(ERROR) << "Failed to write TPM seed to tmpfs file.";
    return false;
  }

  if (chown(filename.value().c_str(), kBiodUid, kBiodGid) == -1) {
    PLOG(ERROR) << "Failed to change ownership of biod tmpfs file.";
    return false;
  }

  return true;
}

int main(int argc, char* argv[]) {
  result_code rc;
  base::FilePath rootdir = base::FilePath(getenv("MOUNT_ENCRYPTED_ROOT"));
  cryptohome::Platform platform;
  cryptohome::EncryptedFs encrypted_fs(rootdir, &platform);

  MetricsLibrary metrics;
  metrics.Init();
  metrics.SetOutputFile(kMountEncryptedMetricsPath);

  INFO_INIT("Starting.");

  bool use_factory_system_key = false;
  if (argc > 1) {
    if (!strcmp(argv[1], "umount")) {
      return encrypted_fs.Teardown();
    } else if (!strcmp(argv[1], "info")) {
      // Report info from the encrypted mount.
      return report_info(encrypted_fs, rootdir);
    } else if (!strcmp(argv[1], "finalize")) {
      return finalize_from_cmdline(encrypted_fs, rootdir,
                                   argc > 2 ? argv[2] : NULL);
    } else if (!strcmp(argv[1], "factory")) {
      use_factory_system_key = true;
    } else {
      fprintf(stderr, "Usage: %s [info|finalize|umount|factory]\n", argv[0]);
      return RESULT_FAIL_FATAL;
    }
  }

  /* For the mount operation at boot, return RESULT_FAIL_FATAL to trigger
   * chromeos_startup do the stateful wipe.
   */
  rc = encrypted_fs.CheckStates();
  if (rc != RESULT_SUCCESS)
    return rc;

  Tpm tpm;
  auto loader = SystemKeyLoader::Create(&tpm, rootdir);
  EncryptionKey key(loader.get(), rootdir);
  if (use_factory_system_key) {
    rc = key.SetFactorySystemKey();
  } else if (has_chromefw()) {
    rc = key.LoadChromeOSSystemKey();
  } else {
    rc = key.SetInsecureFallbackSystemKey();
  }
  RecordEnumeratedHistogram(&metrics, metrics::kSystemKeyStatus,
                            key.system_key_status());
  if (rc != RESULT_SUCCESS) {
    return rc;
  }

  rc = key.LoadEncryptionKey();
  RecordEnumeratedHistogram(&metrics, metrics::kEncryptionKeyStatus,
                            key.encryption_key_status());
  if (rc != RESULT_SUCCESS) {
    return rc;
  }

  /* Log errors during sending seed to biod, but don't stop execution. */
  if (!use_factory_system_key && has_chromefw()) {
    if (!SendSecretToBiodTmpFile(key)) {
      LOG(ERROR) << "Failed to send TPM secret to biod.";
    }
  } else {
    LOG(ERROR) << "Failed to load system key, biod won't get a TPM seed.";
  }

  rc = encrypted_fs.Setup(key.encryption_key(), key.is_fresh());
  if (rc == RESULT_SUCCESS) {
    bool lockbox_valid = false;
    if (loader->CheckLockbox(&lockbox_valid) == RESULT_SUCCESS) {
      NvramSpace* lockbox_space = tpm.GetLockboxSpace();
      if (lockbox_valid && lockbox_space->is_valid()) {
        LOG(INFO) << "Lockbox is valid, exporting.";
        nvram_export(lockbox_space->contents());
      }
    } else {
      LOG(ERROR) << "Lockbox validity check error.";
    }
  }

  INFO_DONE("Done.");

  /* Continue boot. */
  return rc;
}
