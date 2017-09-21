/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <signal.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <vector>

#include <base/files/file_path.h>
#include <base/memory/ptr_util.h>

#include "libcontainer/test_harness.h"

#include "libcontainer/cgroup.h"
#include "libcontainer/config.h"
#include "libcontainer/container.h"
#include "libcontainer/libcontainer.h"
#include "libcontainer/libcontainer_util.h"

namespace {

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
    return base::MakeUnique<MockCgroup>(g_mock_cgroup_state);
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

}  // namespace

static const pid_t INIT_TEST_PID = 5555;
static const int TEST_CPU_SHARES = 200;
static const int TEST_CPU_QUOTA = 20000;
static const int TEST_CPU_PERIOD = 50000;

struct mount_args {
  char* source;
  char* target;
  char* filesystemtype;
  unsigned long mountflags;
  const void* data;
  bool outside_mount;
};
static struct mount_args mount_call_args[5];
static int mount_called;

struct mknod_args {
  base::FilePath pathname;
  mode_t mode;
  dev_t dev;
};
static struct mknod_args mknod_call_args;
static bool mknod_called;
static dev_t stat_rdev_ret;

static int kill_called;
static int kill_sig;
static const char* minijail_alt_syscall_table;
static int minijail_ipc_called;
static int minijail_vfs_called;
static int minijail_net_called;
static int minijail_pids_called;
static int minijail_run_as_init_called;
static int minijail_user_called;
static int minijail_cgroups_called;
static int minijail_wait_called;
static int minijail_reset_signal_mask_called;
static int mount_ret;
static base::FilePath mkdtemp_root;

TEST(premounted_runfs) {
  char premounted_runfs[] = "/tmp/cgtest_run/root";
  struct container_config* config = container_config_create();
  ASSERT_NE(nullptr, config);

  container_config_premounted_runfs(config, premounted_runfs);
  const char* result = container_config_get_premounted_runfs(config);
  ASSERT_EQ(0, strcmp(result, premounted_runfs));

  container_config_destroy(config);
}

TEST(pid_file_path) {
  char pid_file_path[] = "/tmp/cgtest_run/root/container.pid";
  struct container_config* config = container_config_create();
  ASSERT_NE(nullptr, config);

  container_config_pid_file(config, pid_file_path);
  const char* result = container_config_get_pid_file(config);
  ASSERT_EQ(0, strcmp(result, pid_file_path));

  container_config_destroy(config);
}

TEST(plog_preserve) {
  errno = EPERM;
  PLOG_PRESERVE(ERROR) << "This is an expected error log";
  ASSERT_EQ(EPERM, errno);
}

/* Start of tests. */
FIXTURE(container_test) {
  std::unique_ptr<libcontainer::Config> config;
  std::unique_ptr<libcontainer::Container> container;
  int mount_flags;
  char* rootfs;
};

FIXTURE_SETUP(container_test) {
  char temp_template[] = "/tmp/cgtestXXXXXX";
  char rundir_template[] = "/tmp/cgtest_runXXXXXX";
  char* rundir;
  char path[256];
  const char* pargs[] = {
      "/sbin/init",
  };

  g_mock_cgroup_state = new MockCgroupState();
  libcontainer::Cgroup::SetCgroupFactoryForTesting(&MockCgroup::Create);

  memset(&mount_call_args, 0, sizeof(mount_call_args));
  mount_called = 0;
  mknod_called = false;

  self->rootfs = strdup(mkdtemp(temp_template));

  kill_called = 0;
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
  mount_ret = 0;
  stat_rdev_ret = makedev(2, 3);

  snprintf(path, sizeof(path), "%s/dev", self->rootfs);

  self->mount_flags = MS_NOSUID | MS_NODEV | MS_NOEXEC;

  self->config.reset(new libcontainer::Config());
  container_config_uid_map(self->config->get(), "0 0 4294967295");
  container_config_gid_map(self->config->get(), "0 0 4294967295");
  container_config_rootfs(self->config->get(), self->rootfs);
  container_config_program_argv(self->config->get(), pargs, 1);
  container_config_alt_syscall_table(self->config->get(), "testsyscalltable");
  container_config_add_mount(self->config->get(),
                             "testtmpfs",
                             "tmpfs",
                             "/tmp",
                             "tmpfs",
                             nullptr,
                             nullptr,
                             self->mount_flags,
                             0,
                             1000,
                             1000,
                             0x666,
                             0,
                             0);
  container_config_add_device(self->config->get(),
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
  /* test dynamic minor on /dev/null */
  container_config_add_device(self->config->get(),
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

  container_config_set_cpu_shares(self->config->get(), TEST_CPU_SHARES);
  container_config_set_cpu_cfs_params(
      self->config->get(), TEST_CPU_QUOTA, TEST_CPU_PERIOD);
  /* Invalid params, so this won't be applied. */
  container_config_set_cpu_rt_params(self->config->get(), 20000, 20000);

  rundir = mkdtemp(rundir_template);
  self->container.reset(
      new libcontainer::Container("containerUT", base::FilePath(rundir)));
  ASSERT_NE(nullptr, self->container->get());
}

FIXTURE_TEARDOWN(container_test) {
  char path[256];
  int i;

  self->container.reset();
  self->config.reset();
  snprintf(path, sizeof(path), "rm -rf %s", self->rootfs);
  EXPECT_EQ(0, system(path));
  free(self->rootfs);

  for (i = 0; i < mount_called; i++) {
    free(mount_call_args[i].source);
    free(mount_call_args[i].target);
    free(mount_call_args[i].filesystemtype);
  }

  libcontainer::Cgroup::SetCgroupFactoryForTesting(nullptr);
  delete g_mock_cgroup_state;
}

TEST_F(container_test, test_mount_tmp_start) {
  ASSERT_EQ(0, container_start(self->container->get(), self->config->get()));
  ASSERT_EQ(2, mount_called);
  EXPECT_EQ(false, mount_call_args[1].outside_mount);
  EXPECT_STREQ("tmpfs", mount_call_args[1].source);
  EXPECT_STREQ("/tmp", mount_call_args[1].target);
  EXPECT_STREQ("tmpfs", mount_call_args[1].filesystemtype);
  EXPECT_EQ(mount_call_args[1].mountflags,
            static_cast<unsigned long>(self->mount_flags));
  EXPECT_EQ(nullptr, mount_call_args[1].data);

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

  ASSERT_EQ(true, mknod_called);
  base::FilePath node_path = mkdtemp_root.Append("root/dev/null");
  EXPECT_STREQ(node_path.value().c_str(),
               mknod_call_args.pathname.value().c_str());
  EXPECT_EQ(mknod_call_args.mode,
            static_cast<mode_t>(S_IRWXU | S_IRWXG | S_IFCHR));
  EXPECT_EQ(mknod_call_args.dev, makedev(1, 3));

  EXPECT_EQ(1, g_mock_cgroup_state->set_cpu_shares_count);
  EXPECT_EQ(TEST_CPU_SHARES,
            container_config_get_cpu_shares(self->config->get()));
  EXPECT_EQ(1, g_mock_cgroup_state->set_cpu_quota_count);
  EXPECT_EQ(TEST_CPU_QUOTA,
            container_config_get_cpu_quota(self->config->get()));
  EXPECT_EQ(1, g_mock_cgroup_state->set_cpu_period_count);
  EXPECT_EQ(TEST_CPU_PERIOD,
            container_config_get_cpu_period(self->config->get()));
  EXPECT_EQ(0, g_mock_cgroup_state->set_cpu_rt_runtime_count);
  EXPECT_EQ(0, container_config_get_cpu_rt_runtime(self->config->get()));
  EXPECT_EQ(0, g_mock_cgroup_state->set_cpu_rt_period_count);
  EXPECT_EQ(0, container_config_get_cpu_rt_period(self->config->get()));

  ASSERT_NE(nullptr, minijail_alt_syscall_table);
  EXPECT_STREQ("testsyscalltable", minijail_alt_syscall_table);

  EXPECT_EQ(0, container_wait(self->container->get()));
  EXPECT_EQ(1, minijail_wait_called);
  EXPECT_EQ(1, minijail_reset_signal_mask_called);
}

TEST_F(container_test, test_kill_container) {
  ASSERT_EQ(0, container_start(self->container->get(), self->config->get()));
  EXPECT_EQ(0, container_kill(self->container->get()));
  EXPECT_EQ(1, kill_called);
  EXPECT_EQ(SIGKILL, kill_sig);
  EXPECT_EQ(1, minijail_wait_called);
}

/* libc stubs so the UT doesn't need root to call mount, etc. */
extern "C" {

int mount(const char* source,
          const char* target,
          const char* filesystemtype,
          unsigned long mountflags,
          const void* data) {
  if (mount_called >= 5)
    return 0;

  mount_call_args[mount_called].source = strdup(source);
  mount_call_args[mount_called].target = strdup(target);
  mount_call_args[mount_called].filesystemtype = strdup(filesystemtype);
  mount_call_args[mount_called].mountflags = mountflags;
  mount_call_args[mount_called].data = data;
  mount_call_args[mount_called].outside_mount = true;
  ++mount_called;
  return 0;
}

int umount(const char* target) {
  return 0;
}

int umount2(const char* target, int flags) {
  return 0;
}

#ifdef __USE_EXTERN_INLINES
/* Some environments use an inline version of mknod. */
int __xmknod(int ver, const char* pathname, __mode_t mode, __dev_t* dev)
#else
int mknod(const char* pathname, mode_t mode, dev_t dev)
#endif
{
  mknod_call_args.pathname = base::FilePath(pathname);
  mknod_call_args.mode = mode;
#ifdef __USE_EXTERN_INLINES
  mknod_call_args.dev = *dev;
#else
  mknod_call_args.dev = dev;
#endif
  mknod_called = true;
  return 0;
}

int chown(const char* path, uid_t owner, gid_t group) {
  return 0;
}

int kill(pid_t pid, int sig) {
  ++kill_called;
  kill_sig = sig;
  return 0;
}

#ifdef __USE_EXTERN_INLINES
/* Some environments use an inline version of stat. */
int __xstat(int ver, const char* path, struct stat* buf)
#else
int stat(const char* path, struct stat* buf)
#endif
{
  buf->st_rdev = stat_rdev_ret;
  return 0;
}

int chmod(const char* path, mode_t mode) {
  return 0;
}

char* mkdtemp(char* template_string) {
  mkdtemp_root = base::FilePath(template_string);
  return template_string;
}

int mkdir(const char* pathname, mode_t mode) {
  return 0;
}

int rmdir(const char* pathname) {
  return 0;
}

int unlink(const char* pathname) {
  return 0;
}

uid_t getuid(void) {
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
  if (mount_called >= 5)
    return 0;

  mount_call_args[mount_called].source = strdup(source);
  mount_call_args[mount_called].target = strdup(target);
  mount_call_args[mount_called].filesystemtype = strdup(filesystemtype);
  mount_call_args[mount_called].mountflags = mountflags;
  mount_call_args[mount_called].data = data;
  mount_call_args[mount_called].outside_mount = false;
  ++mount_called;
  return 0;
}

void minijail_namespace_user_disable_setgroups(struct minijail* j) {}

void minijail_namespace_vfs(struct minijail* j) {
  ++minijail_vfs_called;
}

void minijail_namespace_ipc(struct minijail* j) {
  ++minijail_ipc_called;
}

void minijail_namespace_net(struct minijail* j) {
  ++minijail_net_called;
}

void minijail_namespace_pids(struct minijail* j) {
  ++minijail_pids_called;
}

void minijail_namespace_user(struct minijail* j) {
  ++minijail_user_called;
}

void minijail_namespace_cgroups(struct minijail* j) {
  ++minijail_cgroups_called;
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
  ++minijail_run_as_init_called;
}

int minijail_run_pid_pipes_no_preload(struct minijail* j,
                                      const char* filename,
                                      char* const argv[],
                                      pid_t* pchild_pid,
                                      int* pstdin_fd,
                                      int* pstdout_fd,
                                      int* pstderr_fd) {
  *pchild_pid = INIT_TEST_PID;
  return 0;
}

int minijail_write_pid_file(struct minijail* j, const char* path) {
  return 0;
}

int minijail_wait(struct minijail* j) {
  ++minijail_wait_called;
  return 0;
}

int minijail_use_alt_syscall(struct minijail* j, const char* table) {
  minijail_alt_syscall_table = table;
  return 0;
}

int minijail_add_to_cgroup(struct minijail* j, const char* cg_path) {
  return 0;
}

void minijail_reset_signal_mask(struct minijail* j) {
  ++minijail_reset_signal_mask_called;
}

void minijail_skip_remount_private(struct minijail* j) {}

void minijail_close_open_fds(struct minijail* j) {}

}  // extern "C"

TEST_HARNESS_MAIN
