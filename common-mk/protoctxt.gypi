# Helper file to convert text format protobuf files to binary format.

# Note: This file is similar to protoc.gypi, but is kept separate because
# - They expect different inputs/outputs: both rules would have to set default
#   values for variables that are not really sensible.
# - protoc.gypi exports a hard dependency, while protoctxt.gypi does not.

# To use this rule, add a target to your gyp file defining:
# protoc_proto_dir: The directory where the proto definition can be found.
# protoc_proto_def: Relative path to proto definition from |protoc_proto_dir|.
# protoc_message_name: The fully qualified name of the message contained in
#     the protobuf.
# protoc_text_dir: The directory where the text format protobuf[s] can be found.
# protoc_bin_dir: The full path to the output directory. Note that this path is
#     not relative to the <(SHARED_INTERMEDIATE_DIR). You must ensure that this
#     directory is unique across all platform2 projects to avoid collisions. One
#     way to do this is to include your project name in the directory path.
# Finally, specify the protobuf files in text format as the sources. These files
# should have a '.prototxt' file extension.
{
  'variables': {
    'protoc_proto_dir%': '.',
    'protoc_text_dir%': '.',
    'protoc': '<!(which protoc)',
  },
  'rules': [
    {
      'rule_name': 'protoc_text_to_bin',
      'extension': 'prototxt',
      'inputs': [
        '<(protoc)',
        '<(protoc_proto_dir)/<(protoc_proto_def)',
      ],
      'outputs': [
        '<(protoc_bin_dir)/<(RULE_INPUT_ROOT).pbf',
      ],
      'action': [
        '<(protoc)',
        '--proto_path', '<(protoc_proto_dir)',
        '--encode', '<(protoc_message_name)',
        '--protobuf_in', '<(protoc_text_dir)/<(RULE_INPUT_NAME)',
        '--protobuf_out', '<(protoc_bin_dir)/<(RULE_INPUT_ROOT).pbf',
        '<(protoc_proto_dir)/<(protoc_proto_def)',
      ],
      'message': 'Encoding text format protobuf file <(RULE_INPUT_ROOT).',
    },
  ],
}
