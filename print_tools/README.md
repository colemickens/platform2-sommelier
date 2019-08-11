# print_tools: various tools related to the native printing system

print_diag: this tool sends to a given printer (or other IPP endpoint)
Get-Printer-Attributes request and returns the received response. The response
is parsed and can be saved both as JSON file or as a raw binary file. This
tool allows also to choose an IPP version used in exchange. Run the program
with --help parameter to get more details.

Example:

print_diag --url "ipp://example.org/printer" --version=2.0 --jsonf=out.json --binary=out.raw

In this example, the IPP Get-Printer-Attributes request is sent to the given
URL. The version of IPP protocol is set to 2.0 (default is 1.1). Obtained
response is dumped to a binary file out.raw (optional). The response is also
parsed and saved in JSON format to a file out.json (optional).
