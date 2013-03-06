// Copied from kernel sources. See COPYING for license details.

#ifndef PERF_INTERNALS_H_
#define PERF_INTERNALS_H_

#include <linux/limits.h>
#include <stdint.h>

#include "perf_event.h"

// These typedefs are from tools/perf/util/types.h in the kernel.
typedef uint64_t           u64;
typedef int64_t            s64;
typedef unsigned int	   u32;
typedef signed int	   s32;
typedef unsigned short	   u16;
typedef signed short	   s16;
typedef unsigned char	   u8;
typedef signed char	   s8;

#define BITS_PER_BYTE           8
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define BITS_TO_LONGS(nr)       DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(long))

#define DECLARE_BITMAP(name,bits) \
	unsigned long name[BITS_TO_LONGS(bits)]

#define MAX_EVENT_NAME 64

// These data structures have been copied from the kernel. See files under
// tools/perf/util.

enum {
	HEADER_RESERVED		= 0,	/* always cleared */
	HEADER_FIRST_FEATURE	= 1,
	HEADER_TRACE_INFO	= 1,
	HEADER_BUILD_ID,

	HEADER_HOSTNAME,
	HEADER_OSRELEASE,
	HEADER_VERSION,
	HEADER_ARCH,
	HEADER_NRCPUS,
	HEADER_CPUDESC,
	HEADER_CPUID,
	HEADER_TOTAL_MEM,
	HEADER_CMDLINE,
	HEADER_EVENT_DESC,
	HEADER_CPU_TOPOLOGY,
	HEADER_NUMA_TOPOLOGY,
	HEADER_BRANCH_STACK,
	HEADER_LAST_FEATURE,
	HEADER_FEAT_BITS	= 256,
};

struct perf_file_section {
	u64 offset;
	u64 size;
};

struct perf_file_attr {
	struct perf_event_attr	attr;
	struct perf_file_section	ids;
};

struct perf_trace_event_type {
  u64     event_id;
  char    name[MAX_EVENT_NAME];
};

struct perf_file_header {
	u64				magic;
	u64				size;
	u64				attr_size;
	struct perf_file_section	attrs;
	struct perf_file_section	data;
	struct perf_file_section	event_types;
	DECLARE_BITMAP(adds_features, HEADER_FEAT_BITS);
};

enum {
	SHOW_KERNEL	= 1,
	SHOW_USER	= 2,
	SHOW_HV		= 4,
};

/*
 * PERF_SAMPLE_IP | PERF_SAMPLE_TID | *
 */
struct ip_event {
	struct perf_event_header header;
	u64 ip;
	u32 pid, tid;
	unsigned char __more_data[];
};

struct mmap_event {
	struct perf_event_header header;
	u32 pid, tid;
	u64 start;
	u64 len;
	u64 pgoff;
	char filename[PATH_MAX];
};

struct comm_event {
	struct perf_event_header header;
	u32 pid, tid;
	char comm[16];
};

struct fork_event {
	struct perf_event_header header;
	u32 pid, ppid;
	u32 tid, ptid;
	u64 time;
};

struct lost_event {
	struct perf_event_header header;
	u64 id;
	u64 lost;
};

/*
 * PERF_FORMAT_ENABLED | PERF_FORMAT_RUNNING | PERF_FORMAT_ID
 */
struct read_event {
	struct perf_event_header header;
	u32 pid, tid;
	u64 value;
	u64 time_enabled;
	u64 time_running;
	u64 id;
};

struct sample_event{
	struct perf_event_header        header;
	u64 array[];
};

typedef union event_union {
	struct perf_event_header	header;
	struct ip_event			ip;
	struct mmap_event		mmap;
	struct comm_event		comm;
	struct fork_event		fork;
	struct lost_event		lost;
	struct read_event		read;
	struct sample_event		sample;
} event_t;

// All the possible fields of a perf sample.  This is not an actual data
// structure found in raw perf data, as each field may or may not be present in
// the data.
struct perf_sample {
	u64 ip;
	u32 pid, tid;
	u64 time;
	u64 addr;
	u64 id;
	u64 stream_id;
	u64 period;
	u32 cpu;
	u32 raw_size;
	void *raw_data;
	struct ip_callchain *callchain;
	struct branch_stack *branch_stack;
};

// End data structures copied from the kernel.


#endif /*PERF_INTERNALS_H_*/
