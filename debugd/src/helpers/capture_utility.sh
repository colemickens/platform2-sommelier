#!/bin/bash

# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Helper script to initiate over-the-air or regular net-device packet
# capture.


# get_device_list returns the name of all network devices shown in an
# "ifconfig" listing.
#
# @return "<device0>\n<device1>..."
get_device_list ()
{
  ifconfig -a | grep '^[^ ]' | awk '{ print $1 }'
}


# get_phy_info gets the WiFi interface type and wiphy identifier for |device|.
#
# @param device, the device we want information about
# @return "<type> <wiphy_number>", or an empty string if |device| is not WiFi.
get_phy_info ()
{
  local device="${1}"
  iw dev "${device}" info 2>/dev/null |
      awk '/^\ttype/ { print $2 }; /^\twiphy/ { print $2 };'
}


# get_devices_for_phy returns all the device names associated with this phy.
#
# @param phy_number, e.g., "3" or "phy3" for which to perform the listing.
# @return "<device0> <device1>...", or an empty string if no devices match.
get_devices_for_phy ()
{
  local phy_number="${1}"
  iw dev 2>/dev/null |
      awk -v search_phy="${phy_number}" \
          'BEGIN { sub(/^phy/, "", search_phy) };
           /^phy#/ { sub(/phy#/, ""); phynum=$1 };
           /^\tInterface/ { if (phynum == search_phy) print $2 }'
}


# get_monitor_phy_list returns a list of WiFi phys that are capable of monitor
# mode on |frequency|.
#
# @param frequency, channel this phy must be able to connect to
# @return "<phy#0> <phy#1> ...", or an empty string if no monitor phys found.
get_monitor_phy_list ()
{
  local frequency="${1}"
  iw phy 2>/dev/null |
      awk -v search_frequency="${frequency}" \
          '/^Wiphy/ { phy=$2; has_frequency=0 };
           /\* [0-9]+ MHz/ { if ($2 == search_frequency) has_frequency=1 };
           /^\t\t \* monitor/ { if (has_frequency) print phy }'
}


# get_phy_info gets the WiFi interface link information -- SSID and frequency.
#
# @param device, the device we want information about
# @return "<BSSID> <frequency>" if connected, otherwise an empty string.
get_link_info ()
{
  local device="${1}"
  iw dev "${device}" link 2>/dev/null |
      awk '/^Connected to/ { print $3 }; /^\tfreq:/ { print $2 };'
}


# get_ht_info gets HT inforomation for a |bssid| on a given |frequency|.
# We depend on the scan cache to get this information since this function
# is only called for a connected AP.
#
# @param device, the device on which to perform the scan
# @param bssid, the identifier for the AP we want information about
# @param frequency, the frequency on which we expect to find this information
# @return "<above|below>" if this is an HT40 network, or an empty string.
get_ht_info ()
{
  local device="${1}"
  local bssid="${2}"
  local frequency="${3}"
  iw dev "${device}" scan dump 2>/dev/null |
      awk -v search_bssid="${bssid}" -v search_frequency="${frequency}" \
           '/^BSS/ { bssid=$2 };
           /^\tfreq:/ { frequency=$2 };
           /\* secondary channel offset: (above|below)/ {
               if (bssid == search_bssid && frequency == search_frequency)
                   print $5
           }'
}


# create_monitor creates a monitor device on |phy|.
#
# @param phy, the phy to create the monitor device on.
# @return "<device>", the name of the created device if successful, or an
#     empty string on failure
create_monitor ()
{
  local phy="${1}"

  # There are no likely collisions here since the caller has already searched
  # for monitor devices over all phys.
  device="${phy}_mon"
  if ! iw phy "${phy}" interface add "${device}" type monitor ; then
    return
  fi
  echo "${device}"
}


# configure_monitor configures a monitor |device| to listen to |frequency|
# and uses an HT location of |ht_location|.
#
# @param device, the monitor device to be configured
# @param frequency, the frequency to listen to
# @param ht_location, "above", "below" or an empty string, indicating that
#     HT40 should not be used.
# @status_code 0 if successful, 1 or the return code for iw.
configure_monitor ()
{
  local device="${1}"
  local frequency="${2}"
  local ht_location="${3}"
  if [[ -z "${ht_location}" ]] ; then
    iw dev "${device}" set freq "${frequency}"
  elif [[ "${ht_location}" == "above" ]] ; then
    iw dev "${device}" set freq "${frequency}" HT40+
  elif [[ "${ht_location}" == "below" ]] ; then
    iw dev "${device}" set freq "${frequency}" HT40-
  else
    error "ht_location should be \"above\" or \"below\", not \"${ht_location}\""
    return 1
  fi
}


# get_monitor_device returns a device that is set up to monitor |frequency|,
# preferably one that is not connected to |wiphy|.
#
# @param frequency, the frequency on which we should monitor
# @param ht_location, the location of the additional 20MHz of bandwidth for
#     HT40, relative to |frequency|.  Can be empty to signify "not HT40".
# @param wiphy, the phy we would rather NOT use for this capture.
# @return "<device>", the discovered or created device, or an empty string
#     indicating failure.
get_monitor_device ()
{
  local frequency="${1}"
  local ht_location="${2}"
  local wiphy="${3}"
  local connected_monitor_device
  local connected_monitor_phy
  local device

  # See if there is a monitor device already around.
  for device in $(get_device_list); do
    local -a phy_info=($(get_phy_info "$device"))
    if [[ "${phy_info[0]}" != "monitor" ]] ; then
      continue
    fi
    if [[ "${phy_info[1]}" == "${wiphy}" ]] ; then
      # Save this one for later, if we don't find anything better.
      connected_monitor_device="${device}"
      continue
    fi
    # If we fail to configure this device, move on to the next device.  The
    # configuration could fail, for example, if the phy is in use.
    if ! configure_monitor "${device}" "${frequency}" "${ht_location}" ; then
      continue
    fi
    echo $device
    return
  done

  # Find a monitor-capable phy and try to create a device on it.
  for phy in $(get_monitor_phy_list "${frequency}"); do
    if [[ "${phy}" = "phy${wiphy}" ]] ; then
      # Try this phy as a last resort.
      connected_monitor_phy=$phy
      continue
    fi

    # Shutdown any un-connected interfaces on this phy.
    local -a phy_devices=($(get_devices_for_phy "${phy}"))
    for check_device in $(get_devices_for_phy "${phy}") ; do
      local -a phy_info=($(get_phy_info "$check_device"))
      if [[ "${phy_info[0]}" == "monitor" ]] ; then
        # We have already tried to use this monitor device  and failed in
        # the first loop.  Skip this phy.
        continue 2
      fi
    done

    for unused_device in $(get_devices_for_phy "${phy}") ; do
      local link_info=($(get_link_info "$unused_device"))
      if [[ ${#link_info[@]} -eq 0 ]] ; then
        error "Shutting down interface ${unused_device} so we can perform"
        error "monitoring.  You may need to disable, then re-enable WiFi to"
        error "use this interface again normally again."
        ifconfig "${unused_device}" down
      else
        error "Warning: Interface ${unused_device} is in-use for an active"
        error "connection.  This may affect the quality of the packet capture."
      fi
    done

    device=$(create_monitor "${phy}")
    if [[ -z "${device}" ]] ; then
      continue
    fi

    if ! configure_monitor "${device}" "${frequency}" "${ht_location}" ; then
      iw dev "${device}" del
      continue
    fi

    echo $device
    return
  done

  # We were unable to find or create a monitor device on a different phy than
  # the one we are connected through.  Let's try using a monitor on the
  # the connected phy.
  if [[ -n "$connected_monitor_device" ]] ; then
    device="${connected_monitor_device}"
    if ! configure_monitor "${device}" "${frequency}" "${ht_location}" ; then
      return
    fi
  elif [[ -n "${connected_monitor_phy}" ]] ; then
    device=$(create_monitor "${connected_monitor_phy}")
    if [[ -z "${device}" ]] ; then
      return
    fi

    if ! configure_monitor "${device}" "${frequency}" "${ht_location}" ; then
      iw dev "${device}" del
      return
    fi
  else
    # The connected phy cannot be used for a monitor either.  We have failed.
    error "Could not find a device to monitor ${frequency} MHz.  It is likely"
    error "that none of your wireless devices are capable of monitor-mode."
    return
  fi
  echo "${device}"
}


# get_monitor_for_link does a "best effort" capture on the specified device.
# If |device| is a connected managed-mode WiFi device, the best case
# scenario is to find an unconnected monitor-capable wireless device that
# can perform a capture on the same channel.  Failing this, if the monitored
# device can enter monitor mode, this would be the second best choice.  Failing
# this (or if this is an un-connected WiFi device or is not in managed mode
# or not WiFi at all) we return an empty string, signifying failure.
#
# @param monitored_device, the device we are asking to monitor
# @param device, the device we want to monitor with, or an empty string
#     to have this function choose one.
# @return "<device>" a monitor device that can be used to capture this link,
#     or an empty string indicating failure.
get_monitor_for_link ()
{
  local monitored_device="${1}"
  local device="${2}"
  local -a phy_info=($(get_phy_info "$monitored_device"))
  if [[ "${phy_info[0]}" != "managed" ]] ; then
    error "Cannot monitor ${monitored_device}: it is not an 802.11 device."
    return
  fi

  local link_info=($(get_link_info "$monitored_device"))
  if [[ ${#link_info[@]} -eq 0 ]] ; then
    error "Cannot monitor ${monitored_device}: it is not currently connected."
    return
  fi

  local bssid="${link_info[0]}"
  local frequency="${link_info[1]}"
  local ht_info=$(get_ht_info "${monitored_device}" "${bssid}" "${frequency}")
  if [[ -z "${device}" ]] ; then
    device=$(get_monitor_device "${frequency}" "${ht_info}" "${phy_info[1]}")
  elif ! configure_monitor "${device}" "${frequency}" "${ht_info}" ; then
    error "Cannot monitor ${monitored_device}: ${device} did not configure."
    return
  fi

  if [[ -z "${device}" ]] ; then
    # Couldn't find or create a monitor-mode device.
    return
  fi

  echo "${device}"
  return
}


# start_capture starts a packet capture on |device|.
#
# @param device, the device to capture form
# @param output_file, file to write the output packet capture to
# @status_code the return value from the capture process.
start_capture ()
{
  local device="${1}"
  local output_file="${2}"
  echo "Capturing from ${device}.  Press Ctrl-C to stop."
  ifconfig "${device}" up
  exec /usr/libexec/debugd/helpers/capture_packets "${device}" "${output_file}"
}


# usage displays a help message explaining the available options.
usage ()
{
  echo "Usage: $0 [ --device <device> ] [ --frequency <frequency> ] "
  echo "        [ --ht-location <above|below> ] "
  echo "        [ --monitor-connection-on <monitored_device> ]"
  echo "        [ --help ]"
  echo "        --output-file <pcap_output_file>"
  echo
  echo "Where <device> can be one of:"
  local device
  for device in $(get_device_list); do
    local -a phy_info=($(get_phy_info "$device"))
    echo -n "    $device: "
    if [[ ${#phy_info[@]} -eq 0 ]] ; then
      echo "Ethernet-like device"
      continue
    else
      echo "Wireless device in ${phy_info[0]} mode using Wiphy${phy_info[1]}"
    fi
  done
}


# Prints an error |message|.
#
# @param message to send before exiting
# @param usage, if non-empty, lists command-line usage.
error ()
{
  local message="${1}"
  echo "${message}" 1>&2
}


# fatal_error sends a |message| and exits.
#
# @param message to send before exiting
fatal_error ()
{
  local message="${1}"
  error "${message}"
  exit 1
}


# fatal_error sends a |message|, prints helpful hints about command line usage,
# then exits.
#
# @param message to send before exiting
command_line_error ()
{
  local message="${1}"
  echo "${message}"
  echo
  usage
  exit 1
}


main ()
{
  local device
  local frequency
  local ht_location
  local monitor_connection_on
  local output_file
  while [[ $# -gt 0 ]] ; do
    param="${1}"
    shift
    case "${param}" in
      --device)
        device="${1}"
        shift
        ;;
      --frequency)
        frequency="${1}"
        shift
        ;;
      --ht-location)
        ht_location="${1}"
        if [[ "${ht_location}" != "above" &&
              "${ht_location}" != "below" ]] ; then
          command_line_error "HT location must be either \"above\" or \"below\""
        fi
        shift
        ;;
      --monitor-connection-on)
        monitor_connection_on="${1}"
        shift
        ;;
      --output-file)
        output_file="${1}"
        shift
        ;;
      --help)
        usage
        return 0
        ;;
      *)
        command_line_error "Unknown option ${param}"
        ;;
    esac
  done

  if [[ -z "${output_file}" ]] ; then
    command_line_error "The --output-file argument is mandatory"
  fi

  if [[ -n "${monitor_connection_on}" ]] ; then
    device=$(get_monitor_for_link "${monitor_connection_on}" "${device}")
    if [[ -z "${device}" ]] ; then
      fatal_error "Cannot create a device to monitor ${monitor_connection_on}"
    fi
  elif [[ -z "${device}" ]] ; then
    if [[ -n "${frequency}" ]] ; then
      device=$(get_monitor_device "${frequency}" "${ht_location}")
      if [[ -z "${device}" ]] ; then
        fatal_error "No devices found to capture channel ${frequency}"
      fi
    else
      command_line_error "I don't know what you want me to capture!"
    fi
  elif [[ -n "${frequency}" ]] ; then
    if ! configure_monitor "${device}" "${frequency}" "${ht_location}" ; then
      fatal_error "Unable to set frequency on device ${device}."
    fi
  elif [[ -n "${ht_location}" ]] ; then
    command_line_error "Channel was not specified but ht_location was."
  fi
  start_capture "${device}" "${output_file}"
}

set -e
main $@
