// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* For background on this, see:
 * https://gerrit.chromium.org/gerrit/43885
 * https://crosbug.com/39422
 *
 * Short answer: `xauth` pulls in a lot of legacy libs we don't care about.
 */

#include <arpa/inet.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if __GLIBC_MINOR__ >= 16
/* New to glibc-2.16 */
#include <sys/auxv.h>
#else
/* Roll our own as long as we have glibc-2.15 */
#include <elf.h>
#include <link.h>
static unsigned long getauxval(unsigned long type)
{
  unsigned long val = 0;
  ElfW(auxv_t) auxv;
  FILE *fp = fopen("/proc/self/auxv", "r");
  while (fread(&auxv, sizeof(auxv), 1, fp))
    if (auxv.a_type == type) {
      val = auxv.a_un.a_val;
      break;
    }
  fclose(fp);
  return val;
}
#endif

/* New to glibc-2.16 / gcc-4.7 */
#ifndef static_assert
# define static_assert(cond, msg) \
  do { \
    (void)(sizeof(char[1 - 2 * !(cond)])); \
  } while (0)
#endif

static bool writeu16(FILE *fp, uint16_t val)
{
  uint16_t tmp = htons(val);
  return fwrite(&tmp, sizeof(tmp), 1, fp) == 1 ? true : false;
}

static bool writelv(FILE *fp, uint16_t len, const char *val)
{
  if (!writeu16(fp, len) ||
      fwrite(val, len, 1, fp) != 1)
    return false;
  return true;
}

static bool writestrlv(FILE *fp, const char *str)
{
  return writelv(fp, (uint16_t)strlen(str), str);
}

/*
 * cros-xauth - ChromeOS MIT-MAGIC-COOKIE-1 generator
 *
 * Usage: cros-xauth <Xauthority file>
 *
 * Outputs an xauth cookie equivalent to:
 *   $ xauth -q -f .Xauthority add :0 . $(mcookie)
 */
int main(int argc, char *argv[])
{
  static uint16_t family = 0x100;
  static const char address[] = "localhost";
  static const char number[] = "0";
  static const char name[] = "MIT-MAGIC-COOKIE-1";
  char cookie[16];

  /* Sanity in an insane world! */
  umask(0022);

  /* The kernel gives us a pointer to 16 bytes of random data via
   * AT_RANDOM.  Suck it up as our cookie if possible.
   */
  void *at_random = (void *)(uintptr_t)getauxval(AT_RANDOM);
  if (!at_random) {
    /* Rely on ASLR to give us at least 32 bits of randomness,
     * and we'll let garbage on the stack do the rest.
     */
    uintptr_t addr = (uintptr_t)&main;
    memcpy(cookie, &addr, sizeof(addr));
    static_assert(sizeof(addr) <= sizeof(cookie), "cookie/pointer mismatch");
  } else {
    memcpy(cookie, at_random, 16);
    static_assert(16 <= sizeof(cookie), "cookie too small for AT_RANDOM");
  }

  FILE *fp = fopen(argv[1], "wb");
  if (fp == NULL ||
      !writeu16(fp, family) ||
      !writestrlv(fp, address) ||
      !writestrlv(fp, number) ||
      !writestrlv(fp, name) ||
      !writelv(fp, sizeof(cookie), cookie)) {
    perror(argv[0]);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
