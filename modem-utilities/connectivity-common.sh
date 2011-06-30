# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Common utilities for shell programs that manipulate devices and the
# connection manager, including "modem" and "connectivity"

# Generates a small snippet of code to take a single argument out of our
# parameter list, and complain (and exit) if it's not present. Used in other
# places like: $(needarg foo), which binds foo to $1.
needarg () {
	# We need to echo eval here because the part of bash that understands
	# variable assignments has already run by the time we substitute in the
	# text of $(needarg foo) - i.e., bash will try to execute 'foo="$1"' as
	# a *command*, which it isn't. The eval forces bash to reparse the code
	# before executing it.
	echo eval "$1=\"\$1\";
		  [ -z \"\$$1\" ] && echo 'Missing arg: $1' && usage;
		  shift"
}

# Generates a small snippet of code to take a matching flag argument
# and value out of our parameter list if present.  If not, assign the
# default value to $1.
# Used in other places like: $(arg_or_default foo bar)
# which binds foo to "bar", unless $1 is "-foo" in which case foo is
# bound to $2.
arg_or_default () {
	echo eval "[ x\"\$1\" = x\"-$1\" ] && $1=\"\$2\" && shift 2;
		  [ -z \"\$$1\" ] && $1=\"$2\""
}

# Generates a small snippet of code to take a single argument out of our
# parameter list.  If it's not present, prompt the user and accept a
# blank input as if the users chose the default specified as $2.
arg_or_prompt () {
	echo eval "$1=\"\$1\";
		  [ -n \"\$$1\" ] && shift ;
		  [ -z \"\$$1\" ] && read -p \"$1 [$2]: \" $1 ;
		  [ -z \"\$$1\" ] && $1=\"$2\";"
}

# Require a key in a csv list of key value pairs
#	$1 - comma separated list of keys and values
#	$2 - required key
# If the key is not found in the argument list, then prompt the user
# for a value for key, and return $key,$value appended to $1
require () {
	local value
	local args=$1
	local key=$2
	if [ -z "$args" -o -n "${args##*$2*}" ] ; then
		read -p "$key: " value
		if [ -n "$args" ] ; then
			args="$args,"
		fi
		args="$args$key,$value"
	fi
	echo "$args"
}

# Removes the indexes output by the --fixed option of dbus-send
stripindexes () {
	sed -e 's/^\/[[:digit:]]\+\///' -e 's/[^[:space:]]*/\0:/' -e 's/^/	/'
}

# Prints values for dbus-send --fixed output lines whose keys match
# the first argument.  Call it with 'Key' and it will take input like
#	/4/Key/0 value value
#	/5/SomethingElse/0 something else
# and write
#	value value
extract_dbus_match () {
	local argument=$1
	sed -r -n -e "s_^/[[:digit:]]+/$argument/\S+\s+__p"
}
