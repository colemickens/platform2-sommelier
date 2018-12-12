# qmi2cpp - Chrome OS QMI IDL Compiler

## Background

libqmi is a library that allows for the encoding and decoding of QMI (Qualcomm
MSM Interface) data. The library uses structs that represent the decoded data of
requests/responses/indications in conjunction with `qmi_elem_info` arrays to
perform the desired encoding and decoding of data. The purpose of the
`qmi_elem_info` arrays are to fill in the gap left by the lack of reflection in
the C language. Each `qmi_elem_info` element in an array represents a single
member in a message struct, allowing for the libqmi runtime to examine and
traverse structs during decoding and encoding. The process of hand-writing such
arrays is time-consuming and error-prone however, which led to the development
of a number of tools--such as qmic--to perform this generation automatically
from a human-readible set of message specifications. Existing tools have
limitations such as incomplete support for QMI data types (e.g. enums and
masks), sometimes not generating code that can be used by a ChromeOS project
without hand-modification, and often seem to be difficult to extend in
meaningful ways.

qmi2cpp is a QMI IDL compiler that is built with extensibility in mind, allowing
for easy modifications to the specification language or the generated output.
Additionally, all generated outputs are directly usable by ChromeOS projects
without manual modifications.

## Basic Usage

As per the usage message:
`usage: qmi2cpp.py [-h] PROJECT_NAME [QMI_FILE]`

qmi2cpp requires the specification of a project name, which is used for include
guards and relative paths, and the qmi specification file. Generated output
files are by default placed in the same directory as the input qmi specification
file.

### QMI Message Specification Language

The interface description language used by qmi2cpp input files is a C-like
language very similar to the (undocumented) qmic IDL. General characteristics of
the language are:
*   Statements, message elements, and message definitions end with a semicolon.
*   Struct definitions do not end with semicolons.
*   The first statement in every specification file should be be `package
    PACKAGE_NAME;`.
    *   PACKAGE_NAME can be useful for disambiguating message structs for
        different QMI services (e.g. UIM vs WDS) that have the same name.
*   Message definitions and message elements (but not struct definitions) end
    with an assignment to a number literal. This number represents the type
    value of the message or element (which will be the T value in the encoded
    QMI TLV).
    *   Two elements in the same message may NOT have the same type value, but
        elements in different messages may.
*   Numbers may be decimal integers written as a series of contiguous digits,
    or hexadecimal integers written as a series of contiguous digits preceded
    by '0x' or '0X'.

The EBNF specification of the IDL (lang_spec.ebnf) can be used as a more
complete documentation of the language. For those familiar with C, however, the
following example should make the syntax rather clear:
```
package uim;

const MESSAGE_SIZE = 256;

struct result_t {
  u16 result;
  u16 error;
};

request initialize_req {
  required u8 slot = 0x01;
  optional u8 hello_message(u16 : MESSAGE_SIZE) = 0x10;
} = 0x12;

response initialize_resp {
  required result_t result = 0x2;
  optional u8 channel_id = 0x10;
  optional u8 hello_response(u8 : MESSAGE_SIZE) = 0x11;
} = 0x12;

request disconnect_req {
  required u8 slot = 0x01;
} = 0x1A;

response disconnect_resp {
  required result_t result = 0x2;
} = 0x1A;
```
