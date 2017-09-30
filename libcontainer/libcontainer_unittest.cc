// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <signal.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "libcontainer/cgroup.h"
#include "libcontainer/config.h"
#include "libcontainer/container.h"
#include "libcontainer/libcontainer.h"
#include "libcontainer/libcontainer_util.h"

namespace libcontainer {

namespace {

constexpr pid_t kInitTestPid = 5555;
constexpr int kTestCpuShares = 200;
constexpr int kTestCpuQuota = 20000;
constexpr int kTestCpuPeriod = 50000;

struct MockPosixState {
  struct MountArgs {
    std::string source;
    base::FilePath target;
    std::string filesystemtype;
    unsigned long mountflags;
    const void* data;
    bool outside_mount;
  };
  std::vector<MountArgs> mount_args;

  dev_t stat_rdev_ret = makedev(2, 3);

  std::vector<int> kill_sigs;
  base::FilePath mkdtemp_root;
};
MockPosixState* g_mock_posix_state = nullptr;

struct MockCgroupState {
  struct AddedDevice {
    bool allow;
    int major;
    int minor;
    bool read;
    bool write;
    bool modify;
    char type;
  };

  int freeze_ret;
  int thaw_ret;
  int deny_all_devs_ret;
  int add_device_ret;
  int set_cpu_ret;

  int init_called_count;
  int deny_all_devs_called_count;

  std::vector<AddedDevice> added_devices;

  int set_cpu_shares_count;
  int set_cpu_quota_count;
  int set_cpu_period_count;
  int set_cpu_rt_runtime_count;
  int set_cpu_rt_period_count;
};
MockCgroupState* g_mock_cgroup_state = nullptr;

class MockCgroup : public libcontainer::Cgroup {
 public:
  explicit MockCgroup(MockCgroupState* state) : state_(state) {}
  ~MockCgroup() = default;

  static std::unique_ptr<libcontainer::Cgroup> Create(
      base::StringPiece name,
      const base::FilePath& cgroup_root,
      const base::FilePath& cgroup_parent,
      uid_t cgroup_owner,
      gid_t cgroup_group) {
    return std::make_unique<MockCgroup>(g_mock_cgroup_state);
  }

  int Freeze() override { return state_->freeze_ret; }

  int Thaw() override { return state_->thaw_ret; }

  int DenyAllDevices() override {
    ++state_->deny_all_devs_called_count;
    return state_->deny_all_devs_ret;
  }

  int AddDevice(bool allow,
                int major,
                int minor,
                bool read,
                bool write,
                bool modify,
                char type) override {
    state_->added_devices.emplace_back(MockCgroupState::AddedDevice{
        allow, major, minor, read, write, modify, type});
    return state_->add_device_ret;
  }

  int SetCpuShares(int shares) override {
    state_->set_cpu_shares_count++;
    return state_->set_cpu_ret;
  }

  int SetCpuQuota(int quota) override {
    state_->set_cpu_quota_count++;
    return state_->set_cpu_ret;
  }

  int SetCpuPeriod(int period) override {
    state_->set_cpu_period_count++;
    return state_->set_cpu_ret;
  }

  int SetCpuRtRuntime(int rt_runtime) override {
    state_->set_cpu_rt_runtime_count++;
    return state_->set_cpu_ret;
  }

  int SetCpuRtPeriod(int rt_period) override {
    state_->set_cpu_rt_period_count++;
    return state_->set_cpu_ret;
  }

 private:
  MockCgroupState* const state_;

  DISALLOW_COPY_AND_ASSIGN(MockCgroup);
};

const char* minijail_alt_syscall_table;
int minijail_ipc_called;
int minijail_vfs_called;
int minijail_net_called;
int minijail_pids_called;
int minijail_run_as_init_called;
int minijail_user_called;
int minijail_cgroups_called;
int minijail_wait_called;
int minijail_reset_signal_mask_called;

}  // namespace

TEST(LibcontainerTest, PremountedRunfs) {
  char premounted_runfs[] = "/tmp/cgtest_run/root";
  struct container_config* config = container_config_create();
  ASSERT_NE(nullptr, config);

  container_config_premounted_runfs(config, premounted_runfs);
  const char* result = container_config_get_premounted_runfs(config);
  ASSERT_EQ(0, strcmp(result, premounted_runfs));

  container_config_destroy(config);
}

TEST(LibcontainerTest, PidFilePath) {
  char pid_file_path[] = "/tmp/cgtest_run/root/container.pid";
  struct container_config* config = container_config_create();
  ASSERT_NE(nullptr, config);

  container_config_pid_file(config, pid_file_path);
  const char* result = container_config_get_pid_file(config);
  ASSERT_EQ(0, strcmp(result, pid_file_path));

  container_config_destroy(config);
}

TEST(LibcontainerTest, LogPreserve) {
  errno = EPERM;
  PLOG_PRESERVE(ERROR) << "This is an expected error log";
  ASSERT_EQ(EPERM, errno);
}

class ContainerTest : public ::testing::Test {
 public:
  ContainerTest() = default;
  ~ContainerTest() override = default;

  void SetUp() override {
    g_mock_posix_state = new MockPosixState();
    g_mock_cgroup_state = new MockCgroupState();
    libcontainer::Cgroup::SetCgroupFactoryForTesting(&MockCgroup::Create);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    ASSERT_TRUE(base::CreateTemporaryDirInDir(
        temp_dir_.path(), FILE_PATH_LITERAL("container_test"), &rootfs_));

    minijail_alt_syscall_table = nullptr;
    minijail_ipc_called = 0;
    minijail_vfs_called = 0;
    minijail_net_called = 0;
    minijail_pids_called = 0;
    minijail_run_as_init_called = 0;
    minijail_user_called = 0;
    minijail_cgroups_called = 0;
    minijail_wait_called = 0;
    minijail_reset_signal_mask_called = 0;

    mount_flags_ = MS_NOSUID | MS_NODEV | MS_NOEXEC;

    config_.reset(new Config());
    container_config_uid_map(config_->get(), "0 0 4294967295");
    container_config_gid_map(config_->get(), "0 0 4294967295");
    container_config_rootfs(config_->get(), rootfs_.value().c_str());

    static const char* kArgv[] = {
        "/sbin/init",
    };
    container_config_program_argv(config_->get(), kArgv, 1);
    container_config_alt_syscall_table(config_->get(), "testsyscalltable");
    container_config_add_mount(config_->get(),
                               "testtmpfs",
                               "tmpfs",
                               "/tmp",
                               "tmpfs",
                               nullptr,
                               nullptr,
                               mount_flags_,
                               0,
                               1000,
                               1000,
                               0x666,
                               0,
                               0);
    container_config_add_device(config_->get(),
                                'c',
                                "/dev/foo",
                                S_IRWXU | S_IRWXG,
                                245,
                                2,
                                0,
                                1000,
                                1001,
                                1,
                                1,
                                0);
    // test dynamic minor on /dev/null
    container_config_add_device(config_->get(),
                                'c',
                                "/dev/null",
                                S_IRWXU | S_IRWXG,
                                1,
                                -1,
                                1,
                                1000,
                                1001,
                                1,
                                1,
                                0);

    container_config_set_cpu_shares(config_->get(), kTestCpuShares);
    container_config_set_cpu_cfs_params(
        config_->get(), kTestCpuQuota, kTestCpuPeriod);
    /* Invalid params, so this won't be applied. */
    container_config_set_cpu_rt_params(config_->get(), 20000, 20000);

    base::FilePath rundir;
    ASSERT_TRUE(base::CreateTemporaryDirInDir(
        temp_dir_.path(), FILE_PATH_LITERAL("container_test_run"), &rundir));
    container_.reset(new Container("containerUT", rundir));
    ASSERT_NE(nullptr, container_.get());
  }

  void TearDown() override {
    container_.reset();
    config_.reset();

    ASSERT_TRUE(temp_dir_.Delete());
    delete g_mock_posix_state;
    g_mock_posix_state = nullptr;
    libcontainer::Cgroup::SetCgroupFactoryForTesting(nullptr);
    delete g_mock_cgroup_state;
  }

 protected:
  std::unique_ptr<Config> config_;
  std::unique_ptr<Container> container_;
  int mount_flags_;
  base::FilePath rootfs_;

 private:
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(ContainerTest);
};

TEST_F(ContainerTest, TestMountTmpStart) {
  ASSERT_EQ(0, container_start(container_->get(), config_->get()));
  ASSERT_EQ(2, g_mock_posix_state->mount_args.size());
  EXPECT_EQ(false, g_mock_posix_state->mount_args[1].outside_mount);
  EXPECT_STREQ("tmpfs", g_mock_posix_state->mount_args[1].source.c_str());
  EXPECT_STREQ("/tmp",
               g_mock_posix_state->mount_args[1].target.value().c_str());
  EXPECT_STREQ("tmpfs",
               g_mock_posix_state->mount_args[1].filesystemtype.c_str());
  EXPECT_EQ(g_mock_posix_state->mount_args[1].mountflags,
            static_cast<unsigned long>(mount_flags_));
  EXPECT_EQ(nullptr, g_mock_posix_state->mount_args[1].data);

  EXPECT_EQ(1, minijail_ipc_called);
  EXPECT_EQ(1, minijail_vfs_called);
  EXPECT_EQ(1, minijail_net_called);
  EXPECT_EQ(1, minijail_pids_called);
  EXPECT_EQ(1, minijail_user_called);
  EXPECT_EQ(1, minijail_cgroups_called);
  EXPECT_EQ(1, minijail_run_as_init_called);
  EXPECT_EQ(1, g_mock_cgroup_state->deny_all_devs_called_count);

  ASSERT_EQ(2, g_mock_cgroup_state->added_devices.size());
  EXPECT_EQ(1, g_mock_cgroup_state->added_devices[0].allow);
  EXPECT_EQ(245, g_mock_cgroup_state->added_devices[0].major);
  EXPECT_EQ(2, g_mock_cgroup_state->added_devices[0].minor);
  EXPECT_EQ(1, g_mock_cgroup_state->added_devices[0].read);
  EXPECT_EQ(1, g_mock_cgroup_state->added_devices[0].write);
  EXPECT_EQ(0, g_mock_cgroup_state->added_devices[0].modify);
  EXPECT_EQ('c', g_mock_cgroup_state->added_devices[0].type);

  EXPECT_EQ(1, g_mock_cgroup_state->added_devices[1].allow);
  EXPECT_EQ(1, g_mock_cgroup_state->added_devices[1].major);
  EXPECT_EQ(-1, g_mock_cgroup_state->added_devices[1].minor);
  EXPECT_EQ(1, g_mock_cgroup_state->added_devices[1].read);
  EXPECT_EQ(1, g_mock_cgroup_state->added_devices[1].write);
  EXPECT_EQ(0, g_mock_cgroup_state->added_devices[1].modify);
  EXPECT_EQ('c', g_mock_cgroup_state->added_devices[1].type);

  EXPECT_EQ(1, g_mock_cgroup_state->set_cpu_shares_count);
  EXPECT_EQ(kTestCpuShares, container_config_get_cpu_shares(config_->get()));
  EXPECT_EQ(1, g_mock_cgroup_state->set_cpu_quota_count);
  EXPECT_EQ(kTestCpuQuota, container_config_get_cpu_quota(config_->get()));
  EXPECT_EQ(1, g_mock_cgroup_state->set_cpu_period_count);
  EXPECT_EQ(kTestCpuPeriod, container_config_get_cpu_period(config_->get()));
  EXPECT_EQ(0, g_mock_cgroup_state->set_cpu_rt_runtime_count);
  EXPECT_EQ(0, container_config_get_cpu_rt_runtime(config_->get()));
  EXPECT_EQ(0, g_mock_cgroup_state->set_cpu_rt_period_count);
  EXPECT_EQ(0, container_config_get_cpu_rt_period(config_->get()));

  ASSERT_NE(nullptr, minijail_alt_syscall_table);
  EXPECT_STREQ("testsyscalltable", minijail_alt_syscall_table);

  EXPECT_EQ(0, container_wait(container_->get()));
  EXPECT_EQ(1, minijail_wait_called);
  EXPECT_EQ(1, minijail_reset_signal_mask_called);
}

TEST_F(ContainerTest, TestKillContainer) {
  ASSERT_EQ(0, container_start(container_->get(), config_->get()));
  EXPECT_EQ(0, container_kill(container_->get()));
  EXPECT_EQ(std::vector<int>{SIGKILL}, g_mock_posix_state->kill_sigs);
  EXPECT_EQ(1, minijail_wait_called);
}

}  // namespace libcontainer

// libc stubs so the UT doesn't need root to call mount, etc.
extern "C" {

extern decltype(chmod) __real_chmod;
int __wrap_chmod(const char* path, mode_t mode) {
  if (!libcontainer::g_mock_posix_state)
    return __real_chmod(path, mode);
  return 0;
}

extern decltype(chown) __real_chown;
int __wrap_chown(const char* path, uid_t owner, gid_t group) {
  if (!libcontainer::g_mock_posix_state)
    return __real_chown(path, owner, group);
  return 0;
}

extern decltype(getuid) __real_getuid;
uid_t __wrap_getuid(void) {
  if (!libcontainer::g_mock_posix_state)
    return __real_getuid();
  return 0;
}

extern decltype(kill) __real_kill;
int __wrap_kill(pid_t pid, int sig) {
  if (!libcontainer::g_mock_posix_state)
    return __real_kill(pid, sig);
  libcontainer::g_mock_posix_state->kill_sigs.push_back(sig);
  return 0;
}

extern decltype(mkdir) __real_mkdir;
int __wrap_mkdir(const char* pathname, mode_t mode) {
  if (!libcontainer::g_mock_posix_state)
    return __real_mkdir(pathname, mode);
  return 0;
}

extern decltype(mkdtemp) __real_mkdtemp;
char* __wrap_mkdtemp(char* template_string) {
  if (!libcontainer::g_mock_posix_state)
    return __real_mkdtemp(template_string);
  libcontainer::g_mock_posix_state->mkdtemp_root =
      base::FilePath(template_string);
  return template_string;
}

extern decltype(mount) __real_mount;
int __wrap_mount(const char* source,
                 const char* target,
                 const char* filesystemtype,
                 unsigned long mountflags,
                 const void* data) {
  if (!libcontainer::g_mock_posix_state)
    return __real_mount(source, target, filesystemtype, mountflags, data);

  libcontainer::g_mock_posix_state->mount_args.emplace_back(
      libcontainer::MockPosixState::MountArgs{source,
                                              base::FilePath(target),
                                              filesystemtype,
                                              mountflags,
                                              data,
                                              true});
  return 0;
}

extern decltype(rmdir) __real_rmdir;
int __wrap_rmdir(const char* pathname) {
  if (!libcontainer::g_mock_posix_state)
    return __real_rmdir(pathname);
  return 0;
}

extern decltype(umount) __real_umount;
int __wrap_umount(const char* target) {
  if (!libcontainer::g_mock_posix_state)
    return __real_umount(target);
  return 0;
}

extern decltype(umount2) __real_umount2;
int __wrap_umount2(const char* target, int flags) {
  if (!libcontainer::g_mock_posix_state)
    return __real_umount2(target, flags);
  return 0;
}

extern decltype(unlink) __real_unlink;
int __wrap_unlink(const char* pathname) {
  if (!libcontainer::g_mock_posix_state)
    return __real_unlink(pathname);
  return 0;
}

extern decltype(__xmknod) __real___xmknod;
int __wrap___xmknod(int ver, const char* pathname, mode_t mode, dev_t* dev) {
  if (!libcontainer::g_mock_posix_state)
    return __real___xmknod(ver, pathname, mode, dev);
  return 0;
}

extern decltype(__xstat) __real___xstat;
int __wrap___xstat(int ver, const char* path, struct stat* buf) {
  if (!libcontainer::g_mock_posix_state)
    return __real___xstat(ver, path, buf);
  buf->st_rdev = libcontainer::g_mock_posix_state->stat_rdev_ret;
  return 0;
}

/* Minijail stubs */
struct minijail* minijail_new(void) {
  return (struct minijail*)0x55;
}

void minijail_destroy(struct minijail* j) {}

int minijail_mount_with_data(struct minijail* j,
                             const char* source,
                             const char* target,
                             const char* filesystemtype,
                             unsigned long mountflags,
                             const char* data) {
  libcontainer::g_mock_posix_state->mount_args.emplace_back(
      libcontainer::MockPosixState::MountArgs{source,
                                              base::FilePath(target),
                                              filesystemtype,
                                              mountflags,
                                              data,
                                              false});
  return 0;
}

void minijail_namespace_user_disable_setgroups(struct minijail* j) {}

void minijail_namespace_vfs(struct minijail* j) {
  ++libcontainer::minijail_vfs_called;
}

void minijail_namespace_ipc(struct minijail* j) {
  ++libcontainer::minijail_ipc_called;
}

void minijail_namespace_net(struct minijail* j) {
  ++libcontainer::minijail_net_called;
}

void minijail_namespace_pids(struct minijail* j) {
  ++libcontainer::minijail_pids_called;
}

void minijail_namespace_user(struct minijail* j) {
  ++libcontainer::minijail_user_called;
}

void minijail_namespace_cgroups(struct minijail* j) {
  ++libcontainer::minijail_cgroups_called;
}

int minijail_uidmap(struct minijail* j, const char* uidmap) {
  return 0;
}

int minijail_gidmap(struct minijail* j, const char* gidmap) {
  return 0;
}

int minijail_enter_pivot_root(struct minijail* j, const char* dir) {
  return 0;
}

void minijail_run_as_init(struct minijail* j) {
  ++libcontainer::minijail_run_as_init_called;
}

int minijail_run_pid_pipes_no_preload(struct minijail* j,
                                      const char* filename,
                                      char* const argv[],
                                      pid_t* pchild_pid,
                                      int* pstdin_fd,
                                      int* pstdout_fd,
                                      int* pstderr_fd) {
  *pchild_pid = libcontainer::kInitTestPid;
  return 0;
}

int minijail_write_pid_file(struct minijail* j, const char* path) {
  return 0;
}

int minijail_wait(struct minijail* j) {
  ++libcontainer::minijail_wait_called;
  return 0;
}

int minijail_use_alt_syscall(struct minijail* j, const char* table) {
  libcontainer::minijail_alt_syscall_table = table;
  return 0;
}

int minijail_add_to_cgroup(struct minijail* j, const char* cg_path) {
  return 0;
}

void minijail_reset_signal_mask(struct minijail* j) {
  ++libcontainer::minijail_reset_signal_mask_called;
}

void minijail_skip_remount_private(struct minijail* j) {}

void minijail_close_open_fds(struct minijail* j) {}

}  // extern "C"
