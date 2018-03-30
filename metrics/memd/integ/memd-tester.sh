#! /bin/bash
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Test driver for memd.

set -e

# ----------------------
# SET UP AUXILIARY FILES
# ----------------------
#
# create fake directories under TESTING_ROOT

TR=./testing-root
rm -rf $TR
for dir in /proc \
           /sys/kernel/mm/chromeos-low_mem \
           /sys/kernel/debug/tracing \
           /dev \
           /proc/sys/vm \
           /var/log \
           ; do
  mkdir -p $TR/$dir
done

# fake /proc/vmstat
cat > $TR/proc/vmstat << EOF
nr_free_pages 50489
nr_alloc_batch 3326
nr_inactive_anon 48591
nr_active_anon 145673
nr_inactive_file 49597
nr_active_file 50199
nr_unevictable 0
nr_mlock 0
nr_anon_pages 180171
nr_mapped 63194
nr_file_pages 149074
nr_dirty 136
nr_writeback 0
nr_slab_reclaimable 10484
nr_slab_unreclaimable 16134
nr_page_table_pages 9255
nr_kernel_stack 979
nr_unstable 0
nr_bounce 0
nr_vmscan_write 60544150
nr_vmscan_immediate_reclaim 29587
nr_writeback_temp 0
nr_isolated_anon 0
nr_isolated_file 0
nr_shmem 8982
nr_dirtied 3909911
nr_written 64426230
nr_pages_scanned 88
workingset_refault 16082484
workingset_activate 1186339
workingset_nodereclaim 0
nr_anon_transparent_hugepages 2
nr_free_cma 0
nr_dirty_threshold 68740
nr_dirty_background_threshold 5728
pgpgin 41070493
pgpgout 16914684
pswpin 59158894
pswpout 60513438
pgalloc_dma 3247
pgalloc_dma32 131210265
pgalloc_normal 0
pgalloc_movable 0
pgfree 140534607
pgactivate 70428610
pgdeactivate 79401090
pgfault 110047495
pgmajfault 52561387
pgmajfault_s 97569
pgmajfault_a 52314090
pgmajfault_f 149728
pgrefill_dma 4749
pgrefill_dma32 79973171
pgrefill_normal 0
pgrefill_movable 0
pgsteal_kswapd_dma 112
pgsteal_kswapd_dma32 62450553
pgsteal_kswapd_normal 0
pgsteal_kswapd_movable 0
pgsteal_direct_dma 0
pgsteal_direct_dma32 18534641
pgsteal_direct_normal 0
pgsteal_direct_movable 0
pgscan_kswapd_dma 4776
pgscan_kswapd_dma32 126413953
pgscan_kswapd_normal 0
pgscan_kswapd_movable 0
pgscan_direct_dma 11
pgscan_direct_dma32 36676518
pgscan_direct_normal 0
pgscan_direct_movable 0
pgscan_direct_throttle 0
pginodesteal 3302
slabs_scanned 24535326
kswapd_inodesteal 237022
kswapd_low_wmark_hit_quickly 24998
kswapd_high_wmark_hit_quickly 40300
pageoutrun 87752
allocstall 376923
pgrotated 247987
drop_pagecache 0
drop_slab 0
pgmigrate_success 8814551
pgmigrate_fail 94348
compact_migrate_scanned 86192323
compact_free_scanned 1982032044
compact_isolated 19084449
compact_stall 20280
compact_fail 17323
compact_success 2957
unevictable_pgs_culled 0
unevictable_pgs_scanned 0
unevictable_pgs_rescued 0
unevictable_pgs_mlocked 0
unevictable_pgs_munlocked 0
unevictable_pgs_cleared 0
unevictable_pgs_stranded 0
thp_fault_alloc 15
thp_fault_fallback 0
thp_collapse_alloc 7894
thp_collapse_alloc_failed 4784
thp_split 13
thp_zero_page_alloc 0
thp_zero_page_alloc_failed 0
EOF

# fake /proc/zoneinfo
cat > $TR/proc/zoneinfo << EOF
Node 0, zone      DMA
  pages free     1756
        min      179
        low      223
        high     268
        scanned  0
        spanned  4095
        present  3999
        managed  3977
    nr_free_pages 1756
    nr_alloc_batch 45
    nr_inactive_anon 0
    nr_active_anon 0
    nr_inactive_file 238
    nr_active_file 377
    nr_unevictable 0
    nr_mlock     0
    nr_anon_pages 0
    nr_mapped    284
    nr_file_pages 615
    nr_dirty     0
    nr_writeback 0
    nr_slab_reclaimable 23
    nr_slab_unreclaimable 22
    nr_page_table_pages 2
    nr_kernel_stack 0
    nr_unstable  0
    nr_bounce    0
    nr_vmscan_write 112
    nr_vmscan_immediate_reclaim 0
    nr_writeback_temp 0
    nr_isolated_anon 0
    nr_isolated_file 0
    nr_shmem     0
    nr_dirtied   352
    nr_written   464
    nr_pages_scanned 0
    workingset_refault 0
    workingset_activate 0
    workingset_nodereclaim 0
    nr_anon_transparent_hugepages 0
    nr_free_cma  0
        protection: (0, 1928, 1928, 1928)
  pagesets
    cpu: 0
              count: 0
              high:  0
              batch: 1
  vm stats threshold: 4
    cpu: 1
              count: 0
              high:  0
              batch: 1
  vm stats threshold: 4
  all_unreclaimable: 1
  start_pfn:         1
  inactive_ratio:    1
Node 0, zone    DMA32
  pages free     56774
        min      22348
        low      27935
        high     33522
        scanned  0
        spanned  506914
        present  506658
        managed  494688
    nr_free_pages 56774
    nr_alloc_batch 3023
    nr_inactive_anon 48059
    nr_active_anon 138044
    nr_inactive_file 47952
    nr_active_file 49885
    nr_unevictable 0
    nr_mlock     0
    nr_anon_pages 170349
    nr_mapped    62610
    nr_file_pages 147990
    nr_dirty     231
    nr_writeback 0
    nr_slab_reclaimable 10414
    nr_slab_unreclaimable 16112
    nr_page_table_pages 9231
    nr_kernel_stack 981
    nr_unstable  0
    nr_bounce    0
    nr_vmscan_write 60554686
    nr_vmscan_immediate_reclaim 29587
    nr_writeback_temp 0
    nr_isolated_anon 0
    nr_isolated_file 0
    nr_shmem     8982
    nr_dirtied   3918108
    nr_written   64443856
    nr_pages_scanned 0
    workingset_refault 16084171
    workingset_activate 1186426
    workingset_nodereclaim 0
    nr_anon_transparent_hugepages 2
    nr_free_cma  0
        protection: (0, 0, 0, 0)
  pagesets
    cpu: 0
              count: 135
              high:  186
              batch: 31
  vm stats threshold: 20
    cpu: 1
              count: 166
              high:  186
              batch: 31
  vm stats threshold: 20
  all_unreclaimable: 0
  start_pfn:         4096
  inactive_ratio:    3
EOF

# Expected memd.parameters file, except the first line which is a time stamp.
EXPECTED_MEMD_PARAMETERS=$TR/expected.memd.parameters
cat > $EXPECTED_MEMD_PARAMETERS << EOF
margin 200
min_filelist_kbytes 100000
min_free_kbytes 80000
extra_free_kbytes 60000
min_water_mark_kbytes 90108
low_water_mark_kbytes 112632
high_water_mark_kbytes 135160
EOF

# Value for available RAM (MB) stored in "available" file during low pressure
high_available=1000
# Medium-pressure available RAM (fast poll).
low_available=300
# Below-margin available RAM.
below_margin_available=150
# Low-mem margin.
margin=200

# Set up other fake sysfs/procfs entries.
MARGIN_FILE=$TR/sys/kernel/mm/chromeos-low_mem/margin
echo $margin > $MARGIN_FILE
echo "0.16 0.18 0.22 4/981 8504" > $TR/proc/loadavg
echo 1 > $TR/sys/kernel/debug/tracing/tracing_enabled
echo 1 > $TR/sys/kernel/debug/tracing/tracing_on
echo -n > $TR/sys/kernel/debug/tracing/set_ftrace_filter
echo -n > $TR/sys/kernel/debug/tracing/current_tracer

echo 100000 > $TR/proc/sys/vm/min_filelist_kbytes
echo 80000 > $TR/proc/sys/vm/min_free_kbytes
echo 60000 > $TR/proc/sys/vm/extra_free_kbytes

TRACE_PIPE=$TR/sys/kernel/debug/tracing/trace_pipe
mkfifo $TRACE_PIPE

LOW_MEM_DEVICE=$TR/dev/chromeos-low-mem
mkfifo $LOW_MEM_DEVICE

# clean up /var/log
rm -rf /var/log/memd/*

# ---------
# FUNCTIONS
# ---------

function log() {
  echo "memd-tester.sh: $*"
}

function die() {
  message="memd-tester.sh: $*"
  echo $message
  exit 1
}

function abs() {
  echo ${1#-}
}

# Returns success status if $1 and $2 don't differ by more than $3.
function roughly_equal() {
  diff=$(abs $(($1 - $2)))
  [ $diff -lt $3 ]
}

# Returns $1 rounded up to next multiple of $2.
function round_up() {
  if [ $(($1 % $2)) -eq 0 ]; then
    echo $1
  else
    echo $(($1 / $2 * $2 + $2))
  fi
}

# Sleep until uptime $1 (relative to start of test) is reached.
function sleep_until() {
  sleep $(($1 - T_CURRENT_RELATIVE_UPTIME))
  T_CURRENT_RELATIVE_UPTIME=$1
}

# Impersonates Chrome sending a signal.  Assumes D-Bus is available in the test
# environment.  (If not, we should use another fifo to fake it.)
function send_signal() {
  local type=$1
    dbus-send --system --type=signal / org.chromium.EventLogger.ChromeEvent \
              string:$type
}

# Returns the uptime in centiseconds.  Assumes /proc/uptime uses 2 decimals.
function uptime_centiseconds() {
  cat /proc/uptime | sed -e 's/ .*//' -e 's/\.//'
}

# Returns the uptime.
function uptime_seconds() {
  cat /proc/uptime | sed -e 's/ .*//'
}

# Takes a clip number and checks that the number of variables in the header
# matches the number of values in each sample.
function check_clip_format() {
  local clip_file=$1
  [ -e $clip_file ] || die "file $clip_file is missing"
  # The first line is a time stamp (we don't check it).  The second line is a
  # list of names for the values in the following lines.
  header_count=$(sed -n 2p $clip_file | wc -w)
  # Skip the first two lines.
  sed -n '3,$p' $clip_file | while read line; do
    if [ $(echo "$line" | wc -w) -ne $header_count ]; then
      log "mismatch: expected $header_count words in this line:"
      log "$line"
    fi
  done
}

# The purpose of this is to ensure that there is always a valid integer in the
# target file.  Simply executing 'echo $content > $target' truncates $target
# first, so there is race which may cause a reader expecting a valid integer
# finds an empty file instead.  By using "dd" we atomically overwrite the
# previous content.  The reader applies trim() to the file content, thus we can
# append with spaces to mask any difference in the number of digits.
#
# The atomic file rename trick will not work here because the reader is keeping
# the file open, thus the reader's fd would keep pointing to the removed file.
function update_sysfs_file() {
  content="$1"
  target="$2"
  echo "$content      " | dd of="${target}" conv=notrunc status=none
}

# Update the available sysfs entry, with the possible side effect of making the
# low-mem device ready to read.
function update_available() {
  amount="$1"
  update_sysfs_file $amount "$TR/sys/kernel/mm/chromeos-low_mem/available"
  if [ $amount -le "$(cat $MARGIN_FILE)" ]; then
    # The fifo writer blocks until a reader comes along.
    log "Signaling low-mem (amount = $amount)"
    (echo "anything" > $LOW_MEM_DEVICE) &
  else
    log "Clearing low-mem (amount = $amount)"
    # Non-blocking flush.  Don't print dd info messages.
    dd if=$LOW_MEM_DEVICE of=/dev/null iflag=nonblock status=none
  fi
}

# Simulates a kernel OOM notification.
function kernel_oom_notify {
  local message="        chrome-13700 [001] .... $(uptime_seconds)"
  message="${message}: oom_kill_process <-out_of_memory"
  (echo "${message}" > $TRACE_PIPE) &
}

# Returns clip filename for the given clip number.
function clip_filename {
  printf "$TR/var/log/memd/memd.clip%03d.log" $1
}


# Takes a clip number and returns the uptime (in centiseconds) of the first and
# last samples, and the sample count.
function get_clip_data() {
  local clip=$(clip_filename $1)
  local start_time=$(sed -n 3p $clip | sed -e 's/\.//' -e 's/ .*//')
  local end_time=$(sed -n '$p' $clip | sed -e 's/\.//' -e 's/ .*//')
  # The header consists of the first two lines.
  local count=$(( $(cat $clip | wc -l) - 2 ))
  echo $start_time $end_time $count
}

# Checks the validity of the content of a clip file.
function check_clip() {
  local clip_file=$(clip_filename $1)
  log "checking clip $clip_file"
  # All times are in centiseconds.
  local expected_start_time=$2
  local expected_end_time=$3
  local expected_event_count=$4

  check_clip_format $clip_file

  local found_start_time=$(sed -n 3p $clip_file | sed -e 's/\.//' -e 's/ .*//')
  local found_end_time=$(sed -n '$p' $clip_file | sed -e 's/\.//' -e 's/ .*//')

  # When checking times, allow 10 centiseconds of slack from quantization, and
  # 10 more of drift.
  slack=20

  roughly_equal $expected_start_time $found_start_time $slack || \
    die "$clip_file: start time: expected $expected_start_time," \
        "got $found_start_time"

  roughly_equal $expected_end_time $found_end_time $slack || \
    die "$clip_file: end time: expected $expected_end_time," \
        "got $found_end_time"

  # Check number of samples. Subtract 2 for header lines.
  local found_sample_count=$(( $(cat $clip_file | wc -l) - 2 ))

  # If there were no events, we'd expect 10 samples/second (actually a tiny bit
  # less since memd allows drift).  Then the total number of samples would be
  # 10 per second, plus the event samples.  But whenever an event comes in, the
  # following sample is delayed by an amount between zero and one period.  So
  # there is an uncertainty equal to the number of events.
  exp_sample_count=$(((expected_end_time - expected_start_time) / 10))
  # Allow losing one sample to drift.
  min_exp_sample_count=$((exp_sample_count - 1))
  max_exp_sample_count=$((exp_sample_count + expected_event_count * 2))
  [ $found_sample_count -lt $min_exp_sample_count ] || \
  [ $found_sample_count -gt $max_exp_sample_count ] && \
    die "$clip_file: sample_count: expected $min_exp_sample_count" \
        "to $max_exp_sample_count, got $found_sample_count"

  found_event_count=$(sed -n '3,$p' $clip_file | grep -v timer | wc -l)
  [ $found_event_count -eq $expected_event_count ] || \
    die "$clip_file: event_count: expected $expected_event_count," \
        "found $found_event_count"
}

# Runs a test as specified by the first argument.  See "DESCRIPTORS" below for
# the syntax of a descriptor.
function run_test() {
  local test_descriptor="$1"

  log "Running test:"
  log "$test_descriptor"

  # Clean up from previous run.
  rm -rf $TR/var/log/memd/*
  update_available $high_available

  # Check descriptor syntax.
  [ "${test_descriptor:0:1}" == $'\n' ] || \
    die "descriptor must start with newline"
  [ "${test_descriptor: -1:1}" == $'\n' ] || \
    die "descriptor must end with newline"

  # Remove vertical bars and wrapping newlines.
  test_descriptor=$(echo "${test_descriptor:1:-1}" | sed 's/|//g')

  # Split lines of test descriptor into an array.
  readarray -t descriptor_array <<< "${test_descriptor}"

  local expected_clips="${descriptor_array[-1]}"
  local parallel_events=("${descriptor_array[@]}")
  unset parallel_events[-1]

  # Find length of expected clips.
  local duration=${#expected_clips}

  # Check that it matches the main event sequence length.
  [ $duration -eq ${#parallel_events[0]} ] || die "mismatched descriptors"

  # ============
  # RUN THE TEST
  # ============

  # Start daemon in test mode in the background.
  if false; then
    RUST_LOG=debug RUST_BACKTRACE=1 strace \
            ./target/debug/memd test >& memd.strace.out &
  elif false; then
    RUST_LOG=debug RUST_BACKTRACE=1 ./target/debug/memd test &
  else
    RUST_BACKTRACE=1 ./target/debug/memd test &
  fi
  memd_pid=$!

  local test_start_time_cs=$(uptime_centiseconds)
  log test start: $test_start_time_cs
  local seconds=0

  while [ $seconds -lt $duration ]; do
    kill -0 $memd_pid > /dev/null 2>&1 || die "memd exited prematurely"
    for events in "${parallel_events[@]}"; do
      e=${events:$seconds:1}
      case "$e" in
        (''|' '|'.')
          # do nothing
          ;;
        ([0-9])
          send_signal tab-discard
          ;;
        (k)
          send_signal oom-kill
          ;;
        (K)
          kernel_oom_notify
          ;;
        (L)
          # start low pressure (slow poll)
          update_available $high_available
          ;;
        (M)
          # start medium pressure (fast poll)
          update_available $low_available
          ;;
        (H)
          # start high pressure (low-mem notification)
          update_available $below_margin_available
          ;;
        (*)
          die "invalid descriptor character '$e'"
          ;;
      esac
    done
    sleep 1
    seconds=$((seconds + 1))
  done

  # Request graceful exit.  Signals are only checked in fast polling, so we
  # must activate that first.
  update_available $low_available
  send_signal exit-gracefully
  wait

  # CHECK OUTPUTS

  # Check parameters file.  Remove time stamp first.
  [ "$(sed 1d $TR/var/log/memd/memd.parameters)" == \
    "$(cat $EXPECTED_MEMD_PARAMETERS)" ] || \
    die "unexpected content of memd.parameters"

  # Check clip files.
  local event_count
  local uptime_cs=$test_start_time_cs
  local seconds=0
  local current_clip_token='.'
  local last_clip_number="-1"
  local clip_start_time_cs="not-started"

  while [ $seconds -lt $duration ]; do
    new_clip_token=${expected_clips:$seconds:1}
    if [ "$current_clip_token" != "$new_clip_token" ]; then
      # If this is the end of a clip, verify the clip file contents.
      if [[ $current_clip_token =~ [0-9] ]]; then
        last_clip_number=$current_clip_token
        check_clip $current_clip_token \
                   $clip_start_time_cs $uptime_cs $event_count
      fi
      # Start new clip.
      clip_start_time_cs=$uptime_cs
      current_clip_token=$new_clip_token
      event_count=0
    fi

    # Count number of events in clip.
    for events in "${parallel_events[@]}"; do
      e=${events:$seconds:1}
      case "$e" in
        ([0-9Hk])
          event_count=$((event_count + 1))
          ;;
        (K)
          # memd emits two samples for this one
          event_count=$((event_count + 2))
          ;;
      esac
    done

    # Advance time variables.
    seconds=$((seconds + 1))
    uptime_cs=$((uptime_cs + 100))

  done

  # Ensure there are no additional clips.
  clip_file=$(clip_filename $((last_clip_number + 1)))
  [ -e "$clip_file" ] && die "not expecting clip file $clip_file"

  log "***** Test passed. *****"
}

# ================
# Test Descriptors
# ================

# "Graphically" define events and expected result.
#
# The top lines of the test descriptor (all lines except the last one) define
# sequences of events.  The last line describes the expected result.
#
# Events are single characters:
#
# M = start medium pressure (fast poll)
# H = start high pressure (low-mem notification)
# L = start low pressure (slow poll)
# <digit n> = n-th tab discard
# K = kernel OOM kill
# k = chrome notification of OOM kill
# ' ', . = nop (just wait 1 second)
# | = ignored (no delay), cosmetic only
#
# - each character indicates a 1-second slot
# - events (if any) happen near the beginning of their slot
# - multiple events in the same slot are stacked vertically
#
# Example:
#
# ..H.1..L
#     2
#
# means:
#  - wait 2 seconds
#  - signal high-memory pressure, wait 1 second
#  - wait 1 second
#  - signal two tab discard events (named 1 and 2), wait 1 second
#  - wait 2 more seconds
#  - return to low-memory pressure
#
# The last line describes the expected clip logs.  Each log is identified by
# one digit: 0 for memd.clip000.log, 1 for memd.clip001.log etc.  The positions
# of the digits correspond to the time span covered by each clip.  So a clip
# file whose description is 5 characters long is supposed to contain 5 seconds
# worth of samples.
#
# For readability, the descriptor must start and end with a newline.

# Very simple test: go from slow poll to fast poll and back.  No clips
# are collected.

run_test "
.M.L.
.....
"

# Simple test: start fast poll, signal low-mem, signal tab discard.
run_test "
.M...H..1.....L
...0000000000..
"

# Two full disjoint clips.  Also tests kernel-reported and chrome-reported OOM
# kills.
run_test "
.M......K.............k.....
...0000000000....1111111111.
"

# Test that clip collection continues for the time span of interest even if
# available memory returns quickly to a high level.  Note that the
# medium-pressure event (M) is at t = 3s, but the fast poll starts at 4s
# (multiple of 2s slow-poll period).
run_test "
.MH1L.....
..000000..
"

# Several discards, which result in three consecutive clips.  Tab discards 1
# and 2 produce an 8-second clip because the first two seconds of data are
# missing.  Also see the note above regarding fast poll start.
run_test "
...M.H12..|...3...6..|.7.....L
          |   4      |
          |   5      |
....000000|0011111111|112222..
"
