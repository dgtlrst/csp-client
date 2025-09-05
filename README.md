 # libcsp-2-linux

A linux libcsp 2.0 client/server, utilizing can / uart communication interfaces.

This is using https://github.com/libcsp/libcsp/tree/develop

## Building

```bash
sudo ./build.sh
```

To see output (csp_print) and to use the tool, please enable the following options inside the `csp` CMakeLists.txt file:

```
option(CSP_HAVE_STDIO "OS provides C Standard I/O functions" ON)
option(CSP_ENABLE_CSP_PRINT "Enable csp_print() function" ON)
option(CSP_PRINT_STDIO "Use vprintf() for csp_print() function" ON)

set(CSP_QFIFO_LEN 15 CACHE STRING "Length of incoming queue for router task")
set(CSP_PORT_MAX_BIND 16 CACHE STRING "Length of incoming queue for router task")
set(CSP_CONN_RXQUEUE_LEN 16 CACHE STRING "Number of packets in connection queue")
set(CSP_CONN_MAX 8 CACHE STRING "Number of new connections on socket queue")
set(CSP_BUFFER_SIZE 256 CACHE STRING "Bytes in each packet buffer")
set(CSP_BUFFER_COUNT 15 CACHE STRING "Number of total packet buffers")
set(CSP_RDP_MAX_WINDOW 5 CACHE STRING "Max window size for RDP")
set(CSP_RTABLE_SIZE 10 CACHE STRING "Number of elements in routing table")

option(CSP_HAVE_LIBSOCKETCAN "Use libsocketcan" ON)
option(CSP_HAVE_LIBZMQ "Use libzmq" ON)
option(CSP_USE_RTABLE "Use routing table" ON)
```

alternatively, pass these in from the project's CMakeLists.txt itself. Note that this can be already done. Attempt to build first, and only if you have issues, refer to the CMakeLists.txt file.

## For compatibility with KISS

The KISS frames produced by this client are structured:

```
C0 00 [6-byte CSP header] [8-byte ping data] [4-byte CRC32] C0
```

For the server to successfully interpret KISS frames from any node, the following must be completed:
- wrap regular CSP packet with `0xC0 0x00` and `0xC0`
- calculate and append CRC32 to the end of the packet
- append `0xC0` to the end of the packet

The CRC32 is part of the protocol and is not the application-level connection option checksum.
