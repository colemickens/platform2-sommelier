/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "libcontainer/test_harness.h"

#include "libcontainer/container_cgroup.h"

static const char CGNAME[] = "testcg";
static const char CGPARENTNAME[] = "testparentcg";

static int check_is_dir(const char *path) {
	int rc;
	struct stat st_buf;

	rc = stat(path, &st_buf);
	if (rc)
		return 0;
	return S_ISDIR(st_buf.st_mode);
}

static void create_file_with_content(const char *name, const char *content) {
	FILE *fp = fopen(name, "w");
	if (content)
		fputs(content, fp);
	fclose(fp);
}

static void create_file(const char *name) {
	create_file_with_content(name, nullptr);
}

static int string_in_file(const char *path, const char *str) {
	FILE *fp = fopen(path, "r");
	char *buf = nullptr;
	size_t len;

	if (!fp)
		return 0;

	while (getline(&buf, &len, fp) > 0) {
		if (strstr(buf, str)) {
			free(buf);
			fclose(fp);
			return 1;
		}
	}
	free(buf);
	fclose(fp);
	return 0;
}

static int file_has_line(const char *path, const char *line) {
	FILE *fp;
	char *buf = nullptr;
	size_t len;
	int found;

	fp = fopen(path, "r");
	if (!fp)
		return 0;

	if (getline(&buf, &len, fp) <= 0) {
		found = 0;
		goto done_ret;
	}

	found = !strcmp(buf, line);

done_ret:
	free(buf);
	fclose(fp);
	return found;
}

TEST(cgroup_new_with_parent) {
	struct container_cgroup *ccg = nullptr;

	char *cgroup_root = nullptr;
	const char *cgroup_parent_name;
	const char *cgroup_name;
	char cpu_cg[256];
	char cpuacct_cg[256];
	char cpuset_cg[256];
	char devices_cg[256];
	char freezer_cg[256];
	char schedtune_cg[256];

	char temp_template[] = "/tmp/cgtestXXXXXX";
	char path[256];

	cgroup_root = strdup(mkdtemp(temp_template));
	snprintf(path, sizeof(path), "rm -rf %s/*", cgroup_root);
	EXPECT_EQ(0, system(path));

	snprintf(path, sizeof(path), "%s/cpu", cgroup_root);
	mkdir(path, S_IRWXU | S_IRWXG);
	snprintf(path, sizeof(path), "%s/cpuacct", cgroup_root);
	mkdir(path, S_IRWXU | S_IRWXG);
	snprintf(path, sizeof(path), "%s/cpuset", cgroup_root);
	mkdir(path, S_IRWXU | S_IRWXG);
	snprintf(path, sizeof(path), "%s/devices", cgroup_root);
	mkdir(path, S_IRWXU | S_IRWXG);
	snprintf(path, sizeof(path), "%s/freezer", cgroup_root);
	mkdir(path, S_IRWXU | S_IRWXG);
	snprintf(path, sizeof(path), "%s/schedtune", cgroup_root);
	mkdir(path, S_IRWXU | S_IRWXG);

	cgroup_parent_name = CGPARENTNAME;

	snprintf(path, sizeof(path), "%s/cpu/%s", cgroup_root,
		 cgroup_parent_name);
	mkdir(path, S_IRWXU | S_IRWXG);
	snprintf(path, sizeof(path), "%s/cpuacct/%s", cgroup_root,
		 cgroup_parent_name);
	mkdir(path, S_IRWXU | S_IRWXG);
	snprintf(path, sizeof(path), "%s/cpuset/%s", cgroup_root,
		 cgroup_parent_name);
	mkdir(path, S_IRWXU | S_IRWXG);
	snprintf(path, sizeof(path), "%s/devices/%s", cgroup_root,
		 cgroup_parent_name);
	mkdir(path, S_IRWXU | S_IRWXG);
	snprintf(path, sizeof(path), "%s/freezer/%s", cgroup_root,
		 cgroup_parent_name);
	mkdir(path, S_IRWXU | S_IRWXG);
	snprintf(path, sizeof(path), "%s/schedtune/%s", cgroup_root,
		 cgroup_parent_name);
	mkdir(path, S_IRWXU | S_IRWXG);

	snprintf(path, sizeof(path), "%s/cpuset/%s/cpus",
		 cgroup_root, cgroup_parent_name);
	create_file_with_content(path, "0-3");
	snprintf(path, sizeof(path), "%s/cpuset/%s/mems",
		 cgroup_root, cgroup_parent_name);
	create_file_with_content(path, "0");

	ccg = container_cgroup_new(CGNAME, cgroup_root, cgroup_parent_name,
				   1000, 1000);
	ASSERT_NE(nullptr, ccg);

	cgroup_name = CGNAME;

	snprintf(cpu_cg, sizeof(cpu_cg), "%s/cpu/%s/%s",
		 cgroup_root, cgroup_parent_name, cgroup_name);
	snprintf(cpuacct_cg, sizeof(cpuacct_cg), "%s/cpuacct/%s/%s",
		 cgroup_root, cgroup_parent_name, cgroup_name);
	snprintf(cpuset_cg, sizeof(cpuset_cg), "%s/cpuset/%s/%s",
		 cgroup_root, cgroup_parent_name, cgroup_name);
	snprintf(devices_cg, sizeof(devices_cg), "%s/devices/%s/%s",
		 cgroup_root, cgroup_parent_name, cgroup_name);
	snprintf(freezer_cg, sizeof(freezer_cg), "%s/freezer/%s/%s",
		 cgroup_root, cgroup_parent_name, cgroup_name);
	snprintf(schedtune_cg, sizeof(schedtune_cg), "%s/schedtune/%s/%s",
		 cgroup_root, cgroup_parent_name, cgroup_name);

	EXPECT_TRUE(check_is_dir(cpu_cg));
	EXPECT_TRUE(check_is_dir(cpuacct_cg));
	EXPECT_TRUE(check_is_dir(cpuset_cg));
	EXPECT_TRUE(check_is_dir(devices_cg));
	EXPECT_TRUE(check_is_dir(freezer_cg));
	EXPECT_TRUE(check_is_dir(schedtune_cg));

	char cmd[256];

	snprintf(cmd, sizeof(cmd), "rm -rf %s/*", cgroup_root);

	free(cgroup_root);
	container_cgroup_destroy(ccg);

	EXPECT_EQ(0, system(cmd));
}

FIXTURE(basic_manipulation) {
	struct container_cgroup *ccg;

	char *cgroup_root;
	const char *cgroup_name;
	char cpu_cg[256];
	char cpuacct_cg[256];
	char cpuset_cg[256];
	char devices_cg[256];
	char freezer_cg[256];
	char schedtune_cg[256];
};

FIXTURE_SETUP(basic_manipulation) {
	char temp_template[] = "/tmp/cgtestXXXXXX";
	char path[256];

	self->cgroup_root = strdup(mkdtemp(temp_template));
	snprintf(path, sizeof(path), "rm -rf %s/*", self->cgroup_root);
	EXPECT_EQ(0, system(path));

	snprintf(path, sizeof(path), "%s/cpu", self->cgroup_root);
	mkdir(path, S_IRWXU | S_IRWXG);
	snprintf(path, sizeof(path), "%s/cpuacct", self->cgroup_root);
	mkdir(path, S_IRWXU | S_IRWXG);
	snprintf(path, sizeof(path), "%s/cpuset", self->cgroup_root);
	mkdir(path, S_IRWXU | S_IRWXG);
	snprintf(path, sizeof(path), "%s/devices", self->cgroup_root);
	mkdir(path, S_IRWXU | S_IRWXG);
	snprintf(path, sizeof(path), "%s/freezer", self->cgroup_root);
	mkdir(path, S_IRWXU | S_IRWXG);
	snprintf(path, sizeof(path), "%s/schedtune", self->cgroup_root);
	mkdir(path, S_IRWXU | S_IRWXG);

	snprintf(path, sizeof(path), "%s/cpuset/cpus", self->cgroup_root);
	create_file_with_content(path, "0-3");
	snprintf(path, sizeof(path), "%s/cpuset/mems", self->cgroup_root);
	create_file_with_content(path, "0");

	self->ccg = container_cgroup_new(CGNAME, self->cgroup_root, nullptr, 0, 0);
	ASSERT_NE(nullptr, self->ccg);

	self->cgroup_name = CGNAME;

	snprintf(self->cpu_cg, sizeof(self->cpu_cg), "%s/cpu/%s",
		 self->cgroup_root, self->cgroup_name);
	snprintf(self->cpuacct_cg, sizeof(self->cpuacct_cg), "%s/cpuacct/%s",
		 self->cgroup_root, self->cgroup_name);
	snprintf(self->cpuset_cg, sizeof(self->cpuset_cg), "%s/cpuset/%s",
		 self->cgroup_root, self->cgroup_name);
	snprintf(self->devices_cg, sizeof(self->devices_cg), "%s/devices/%s",
		 self->cgroup_root, self->cgroup_name);
	snprintf(self->freezer_cg, sizeof(self->freezer_cg), "%s/freezer/%s",
		 self->cgroup_root, self->cgroup_name);
	snprintf(self->schedtune_cg, sizeof(self->schedtune_cg), "%s/schedtune/%s",
		 self->cgroup_root, self->cgroup_name);

	EXPECT_TRUE(check_is_dir(self->cpu_cg));
	EXPECT_TRUE(check_is_dir(self->cpuacct_cg));
	EXPECT_TRUE(check_is_dir(self->cpuset_cg));
	EXPECT_TRUE(check_is_dir(self->devices_cg));
	EXPECT_TRUE(check_is_dir(self->freezer_cg));
	EXPECT_TRUE(check_is_dir(self->schedtune_cg));

	snprintf(path, sizeof(path), "%s/tasks", self->cpu_cg);
	create_file(path);
	snprintf(path, sizeof(path), "%s/cpu.shares", self->cpu_cg);
	create_file(path);
	snprintf(path, sizeof(path), "%s/cpu.cfs_quota_us", self->cpu_cg);
	create_file(path);
	snprintf(path, sizeof(path), "%s/cpu.cfs_period_us", self->cpu_cg);
	create_file(path);
	snprintf(path, sizeof(path), "%s/cpu.rt_runtime_us", self->cpu_cg);
	create_file(path);
	snprintf(path, sizeof(path), "%s/cpu.rt_period_us", self->cpu_cg);
	create_file(path);
	snprintf(path, sizeof(path), "%s/tasks", self->cpuacct_cg);
	create_file(path);
	snprintf(path, sizeof(path), "%s/tasks", self->cpuset_cg);
	create_file(path);
	snprintf(path, sizeof(path), "%s/tasks", self->devices_cg);
	create_file(path);
	snprintf(path, sizeof(path), "%s/devices.allow", self->devices_cg);
	create_file(path);
	snprintf(path, sizeof(path), "%s/devices.deny", self->devices_cg);
	create_file(path);
	snprintf(path, sizeof(path), "%s/tasks", self->freezer_cg);
	create_file(path);
	snprintf(path, sizeof(path), "%s/freezer.state", self->freezer_cg);
	create_file(path);
	snprintf(path, sizeof(path), "%s/tasks", self->schedtune_cg);
	create_file(path);
}

FIXTURE_TEARDOWN(basic_manipulation) {
	char cmd[256];

	snprintf(cmd, sizeof(cmd), "rm -rf %s/*", self->cgroup_root);

	free(self->cgroup_root);
	container_cgroup_destroy(self->ccg);

	EXPECT_EQ(0, system(cmd));
}

TEST_F(basic_manipulation, freeze) {
	char path[256];
	EXPECT_EQ(0, self->ccg->ops->freeze(self->ccg));
	snprintf(path, sizeof(path), "%s/freezer.state", self->freezer_cg);
	EXPECT_TRUE(string_in_file(path, "FROZEN"));
}

TEST_F(basic_manipulation, thaw) {
	char path[256];
	EXPECT_EQ(0, self->ccg->ops->thaw(self->ccg));
	snprintf(path, sizeof(path), "%s/freezer.state", self->freezer_cg);
	EXPECT_TRUE(string_in_file(path, "THAWED"));
}

TEST_F(basic_manipulation, default_all_devs_disallow) {
	char path[256];
	ASSERT_EQ(0, self->ccg->ops->deny_all_devices(self->ccg));
	snprintf(path, sizeof(path), "%s/devices.deny", self->devices_cg);
	EXPECT_TRUE(file_has_line(path, "a\n"));
}

TEST_F(basic_manipulation, add_device_invalid_type) {
	EXPECT_NE(0, self->ccg->ops->add_device(self->ccg, 1, 14, 3, 1, 1, 0,
						'x'));
}

TEST_F(basic_manipulation, add_device_no_perms) {
	EXPECT_NE(0, self->ccg->ops->add_device(self->ccg, 1, 14, 3, 0, 0, 0,
						'c'));
}

TEST_F(basic_manipulation, add_device_rw) {
	char path[256];
	EXPECT_EQ(0, self->ccg->ops->add_device(self->ccg, 1, 14, 3, 1, 1, 0,
						'c'));
	snprintf(path, sizeof(path), "%s/devices.allow", self->devices_cg);
	EXPECT_TRUE(file_has_line(path, "c 14:3 rw\n"));
}

TEST_F(basic_manipulation, add_device_rwm) {
	char path[256];
	EXPECT_EQ(0, self->ccg->ops->add_device(self->ccg, 1, 14, 3, 1, 1, 1,
						'c'));
	snprintf(path, sizeof(path), "%s/devices.allow", self->devices_cg);
	EXPECT_TRUE(file_has_line(path, "c 14:3 rwm\n"));
}

TEST_F(basic_manipulation, add_device_ro) {
	char path[256];
	EXPECT_EQ(0, self->ccg->ops->add_device(self->ccg, 1, 14, 3, 1, 0, 0,
						'c'));
	snprintf(path, sizeof(path), "%s/devices.allow", self->devices_cg);
	EXPECT_TRUE(file_has_line(path, "c 14:3 r\n"));
}

TEST_F(basic_manipulation, add_device_wo) {
	char path[256];
	EXPECT_EQ(0, self->ccg->ops->add_device(self->ccg, 1, 14, 3, 0, 1, 0,
						'c'));
	snprintf(path, sizeof(path), "%s/devices.allow", self->devices_cg);
	EXPECT_TRUE(file_has_line(path, "c 14:3 w\n"));
}

TEST_F(basic_manipulation, add_device_major_wide) {
	char path[256];
	EXPECT_EQ(0, self->ccg->ops->add_device(self->ccg, 1, 14, -1, 0, 1, 0,
						'c'));
	snprintf(path, sizeof(path), "%s/devices.allow", self->devices_cg);
	EXPECT_TRUE(file_has_line(path, "c 14:* w\n"));
}

TEST_F(basic_manipulation, add_device_major_minor_wildcard) {
	char path[256];
	EXPECT_EQ(0, self->ccg->ops->add_device(self->ccg, 1, -1, -1, 0, 1, 0,
						'c'));
	snprintf(path, sizeof(path), "%s/devices.allow", self->devices_cg);
	EXPECT_TRUE(file_has_line(path, "c *:* w\n"));
}

TEST_F(basic_manipulation, add_device_deny_all) {
	char path[256];
	EXPECT_EQ(0, self->ccg->ops->add_device(self->ccg, 0, -1, -1, 1, 1, 1,
						'a'));
	snprintf(path, sizeof(path), "%s/devices.deny", self->devices_cg);
	EXPECT_TRUE(file_has_line(path, "a *:* rwm\n"));
}

TEST_F(basic_manipulation, add_device_block) {
	char path[256];
	EXPECT_EQ(0, self->ccg->ops->add_device(self->ccg, 1, 14, 3, 1, 1, 0,
						'b'));
	snprintf(path, sizeof(path), "%s/devices.allow", self->devices_cg);
	EXPECT_TRUE(file_has_line(path, "b 14:3 rw\n"));
}

TEST_F(basic_manipulation, set_cpu_shares) {
	char path[256];
	EXPECT_EQ(0, self->ccg->ops->set_cpu_shares(self->ccg, 500));
	snprintf(path, sizeof(path), "%s/cpu.shares", self->cpu_cg);
	EXPECT_TRUE(string_in_file(path, "500"));
}

TEST_F(basic_manipulation, set_cpu_quota) {
	char path[256];
	EXPECT_EQ(0, self->ccg->ops->set_cpu_quota(self->ccg, 200000));
	snprintf(path, sizeof(path), "%s/cpu.cfs_quota_us", self->cpu_cg);
	EXPECT_TRUE(string_in_file(path, "200000"));
}

TEST_F(basic_manipulation, set_cpu_period) {
	char path[256];
	EXPECT_EQ(0, self->ccg->ops->set_cpu_period(self->ccg, 800000));
	snprintf(path, sizeof(path), "%s/cpu.cfs_period_us", self->cpu_cg);
	EXPECT_TRUE(string_in_file(path, "800000"));
}

TEST_F(basic_manipulation, set_cpu_rt_runtime) {
	char path[256];
	EXPECT_EQ(0, self->ccg->ops->set_cpu_rt_runtime(self->ccg, 100000));
	snprintf(path, sizeof(path), "%s/cpu.rt_runtime_us", self->cpu_cg);
	EXPECT_TRUE(string_in_file(path, "100000"));
}

TEST_F(basic_manipulation, set_cpu_rt_period) {
	char path[256];
	EXPECT_EQ(0, self->ccg->ops->set_cpu_rt_period(self->ccg, 500000));
	snprintf(path, sizeof(path), "%s/cpu.rt_period_us", self->cpu_cg);
	EXPECT_TRUE(string_in_file(path, "500000"));
}

TEST_HARNESS_MAIN
