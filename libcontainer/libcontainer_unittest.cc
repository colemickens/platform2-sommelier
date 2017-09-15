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

#include "libcontainer/test_harness.h"

#include "libcontainer/container_cgroup.h"
#include "libcontainer/libcontainer.h"

static const pid_t INIT_TEST_PID = 5555;
static const int TEST_CPU_SHARES = 200;
static const int TEST_CPU_QUOTA = 20000;
static const int TEST_CPU_PERIOD = 50000;

struct mount_args {
	char *source;
	char *target;
	char *filesystemtype;
	unsigned long mountflags;
	const void *data;
};
static struct mount_args mount_call_args[5];
static int mount_called;

struct mknod_args {
	char *pathname;
	mode_t mode;
	dev_t dev;
};
static struct mknod_args mknod_call_args;
static dev_t stat_rdev_ret;

static int kill_called;
static int kill_sig;
static const char *minijail_alt_syscall_table;
static int minijail_ipc_called;
static int minijail_vfs_called;
static int minijail_net_called;
static int minijail_pids_called;
static int minijail_run_as_init_called;
static int minijail_user_called;
static int minijail_wait_called;
static int minijail_reset_signal_mask_called;
static int mount_ret;
static char *mkdtemp_root;

/* global mock cgroup. */
#define MAX_ADD_DEVICE_CALLS 2
struct mock_cgroup {
	struct container_cgroup cg;
	int freeze_ret;
	int thaw_ret;
	int deny_all_devs_ret;
	int add_device_ret;
	int set_cpu_ret;

	int init_called_count;
	int deny_all_devs_called_count;

	int add_dev_allow[MAX_ADD_DEVICE_CALLS];
	int add_dev_major[MAX_ADD_DEVICE_CALLS];
	int add_dev_minor[MAX_ADD_DEVICE_CALLS];
	int add_dev_read[MAX_ADD_DEVICE_CALLS];
	int add_dev_write[MAX_ADD_DEVICE_CALLS];
	int add_dev_modify[MAX_ADD_DEVICE_CALLS];
	char add_dev_type[MAX_ADD_DEVICE_CALLS];
	int add_dev_called_count;

	int set_cpu_shares_count;
	int set_cpu_quota_count;
	int set_cpu_period_count;
	int set_cpu_rt_runtime_count;
	int set_cpu_rt_period_count;
};

static struct mock_cgroup gmcg;

static int mock_freeze(const struct container_cgroup *cg) {
	struct mock_cgroup *mcg = (struct mock_cgroup *)cg;
	return mcg->freeze_ret;
}

static int mock_thaw(const struct container_cgroup *cg) {
	struct mock_cgroup *mcg = (struct mock_cgroup *)cg;
	return mcg->thaw_ret;
}

static int mock_deny_all_devices(const struct container_cgroup *cg) {
	struct mock_cgroup *mcg = (struct mock_cgroup *)cg;
	++mcg->deny_all_devs_called_count;
	return mcg->deny_all_devs_ret;
}

static int mock_add_device(const struct container_cgroup *cg, int allow,
			   int major, int minor, int read, int write,
			   int modify, char type) {
	struct mock_cgroup *mcg = (struct mock_cgroup *)cg;

	if (mcg->add_dev_called_count >= MAX_ADD_DEVICE_CALLS)
		return mcg->add_device_ret;
	mcg->add_dev_allow[mcg->add_dev_called_count] = allow;
	mcg->add_dev_major[mcg->add_dev_called_count] = major;
	mcg->add_dev_minor[mcg->add_dev_called_count] = minor;
	mcg->add_dev_read[mcg->add_dev_called_count] = read;
	mcg->add_dev_write[mcg->add_dev_called_count] = write;
	mcg->add_dev_modify[mcg->add_dev_called_count] = modify;
	mcg->add_dev_type[mcg->add_dev_called_count] = type;
	mcg->add_dev_called_count++;
	return mcg->add_device_ret;
}

static int mock_set_cpu_shares(const struct container_cgroup *cg, int shares) {
	struct mock_cgroup *mcg = (struct mock_cgroup *)cg;
	mcg->set_cpu_shares_count++;
	return mcg->set_cpu_ret;
}

static int mock_set_cpu_quota(const struct container_cgroup *cg, int quota) {
	struct mock_cgroup *mcg = (struct mock_cgroup *)cg;
	mcg->set_cpu_quota_count++;
	return mcg->set_cpu_ret;
}

static int mock_set_cpu_period(const struct container_cgroup *cg, int period) {
	struct mock_cgroup *mcg = (struct mock_cgroup *)cg;
	mcg->set_cpu_period_count++;
	return mcg->set_cpu_ret;
}

static int mock_set_cpu_rt_runtime(const struct container_cgroup *cg,
				   int rt_runtime) {
	struct mock_cgroup *mcg = (struct mock_cgroup *)cg;
	mcg->set_cpu_rt_runtime_count++;
	return mcg->set_cpu_ret;
}

static int mock_set_cpu_rt_period(const struct container_cgroup *cg,
				  int rt_period) {
	struct mock_cgroup *mcg = (struct mock_cgroup *)cg;
	mcg->set_cpu_rt_period_count++;
	return mcg->set_cpu_ret;
}

struct container_cgroup *container_cgroup_new(const char *name,
					      const char *cgroup_root,
					      const char *cgroup_parent,
					      uid_t uid, gid_t gid) {
	gmcg.cg.name = strdup(name);
	return &gmcg.cg;
}

void container_cgroup_destroy(struct container_cgroup *c) {
	free(c->name);
}

TEST(premounted_runfs) {
	char premounted_runfs[] = "/tmp/cgtest_run/root";
	struct container_config *config = container_config_create();
	ASSERT_NE(nullptr, config);

	container_config_premounted_runfs(config, premounted_runfs);
	const char *result = container_config_get_premounted_runfs(config);
	ASSERT_EQ(0, strcmp(result, premounted_runfs));

	container_config_destroy(config);
}

TEST(pid_file_path) {
	char pid_file_path[] = "/tmp/cgtest_run/root/container.pid";
	struct container_config *config = container_config_create();
	ASSERT_NE(nullptr, config);

	container_config_pid_file(config, pid_file_path);
	const char *result = container_config_get_pid_file(config);
	ASSERT_EQ(0, strcmp(result, pid_file_path));

	container_config_destroy(config);
}

/* Start of tests. */
FIXTURE(container_test) {
	struct container_config *config;
	struct container *container;
	int mount_flags;
	char *rootfs;
};

FIXTURE_SETUP(container_test) {
	char temp_template[] = "/tmp/cgtestXXXXXX";
	char rundir_template[] = "/tmp/cgtest_runXXXXXX";
	char *rundir;
	char path[256];
	const char *pargs[] = {
		"/sbin/init",
	};

	memset(&mount_call_args, 0, sizeof(mount_call_args));
	mount_called = 0;
	memset(&mknod_call_args, 0, sizeof(mknod_call_args));
	mkdtemp_root = nullptr;

	memset(&gmcg, 0, sizeof(gmcg));
	static const struct cgroup_ops cgops = {
		.freeze = mock_freeze,
		.thaw = mock_thaw,
		.deny_all_devices = mock_deny_all_devices,
		.add_device = mock_add_device,
		.set_cpu_shares = mock_set_cpu_shares,
		.set_cpu_quota = mock_set_cpu_quota,
		.set_cpu_period = mock_set_cpu_period,
		.set_cpu_rt_runtime = mock_set_cpu_rt_runtime,
		.set_cpu_rt_period = mock_set_cpu_rt_period,
	};
	gmcg.cg.ops = &cgops;

	self->rootfs = strdup(mkdtemp(temp_template));

	kill_called = 0;
	minijail_alt_syscall_table = nullptr;
	minijail_ipc_called = 0;
	minijail_vfs_called = 0;
	minijail_net_called = 0;
	minijail_pids_called = 0;
	minijail_run_as_init_called = 0;
	minijail_user_called = 0;
	minijail_wait_called = 0;
	minijail_reset_signal_mask_called = 0;
	mount_ret = 0;
	stat_rdev_ret = makedev(2, 3);

	snprintf(path, sizeof(path), "%s/dev", self->rootfs);

	self->mount_flags = MS_NOSUID | MS_NODEV | MS_NOEXEC;

	self->config = container_config_create();
	container_config_rootfs(self->config, self->rootfs);
	container_config_program_argv(self->config, pargs, 1);
	container_config_alt_syscall_table(self->config, "testsyscalltable");
	container_config_add_mount(self->config,
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
				   1);
	container_config_add_device(self->config,
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
	container_config_add_device(self->config,
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

	container_config_set_cpu_shares(self->config, TEST_CPU_SHARES);
	container_config_set_cpu_cfs_params(
		self->config, TEST_CPU_QUOTA, TEST_CPU_PERIOD);
	/* Invalid params, so this won't be applied. */
	container_config_set_cpu_rt_params(self->config, 20000, 20000);

	rundir = mkdtemp(rundir_template);
	self->container = container_new("containerUT", rundir);
	ASSERT_NE(nullptr, self->container);
}

FIXTURE_TEARDOWN(container_test) {
	char path[256];
	int i;

	container_destroy(self->container);
	snprintf(path, sizeof(path), "rm -rf %s", self->rootfs);
	EXPECT_EQ(0, system(path));
	free(self->rootfs);

	for (i = 0; i < mount_called; i++) {
		free(mount_call_args[i].source);
		free(mount_call_args[i].target);
		free(mount_call_args[i].filesystemtype);
	}
	free(mknod_call_args.pathname);
	free(mkdtemp_root);
}

TEST_F(container_test, test_mount_tmp_start) {
	char *path;

	EXPECT_EQ(0, container_start(self->container, self->config));
	EXPECT_EQ(2, mount_called);
	EXPECT_EQ(0, strcmp(mount_call_args[1].source, "tmpfs"));
	EXPECT_LT(0, asprintf(&path, "%s/root/tmp", mkdtemp_root));
	EXPECT_EQ(0, strcmp(mount_call_args[1].target, path));
	free(path);
	EXPECT_EQ(0, strcmp(mount_call_args[1].filesystemtype,
			    "tmpfs"));
	EXPECT_EQ(mount_call_args[1].mountflags,
		  static_cast<unsigned long>(self->mount_flags));
	EXPECT_EQ(mount_call_args[1].data, nullptr);

	EXPECT_EQ(1, minijail_ipc_called);
	EXPECT_EQ(1, minijail_vfs_called);
	EXPECT_EQ(1, minijail_net_called);
	EXPECT_EQ(1, minijail_pids_called);
	EXPECT_EQ(1, minijail_user_called);
	EXPECT_EQ(1, minijail_run_as_init_called);
	EXPECT_EQ(1, gmcg.deny_all_devs_called_count);

	EXPECT_EQ(1, gmcg.add_dev_allow[0]);
	EXPECT_EQ(245, gmcg.add_dev_major[0]);
	EXPECT_EQ(2, gmcg.add_dev_minor[0]);
	EXPECT_EQ(1, gmcg.add_dev_read[0]);
	EXPECT_EQ(1, gmcg.add_dev_write[0]);
	EXPECT_EQ(0, gmcg.add_dev_modify[0]);
	EXPECT_EQ('c', gmcg.add_dev_type[0]);

	EXPECT_EQ(1, gmcg.add_dev_allow[1]);
	EXPECT_EQ(1, gmcg.add_dev_major[1]);
	EXPECT_EQ(3, gmcg.add_dev_minor[1]);
	EXPECT_EQ(1, gmcg.add_dev_read[1]);
	EXPECT_EQ(1, gmcg.add_dev_write[1]);
	EXPECT_EQ(0, gmcg.add_dev_modify[1]);
	EXPECT_EQ('c', gmcg.add_dev_type[1]);

	EXPECT_LT(0, asprintf(&path, "%s/root/dev/null", mkdtemp_root));
	EXPECT_EQ(0, strcmp(mknod_call_args.pathname, path));
	free(path);
	EXPECT_EQ(mknod_call_args.mode,
		  static_cast<mode_t>(S_IRWXU | S_IRWXG | S_IFCHR));
	EXPECT_EQ(mknod_call_args.dev, makedev(1, 3));

	EXPECT_EQ(1, gmcg.set_cpu_shares_count);
	EXPECT_EQ(TEST_CPU_SHARES,
		  container_config_get_cpu_shares(self->config));
	EXPECT_EQ(1, gmcg.set_cpu_quota_count);
	EXPECT_EQ(TEST_CPU_QUOTA,
		  container_config_get_cpu_quota(self->config));
	EXPECT_EQ(1, gmcg.set_cpu_period_count);
	EXPECT_EQ(TEST_CPU_PERIOD,
		  container_config_get_cpu_period(self->config));
	EXPECT_EQ(0, gmcg.set_cpu_rt_runtime_count);
	EXPECT_EQ(0, container_config_get_cpu_rt_runtime(self->config));
	EXPECT_EQ(0, gmcg.set_cpu_rt_period_count);
	EXPECT_EQ(0, container_config_get_cpu_rt_period(self->config));

	ASSERT_NE(nullptr, minijail_alt_syscall_table);
	EXPECT_EQ(0, strcmp(minijail_alt_syscall_table,
			    "testsyscalltable"));

	EXPECT_EQ(0, container_wait(self->container));
	EXPECT_EQ(1, minijail_wait_called);
	EXPECT_EQ(1, minijail_reset_signal_mask_called);
}

TEST_F(container_test, test_kill_container) {
	EXPECT_EQ(0, container_start(self->container, self->config));
	EXPECT_EQ(0, container_kill(self->container));
	EXPECT_EQ(1, kill_called);
	EXPECT_EQ(SIGKILL, kill_sig);
	EXPECT_EQ(1, minijail_wait_called);
}

/* libc stubs so the UT doesn't need root to call mount, etc. */
extern "C" {

int mount(const char *source, const char *target, const char *filesystemtype,
	  unsigned long mountflags, const void *data) {
	if (mount_called >= 5)
		return 0;

	mount_call_args[mount_called].source = strdup(source);
	mount_call_args[mount_called].target = strdup(target);
	mount_call_args[mount_called].filesystemtype = strdup(filesystemtype);
	mount_call_args[mount_called].mountflags = mountflags;
	mount_call_args[mount_called].data = data;
	++mount_called;
	return 0;
}

int umount(const char *target) {
	return 0;
}

#ifdef __USE_EXTERN_INLINES
/* Some environments use an inline version of mknod. */
int __xmknod(int ver, const char *pathname, __mode_t mode, __dev_t *dev)
#else
int mknod(const char *pathname, mode_t mode, dev_t dev)
#endif
{
	mknod_call_args.pathname = strdup(pathname);
	mknod_call_args.mode = mode;
#ifdef __USE_EXTERN_INLINES
	mknod_call_args.dev = *dev;
#else
	mknod_call_args.dev = dev;
#endif
	return 0;
}

int chown(const char *path, uid_t owner, gid_t group) {
	return 0;
}

int kill(pid_t pid, int sig) {
	++kill_called;
	kill_sig = sig;
	return 0;
}

#ifdef __USE_EXTERN_INLINES
/* Some environments use an inline version of stat. */
int __xstat(int ver, const char *path, struct stat *buf)
#else
int stat(const char *path, struct stat *buf)
#endif
{
	buf->st_rdev = stat_rdev_ret;
	return 0;
}

int chmod(const char *path, mode_t mode) {
	return 0;
}

char *mkdtemp(char *template_string) {
	mkdtemp_root = strdup(template_string);
	return template_string;
}

int mkdir(const char *pathname, mode_t mode) {
	return 0;
}

int rmdir(const char *pathname) {
	return 0;
}

int unlink(const char *pathname) {
	return 0;
}

/* Minijail stubs */
struct minijail *minijail_new(void) {
	return (struct minijail *)0x55;
}

void minijail_destroy(struct minijail *j) {
}

int minijail_mount(struct minijail *j, const char *src, const char *dest,
		   const char *type, unsigned long flags) {
	return 0;
}

void minijail_namespace_vfs(struct minijail *j) {
	++minijail_vfs_called;
}

void minijail_namespace_ipc(struct minijail *j) {
	++minijail_ipc_called;
}

void minijail_namespace_net(struct minijail *j) {
	++minijail_net_called;
}

void minijail_namespace_pids(struct minijail *j) {
	++minijail_pids_called;
}

void minijail_namespace_user(struct minijail *j) {
	++minijail_user_called;
}

int minijail_uidmap(struct minijail *j, const char *uidmap) {
	return 0;
}

int minijail_gidmap(struct minijail *j, const char *gidmap) {
	return 0;
}

int minijail_enter_pivot_root(struct minijail *j, const char *dir) {
	return 0;
}

void minijail_run_as_init(struct minijail *j) {
	++minijail_run_as_init_called;
}

int minijail_run_pid_pipes_no_preload(struct minijail *j, const char *filename,
				      char *const argv[], pid_t *pchild_pid,
				      int *pstdin_fd, int *pstdout_fd,
				      int *pstderr_fd) {
	*pchild_pid = INIT_TEST_PID;
	return 0;
}

int minijail_write_pid_file(struct minijail *j, const char *path) {
	return 0;
}

int minijail_wait(struct minijail *j) {
	++minijail_wait_called;
	return 0;
}

int minijail_use_alt_syscall(struct minijail *j, const char *table) {
	minijail_alt_syscall_table = table;
	return 0;
}

int minijail_add_to_cgroup(struct minijail *j, const char *cg_path) {
	return 0;
}

void minijail_reset_signal_mask(struct minijail *j) {
	++minijail_reset_signal_mask_called;
}

void minijail_skip_remount_private(struct minijail *j) {
}

}   // extern "C"

TEST_HARNESS_MAIN
