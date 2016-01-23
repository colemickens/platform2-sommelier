/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define _GNU_SOURCE /* For asprintf */

#include <errno.h>
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

static FILE *open_cgroup_file(const char *cgroup_path, const char *name)
{
	FILE *fp;
	char *path = NULL;

	if (asprintf(&path, "%s/%s", cgroup_path, name) < 0)
		return NULL;

	fp = fopen(path, "w");
	free(path);
	return fp;
}

static int write_cgroup_file(const char *cgroup_path, const char *name,
			     const char *str)
{
	FILE *fp;

	fp = open_cgroup_file(cgroup_path, name);
	if (!fp)
		return -errno;
	fputs(str, fp);
	fclose(fp);
	return 0;
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

static const struct cgroup_ops cgroup_ops = {
	.freeze = freeze,
	.thaw = thaw,
	.deny_all_devices = deny_all_devices,
	.add_device = add_device,
};


struct container_cgroup *container_cgroup_new(const char *name,
					      const char *cgroup_root)
{
	int i;
	struct container_cgroup *cg;

	cg = calloc(1, sizeof(*cg));
	if (!cg)
		return NULL;

	for (i = 0; i < NUM_CGROUP_TYPES; ++i) {
		if (asprintf(&cg->cgroup_paths[i], "%s/%s/%s", cgroup_root,
			     cgroup_names[i], name) < 0)
			goto error_free_cg;

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

