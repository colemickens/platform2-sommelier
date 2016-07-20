/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define _GNU_SOURCE /* For asprintf */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "container_cgroup.h"

static const char *cgroup_names[NUM_CGROUP_TYPES] = {
	"cpu", "cpuacct", "devices", "freezer",
};

static int open_cgroup_file(const char *cgroup_path, const char *name)
{
	int fd;
	char *path = NULL;

	if (asprintf(&path, "%s/%s", cgroup_path, name) < 0)
		return -errno;

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0664);
	if (fd == -1)
		fd = -errno;
	free(path);
	return fd;
}

static int write_cgroup_file(const char *cgroup_path, const char *name,
			     const char *str)
{
	int fd;
	int rc = 0;

	fd = open_cgroup_file(cgroup_path, name);
	if (fd < 0)
		return fd;
	const char *buffer = str;
	size_t len = strlen(str);
	if (write(fd, buffer, len) != len)
		rc = -errno;
	close(fd);
	return rc;
}

static int write_cgroup_file_int(const char *cgroup_path, const char *name,
				 const int value)
{
	char *str = NULL;
	int rc;

	if (asprintf(&str, "%d", value) < 0)
		return -errno;

	rc = write_cgroup_file(cgroup_path, name, str);
	free(str);
	return rc;
}

static int freeze(const struct container_cgroup *cg)
{
	return write_cgroup_file(cg->cgroup_paths[CGROUP_FREEZER],
				 "freezer.state", "FROZEN\n");
}

static int thaw(const struct container_cgroup *cg)
{
	return write_cgroup_file(cg->cgroup_paths[CGROUP_FREEZER],
				 "freezer.state", "THAWED\n");
}

static int deny_all_devices(const struct container_cgroup *cg)
{
	return write_cgroup_file(cg->cgroup_paths[CGROUP_DEVICES],
				 "devices.deny", "a\n");
}

static int add_device(const struct container_cgroup *cg, int major, int minor,
		      int read, int write, int modify, char type)
{
	char *perm_string = NULL;
	int rc;

	if (type != 'b' && type != 'c')
		return -EINVAL;
	if (!read && !write)
		return -EINVAL;

	if (minor >= 0) {
		if (asprintf(&perm_string, "%c %d:%d %s%s%s\n",
			     type, major, minor,
			     read ? "r" : "", write ? "w" : "",
			     modify ? "m" : "") < 0)
			return -errno;
	} else {
		/* Set perms for all devices with this major number. */
		if (asprintf(&perm_string, "%c %d:* %s%s%s\n",
			     type, major,
			     read ? "r" : "", write ? "w" : "",
			     modify ? "m" : "") < 0)
			return -errno;
	}
	rc = write_cgroup_file(cg->cgroup_paths[CGROUP_DEVICES],
			      "devices.allow", perm_string);
	free(perm_string);
	return rc;
}

static int set_cpu_shares(const struct container_cgroup *cg, int shares)
{
	return write_cgroup_file_int(cg->cgroup_paths[CGROUP_CPU],
				     "cpu.shares", shares);
}

static int set_cpu_quota(const struct container_cgroup *cg, int quota)
{
	return write_cgroup_file_int(cg->cgroup_paths[CGROUP_CPU],
				     "cpu.cfs_quota_us", quota);
}

static int set_cpu_period(const struct container_cgroup *cg, int period)
{
	return write_cgroup_file_int(cg->cgroup_paths[CGROUP_CPU],
				     "cpu.cfs_period_us", period);
}

static int set_cpu_rt_runtime(const struct container_cgroup *cg, int rt_runtime)
{
	return write_cgroup_file_int(cg->cgroup_paths[CGROUP_CPU],
				     "cpu.rt_runtime_us", rt_runtime);
}

static int set_cpu_rt_period(const struct container_cgroup *cg, int rt_period)
{
	return write_cgroup_file_int(cg->cgroup_paths[CGROUP_CPU],
				     "cpu.rt_period_us", rt_period);
}

static const struct cgroup_ops cgroup_ops = {
	.freeze = freeze,
	.thaw = thaw,
	.deny_all_devices = deny_all_devices,
	.add_device = add_device,
	.set_cpu_shares = set_cpu_shares,
	.set_cpu_quota = set_cpu_quota,
	.set_cpu_period = set_cpu_period,
	.set_cpu_rt_runtime = set_cpu_rt_runtime,
	.set_cpu_rt_period = set_cpu_rt_period,
};


struct container_cgroup *container_cgroup_new(const char *name,
					      const char *cgroup_root,
					      const char *cgroup_parent)
{
	int i;
	struct container_cgroup *cg;

	cg = calloc(1, sizeof(*cg));
	if (!cg)
		return NULL;

	for (i = 0; i < NUM_CGROUP_TYPES; ++i) {
		if (cgroup_parent) {
			if (asprintf(&cg->cgroup_paths[i], "%s/%s/%s/%s",
				     cgroup_root, cgroup_names[i],
				     cgroup_parent, name) < 0)
				goto error_free_cg;
		} else {
			if (asprintf(&cg->cgroup_paths[i], "%s/%s/%s",
				     cgroup_root, cgroup_names[i], name) < 0)
				goto error_free_cg;
		}

		if (mkdir(cg->cgroup_paths[i], S_IRWXU | S_IRWXG) < 0 &&
		    errno != EEXIST)
			goto error_free_cg;

		if (asprintf(&cg->cgroup_tasks_paths[i], "%s/tasks",
			     cg->cgroup_paths[i]) < 0)
			goto error_free_cg;
	}

	cg->name = strdup(name);
	if (!cg->name)
		goto error_free_cg;
	cg->ops = &cgroup_ops;

	return cg;

error_free_cg:
	container_cgroup_destroy(cg);
	return NULL;
}

void container_cgroup_destroy(struct container_cgroup *cg)
{
	int i;

	free(cg->name);
	for (i = 0; i < NUM_CGROUP_TYPES; ++i) {
		if (!cg->cgroup_paths[i])
			continue;
		rmdir(cg->cgroup_paths[i]);
		free(cg->cgroup_paths[i]);
		free(cg->cgroup_tasks_paths[i]);
	}
	free(cg);
}

