# Authpolicy

This directory contains the Authpolicy service which provides functionality to
join Active Directory (AD) domains, authenticate users against AD and to fetch
device and user policies from AD in its native GPO format, convert them into
protobufs and make them available to Chrome OS by injecting them into session
manager.
