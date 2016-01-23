all: libcontainer_unittest container_cgroup_unittest

libcontainer_unittest: libcontainer.c libcontainer_unittest.c libcontainer.h
	gcc -g -o libcontainer_unittest -I../../aosp/external/minijail/ -I../../..//platform2/ libcontainer.c libcontainer_unittest.c

container_cgroup_unittest: container_cgroup.c container_cgroup_unittest.c container_cgroup.h
	gcc -g -o container_cgroup_unittest -I../../../platform2/ container_cgroup.c container_cgroup_unittest.c
