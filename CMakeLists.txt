cmake_minimum_required(VERSION 3.10)
set(PROJECT cspc)
project(${PROJECT} C)

set(source main.c)
set(uart_source comm_uart.c)
set(kiss_source comm_kiss.c)

# pre-configure CSP settings
set(CSP_HAVE_STDIO ON CACHE BOOL "OS provides C Standard I/O functions")
set(CSP_ENABLE_CSP_PRINT ON CACHE BOOL "Enable csp_print() function")
set(CSP_PRINT_STDIO ON CACHE BOOL "Use vprintf() for csp_print() function")

set(CSP_QFIFO_LEN 15 CACHE STRING "Length of incoming queue for router task")
set(CSP_PORT_MAX_BIND 16 CACHE STRING "Length of incoming queue for router task")
set(CSP_CONN_RXQUEUE_LEN 16 CACHE STRING "Number of packets in connection queue")
set(CSP_CONN_MAX 8 CACHE STRING "Number of new connections on socket queue")
set(CSP_BUFFER_SIZE 256 CACHE STRING "Bytes in each packet buffer")
set(CSP_BUFFER_COUNT 15 CACHE STRING "Number of total packet buffers")
set(CSP_RDP_MAX_WINDOW 5 CACHE STRING "Max window size for RDP")
set(CSP_RTABLE_SIZE 10 CACHE STRING "Number of elements in routing table")

set(CSP_HAVE_LIBSOCKETCAN ON CACHE BOOL "Use libsocketcan")
set(CSP_HAVE_LIBZMQ ON CACHE BOOL "Use libzmq")
set(CSP_USE_RTABLE ON CACHE BOOL "Use routing table")

# add the csp subdirectory
add_subdirectory(csp)

# define the executables for the project
add_executable(${PROJECT} ${source})
add_executable(uart_comm ${uart_source})
add_executable(kiss_comm ${kiss_source})

# ensure the CSP library is built before the executables
add_dependencies(${PROJECT} csp)
add_dependencies(uart_comm csp)
add_dependencies(kiss_comm csp)

# configure compilation options
target_compile_options(${PROJECT} PUBLIC -g -fdiagnostics-color=always)
target_compile_options(uart_comm PUBLIC -g -fdiagnostics-color=always)
target_compile_options(kiss_comm PUBLIC -g -fdiagnostics-color=always)
target_compile_definitions(${PROJECT} PUBLIC -D_GNU_SOURCE)
target_compile_definitions(uart_comm PUBLIC -D_GNU_SOURCE)
target_compile_definitions(kiss_comm PUBLIC -D_GNU_SOURCE)

# link the CSP library with the project executables
target_link_libraries(${PROJECT} csp)
target_link_libraries(uart_comm csp pthread)
target_link_libraries(kiss_comm csp pthread)
