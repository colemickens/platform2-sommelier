/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Handles setting basic cgroup properties.  The format of the cgroup files can
 * be found in the linux kernel at Documentation/cgroups/.
 */

#ifndef CONTAINER_MANAGER_CGROUP_H_
#define CONTAINER_MANAGER_CGROUP_H_

#include <sys/types.h>
#include <unistd.h>

enum container_cgroup_types {
	CGROUP_CPU,
	CGROUP_CPUACCT,
	CGROUP_DEVICES,
	CGROUP_FREEZER,
	NUM_CGROUP_TYPES
};

struct container_cgroup;

struct cgroup_ops {
	int (*freeze)(const struct container_cgroup *cg);
	int (*thaw)(const struct container_cgroup *cg);
	int (*deny_all_devices)(const struct container_cgroup *cg);
	int (*add_device)(const struct container_cgroup *cg, int major,
			  int minor, int read, int write, int modify,
			  char type);
	int (*set_cpu_shares)(const struct container_cgroup *cg, int shares);
	int (*set_cpu_quota)(const struct container_cgroup *cg, int quota);
	int (*set_cpu_period)(const struct container_cgroup *cg, int period);
	int (*set_cpu_rt_runtime)(const struct container_cgroup *cg,
				  int rt_runtime);
	int (*set_cpu_rt_period)(const struct container_cgroup *cg,
				 int rt_period);
};

struct container_cgroup {
	const struct cgroup_ops *ops;
	char *name;
	char *cgroup_paths[NUM_CGROUP_TYPES];
	char *cgroup_tasks_paths[NUM_CGROUP_TYPES];
};

struct container_cgroup *container_cgroup_new(const char *name,
					      const char *cgroup_root,
					      const char *cgroup_parent);
void container_cgroup_destroy(struct container_cgroup *);

static inline const char *cgroup_cpu_tasks_path(
		const struct container_cgroup *cg)
{
	return cg->cgroup_tasks_paths[CGROUP_CPU];
}

static inline const char *cgroup_cpuacct_tasks_path(
		const struct container_cgroup *cg)
{
	return cg->cgroup_tasks_paths[CGROUP_CPUACCT];
}

static inline const char *cgroup_devices_tasks_path(
		const struct container_cgroup *cg)
{
	return cg->cgroup_tasks_paths[CGROUP_DEVICES];
}

static inline const char *cgroup_freezer_tasks_path(
		const struct container_cgroup *cg)
{
	return cg->cgroup_tasks_paths[CGROUP_FREEZER];
}

#endif  /* CONTAINER_MANAGER_CGROUP_H_ */
