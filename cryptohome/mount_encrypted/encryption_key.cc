// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mount_encrypted/encryption_key.h"

#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <glib.h>

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <base/strings/string_number_conversions.h>

#include "cryptohome/mount_encrypted.h"
#include "cryptohome/mount_encrypted/tpm.h"
#include "cryptohome/mount_helpers.h"

#define STATEFUL_MNT "mnt/stateful_partition"

namespace {

const gchar* const kKernelCmdline = "/proc/cmdline";
const gchar* const kKernelCmdlineOption = " encrypted-stateful-key=";

const char* const kStaticKeyDefault = "default unsafe static key";
const char* const kStaticKeyFactory = "factory unsafe static key";
const char* const kStaticKeyFinalizationNeeded = "needs finalization";

char* keyfile_read(const char* keyfile, uint8_t* system_key) {
  char* key = NULL;
  unsigned char* cipher = NULL;
  gsize length;
  uint8_t* plain = NULL;
  int plain_length, final_len;
  GError* error = NULL;
  EVP_CIPHER_CTX ctx;
  const EVP_CIPHER* algo = EVP_aes_256_cbc();

  DEBUG("Reading keyfile %s", keyfile);
  if (EVP_CIPHER_key_length(algo) != DIGEST_LENGTH) {
    ERROR("cipher key size mismatch (got %d, want %d)",
          EVP_CIPHER_key_length(algo), DIGEST_LENGTH);
    goto out;
  }

  if (access(keyfile, R_OK)) {
    /* This file being missing is handled in caller, so
     * do not emit error message.
     */
    INFO("%s does not exist.", keyfile);
    goto out;
  }

  if (!g_file_get_contents(keyfile, reinterpret_cast<gchar**>(&cipher), &length,
                           &error)) {
    ERROR("Unable to read %s: %s", keyfile, error->message);
    g_error_free(error);
    goto out;
  }
  plain =
      reinterpret_cast<uint8_t*>(malloc(length + EVP_CIPHER_block_size(algo)));
  if (!plain) {
    PERROR("malloc");
    goto free_cipher;
  }

  DEBUG("Decrypting keyfile %s", keyfile);
  /* Use the default IV. */
  if (!EVP_DecryptInit(&ctx, algo, system_key, NULL)) {
    SSL_ERROR("EVP_DecryptInit");
    goto free_plain;
  }
  if (!EVP_DecryptUpdate(&ctx, plain, &plain_length, cipher, length)) {
    SSL_ERROR("EVP_DecryptUpdate");
    goto free_ctx;
  }
  if (!EVP_DecryptFinal(&ctx, plain + plain_length, &final_len)) {
    SSL_ERROR("EVP_DecryptFinal");
    goto free_ctx;
  }
  plain_length += final_len;

  if (plain_length != DIGEST_LENGTH) {
    ERROR("Decrypted encryption key length (%d) is not %d.", plain_length,
          DIGEST_LENGTH);
    goto free_ctx;
  }

  debug_dump_hex("encryption key", plain, DIGEST_LENGTH);

  key = stringify_hex(plain, DIGEST_LENGTH);

free_ctx:
  EVP_CIPHER_CTX_cleanup(&ctx);
free_plain:
  free(plain);
free_cipher:
  g_free(cipher);
out:
  DEBUG("key:%p", key);
  return key;
}

/* Pushes a previously written file out to disk.  Returns 1 on success and 0 if
 * it cannot be guaranteed that the file has hit the disk.
 */
int file_sync_full(const char* filename) {
  int fd, sync_result, close_result;
  int retval = 0;
  char* dirname = g_path_get_dirname(reinterpret_cast<const gchar*>(filename));

  /* sync file */
  fd = TEMP_FAILURE_RETRY(open(filename, O_WRONLY | O_CLOEXEC));
  if (fd < 0) {
    ERROR("Could not open %s for syncing", filename);
    goto free_dirname;
  }
  sync_result = TEMP_FAILURE_RETRY(fdatasync(fd));
  close_result = close(fd);  // close() may not be retried.
  if (sync_result < 0) {
    ERROR("Failed to sync %s", filename);
    goto free_dirname;
  }
  if (close_result < 0) {
    ERROR("Failed to close %s", filename);
    goto free_dirname;
  }

  /* sync directory */
  fd = TEMP_FAILURE_RETRY(open(dirname, O_RDONLY | O_CLOEXEC));
  if (fd < 0) {
    ERROR("Could not open directory %s for syncing", dirname);
    goto free_dirname;
  }
  sync_result = TEMP_FAILURE_RETRY(fsync(fd));
  close_result = close(fd);  // close() may not be retried.
  if (sync_result < 0) {
    ERROR("Failed to sync %s", filename);
    goto free_dirname;
  }
  if (close_result < 0) {
    ERROR("Failed to close %s", filename);
    goto free_dirname;
  }
  retval = 1;

free_dirname:
  g_free(dirname);
  return retval;
}

int keyfile_write(const char* keyfile, uint8_t* system_key, char* string) {
  int rc = 0;
  size_t length;
  uint8_t plain[DIGEST_LENGTH];
  uint8_t* cipher = NULL;
  int cipher_length, final_len, tries, result;
  GError* error = NULL;
  EVP_CIPHER_CTX ctx;
  const EVP_CIPHER* algo = EVP_aes_256_cbc();
  mode_t mask;

  DEBUG("Staring to process keyfile %s", keyfile);
  /* Have key file be read/write only by root user. */
  mask = umask(0077);

  if (EVP_CIPHER_key_length(algo) != DIGEST_LENGTH) {
    ERROR("cipher key size mismatch (got %d, want %d)",
          EVP_CIPHER_key_length(algo), DIGEST_LENGTH);
    goto out;
  }

  if (access(keyfile, R_OK) == 0) {
    ERROR("%s already exists.", keyfile);
    goto out;
  }

  length = strlen(string);
  if (length != sizeof(plain) * 2) {
    ERROR("Encryption key string length (%zu) is not %zu.", length,
          sizeof(plain) * 2);
    goto out;
  }

  length = sizeof(plain);
  if (!hexify_string(string, plain, length)) {
    ERROR("Failed to convert encryption key to byte array");
    goto out;
  }

  debug_dump_hex("encryption key", plain, DIGEST_LENGTH);

  cipher =
      reinterpret_cast<uint8_t*>(malloc(length + EVP_CIPHER_block_size(algo)));
  if (!cipher) {
    PERROR("malloc");
    goto out;
  }

  DEBUG("Encrypting keyfile %s", keyfile);
  /* Use the default IV. */
  if (!EVP_EncryptInit(&ctx, algo, system_key, NULL)) {
    SSL_ERROR("EVP_EncryptInit");
    goto free_cipher;
  }
  if (!EVP_EncryptUpdate(&ctx, cipher, &cipher_length, (unsigned char*)plain,
                         length)) {
    SSL_ERROR("EVP_EncryptUpdate");
    goto free_ctx;
  }
  if (!EVP_EncryptFinal(&ctx, cipher + cipher_length, &final_len)) {
    SSL_ERROR("EVP_EncryptFinal");
    goto free_ctx;
  }
  length = cipher_length + final_len;

  DEBUG("Writing %zu bytes to %s", length, keyfile);
  /* TODO(keescook): use fd here, and set secure delete. Unsupported
   * by ext4 currently. :(
   *    int f;
   *    ioctl(fd, EXT2_IOC_GETFLAGS, &f);
   *    f |= EXT2_SECRM_FL;
   *    ioctl(fd, EXT2_IOC_SETFLAGS, &f);
   */
  /* Note that the combination of g_file_set_contents() and
   * file_sync_full() isn't atomic, but the time window of non-atomicity
   * should be pretty small.
   */
  tries = 3;
  while (tries > 0 &&
         !(result = g_file_set_contents(
               keyfile, reinterpret_cast<gchar*>(cipher), length, &error))) {
    ERROR("Unable to write %s: %s", keyfile, error->message);
    g_error_free(error);
    --tries;
  }
  if (!result)
    goto free_ctx;

  tries = 3;
  while (tries > 0 && !(result = file_sync_full(keyfile))) {
    ERROR("Unable to sync %s.", keyfile);
    --tries;
  }
  if (!result)
    goto free_ctx;

  rc = 1;

free_ctx:
  EVP_CIPHER_CTX_cleanup(&ctx);
free_cipher:
  free(cipher);
out:
  umask(mask);
  DEBUG("keyfile write rc:%d", rc);
  return rc;
}

}  // namespace

static void sha256(const char* string, uint8_t* digest) {
  SHA256((unsigned char*)string, strlen(string), digest);
}

EncryptionKey::EncryptionKey(const char* rootdir) {
  if (asprintf(&key_path, "%s/%s", rootdir, STATEFUL_MNT "/encrypted.key") ==
      -1)
    goto fail;
  if (asprintf(&needs_finalization_path, "%s/%s", rootdir,
               STATEFUL_MNT "/encrypted.needs-finalization") == -1)
    goto fail;

  return;

fail:
  perror("asprintf");
  exit(1);
}

EncryptionKey::~EncryptionKey() {
  free(encryption_key);
  free(key_path);
  free(needs_finalization_path);
}

/* Extract the desired system key from the kernel's boot command line. */
static result_code get_key_from_cmdline(uint8_t* digest) {
  result_code rc = RESULT_FAIL_FATAL;
  gchar* buffer;
  gsize length;
  char *cmdline, *option_end;
  /* Option name without the leading space. */
  const gchar* option = kKernelCmdlineOption + 1;

  if (!g_file_get_contents(kKernelCmdline, &buffer, &length, NULL)) {
    PERROR("%s", kKernelCmdline);
    return RESULT_FAIL_FATAL;
  }

  /* Find a string match either at start of string or following
   * a space.
   */
  cmdline = buffer;
  if (strncmp(cmdline, option, strlen(option)) == 0 ||
      (cmdline = strstr(cmdline, kKernelCmdlineOption))) {
    /* The "=" exists because it is in kKernelCmdlineOption. */
    cmdline = strstr(cmdline, "=");
    /* strchrnul() cannot return NULL. */
    option_end = strchrnul(cmdline, ' ');
    *option_end = '\0';
    sha256(cmdline, digest);
    debug_dump_hex("system key", digest, DIGEST_LENGTH);
    rc = RESULT_SUCCESS;
  }

  g_free(buffer);
  return rc;
}

/* Find the system key used for decrypting the stored encryption key.
 * ChromeOS devices are required to use the NVRAM area, all the rest will
 * fallback through various places (kernel command line, BIOS UUID, and
 * finally a static value) for a system key.
 */
result_code EncryptionKey::FindSystemKey(int mode, bool has_chromefw) {
  gchar* key;
  gsize length;

  /* By default, do not allow migration. */
  migration_allowed_ = 0;

  /* Factory mode uses a static system key. */
  if (mode == kModeFactory) {
    INFO("Using factory insecure system key.");
    sha256(kStaticKeyFactory, digest);
    debug_dump_hex("system key", digest, DIGEST_LENGTH);
    return RESULT_SUCCESS;
  }

  /* Force ChromeOS devices into requiring the system key come from
   * NVRAM.
   */
  if (has_chromefw) {
    result_code rc;
    rc = get_nvram_key(digest, &migration_allowed_);

    if (rc == RESULT_SUCCESS) {
      INFO("Using NVRAM as system key; already populated%s.",
           migration_allowed_ ? " (legacy)" : "");
    } else {
      INFO("Using NVRAM as system key; finalization needed.");
    }
    return rc;
  }

  if (get_key_from_cmdline(digest) == RESULT_SUCCESS) {
    INFO("Using kernel command line argument as system key.");
    return RESULT_SUCCESS;
  }
  if (g_file_get_contents("/sys/class/dmi/id/product_uuid", &key, &length,
                          NULL)) {
    sha256(key, digest);
    debug_dump_hex("system key", digest, DIGEST_LENGTH);
    g_free(key);
    INFO("Using UUID as system key.");
    return RESULT_SUCCESS;
  }

  INFO("Using default insecure system key.");
  sha256(kStaticKeyDefault, digest);
  debug_dump_hex("system key", digest, DIGEST_LENGTH);
  return RESULT_SUCCESS;
}

char* EncryptionKey::GenerateFreshEncryptionKey() {
  unsigned char rand_bytes[DIGEST_LENGTH];
  unsigned char digest[DIGEST_LENGTH];

  if (get_random_bytes(rand_bytes, sizeof(rand_bytes)) != RESULT_SUCCESS)
    ERROR("No entropy source found -- using uninitialized stack");

  SHA256(rand_bytes, DIGEST_LENGTH, digest);
  debug_dump_hex("encryption key", digest, DIGEST_LENGTH);

  return stringify_hex(digest, DIGEST_LENGTH);
}

void EncryptionKey::Finalized() {
  /* TODO(keescook): once ext4 supports secure delete, just unlink. */
  if (access(needs_finalization_path, R_OK) == 0) {
    /* This is nearly useless on SSDs. */
    shred(needs_finalization_path);
    unlink(needs_finalization_path);
  }
}

bool EncryptionKey::DoFinalize() {
  INFO("Writing keyfile %s.", key_path);
  if (!keyfile_write(key_path, digest, encryption_key)) {
    ERROR("Failed to write %s -- aborting.", key_path);
    return false;
  }

  Finalized();
  did_finalize_ = true;
  return true;
}

void EncryptionKey::NeedsFinalization() {
  uint8_t useless_key[DIGEST_LENGTH];
  sha256(kStaticKeyFinalizationNeeded, useless_key);

  INFO("Writing finalization intent %s.", needs_finalization_path);
  if (!keyfile_write(needs_finalization_path, useless_key, encryption_key)) {
    ERROR("Failed to write %s -- aborting.", needs_finalization_path);
    return;
  }
}

result_code EncryptionKey::SetSystemKey(const brillo::SecureBlob& system_key) {
  if (system_key.size() != DIGEST_LENGTH) {
    LOG(ERROR) << "Invalid key length.";
    return RESULT_FAIL_FATAL;
  }

  memcpy(digest, system_key.data(), DIGEST_LENGTH);
  has_system_key = true;
  return RESULT_SUCCESS;
}

result_code EncryptionKey::LoadEncryptionKey(int mode, bool has_chromefw) {
  has_system_key = FindSystemKey(mode, has_chromefw) == RESULT_SUCCESS;
  if (has_system_key) {
    encryption_key = keyfile_read(key_path, digest);
  } else {
    INFO("No usable system key found.");
    unlink(key_path);
  }

  if (encryption_key) {
    /* If we found a stored encryption key, we've already
     * finished a complete login and Cryptohome Finalize
     * so migration is finished.
     */
    migration_allowed_ = 0;
    valid_keyfile = 1;
  } else {
    uint8_t useless_key[DIGEST_LENGTH];
    sha256(kStaticKeyFinalizationNeeded, useless_key);
    encryption_key = keyfile_read(needs_finalization_path, useless_key);
    if (!encryption_key) {
      /* This is a brand new system with no keys. */
      INFO("Generating new encryption key.");
      encryption_key = GenerateFreshEncryptionKey();
      if (!encryption_key)
        return RESULT_FAIL_FATAL;
      rebuild = 1;
    } else {
      ERROR(
          "Finalization unfinished! "
          "Encryption key still on disk!");
    }
  }

  return RESULT_SUCCESS;
}

void EncryptionKey::SetEncryptionKey(const brillo::SecureBlob& encryption_key) {
  this->encryption_key = stringify_hex(
      const_cast<uint8_t*>(encryption_key.data()), encryption_key.size());
}

void EncryptionKey::Persist() {
  /* When we are creating the encrypted mount for the first time,
   * either finalize immediately, or write the encryption key to
   * disk (*sigh*) to handle the seemingly endless broken or
   * wedged TPM states.
   */
  if (rebuild) {
    unlink(key_path);

    /* Devices that already have the NVRAM area populated and
     * are being rebuilt don't need to wait for Cryptohome
     * because the NVRAM area isn't going to change.
     *
     * Devices that do not have the NVRAM area populated
     * may potentially never have the NVRAM area populated,
     * which means we have to write the encryption key to
     * disk until we finalize. Once secure deletion is
     * supported on ext4, this won't be as horrible.
     */
    if (has_system_key)
      DoFinalize();
    else
      NeedsFinalization();
  } else {
    /* If we're not rebuilding and we have a sane system
     * key, then we must either need finalization (if we
     * failed to finalize in Cryptohome), or we have already
     * finalized, but maybe failed to clean up.
     */
    if (has_system_key) {
      if (!valid_keyfile)
        DoFinalize();
      else
        Finalized();
    }
  }
}
