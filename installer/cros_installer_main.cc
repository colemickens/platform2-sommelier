// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <string>

#include <base/string_tokenizer.h>
#include <gflags/gflags.h>

using std::string;

DEFINE_string(setgoodkernel, "", "Mark current kernel partition as good");
DEFINE_string(recovery, "", "Install OS from recovery installer");
DEFINE_string(postinst, "", "Run OS postinstall steps");
DEFINE_string(install, "", "Install ChromeOS");
DEFINE_string(findrootfs, "", "Find rootfs partition");
DEFINE_string(setimage, "", "Set the bootable image partition");
DEFINE_string(getimage, "", "Get the bootable image partition");
DEFINE_string(updatefirmware, "", "Update the firmware");

// This is a place holder to invoke the backing scripts. Once all scripts have
// been rewritten as library calls this command should be deleted.
int RunCommand(const string& filename, const string& cmdoptions) {
  int err = -1;
  int j = 0, i = 1;
  const int kCmdArgs = 8;
  char **argv = new char* [kCmdArgs+2];
  FILE *fp;

  string sbinfn = "/usr/sbin/" + filename;
  string pwdfn = "./" + filename;

  CStringTokenizer t(cmdoptions.c_str(), cmdoptions.c_str() +
                     strlen(cmdoptions.c_str()), " ");
  while (t.GetNext()) {
    if (i > kCmdArgs) {
      printf("Too many cmdline args\n");
      err = -1;
      goto cleanup;
    }
    argv[i++] = strdup(t.token().c_str());
  }
  argv[i] = NULL;

  // Temporary workaround to develop on your host chroot system
  // See if the file exists
  fp = fopen(sbinfn.c_str(), "r");
  if (fp) {
    argv[0] = strdup(sbinfn.c_str());
    err = execv(sbinfn.c_str(), argv);
    fclose(fp);
  } else {
    argv[0] = strdup(pwdfn.c_str());
    err = execv(pwdfn.c_str(), argv);
  }

cleanup:
  for (j = 0; j < i; j++) {
    free((char*)argv[j]);
  }
  delete [] argv;
  return err;
}

int main(int argc, char** argv) {
  string usage("The Chromium/ChromeOS installer");

  google::SetUsageMessage(usage);
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::CommandLineFlagInfo cmdinfo;
  string cmd, cmdoptions;

  if (FLAGS_setgoodkernel.empty() == false) {
    printf("Marking current partition as known good\n");
    cmd = "chromeos-setgoodkernel";
    cmdoptions = FLAGS_setgoodkernel;
  } else if (FLAGS_recovery.empty() == false) {
    printf("recovery is set\n");
    cmd = "chromeos-recovery";
    cmdoptions = FLAGS_recovery;
  } else if (FLAGS_postinst.empty() == false) {
    printf("postinst is set\n");
    cmd = "chromeos-postinst";
    cmdoptions = FLAGS_postinst;
  } else if (FLAGS_install.empty() == false) {
    printf("install is set\n");
    cmd = "chromeos-install";
    cmdoptions = FLAGS_install;
  } else if (FLAGS_findrootfs.empty() == false) {
    printf("findrootfs is set\n");
    cmd = "chromeos-findrootfs";
    cmdoptions = FLAGS_findrootfs;
  } else if (FLAGS_setimage.empty() == false) {
    printf("setimage is set\n");
    cmd = "chromeos-setimage";
    cmdoptions = FLAGS_setimage;
  } else if (FLAGS_updatefirmware.empty() == false) {
    printf("updatefirmware is set\n");
    cmd = "chromeos-firmwareupdate";
    cmdoptions = FLAGS_updatefirmware;
  }

  RunCommand(cmd, cmdoptions);

  return 0;
}
