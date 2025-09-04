#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <csp/csp.h>
#include <csp/csp_crc32.h>
#include <csp/csp_debug.h>
#include <csp/csp_id.h>
#include <csp/drivers/usart.h>

#define SERVER_PORT 10
#define UART_BAUDRATE 115200

static uint8_t node_addr = 2;
static uint8_t dest_addr = 1;
static const char *uart_device = "/tmp/pty1";
static csp_usart_fd_t uart_fd;

static csp_iface_t uart_iface_data;
static csp_iface_t *uart_iface = &uart_iface_data;
static char uart_ifname[16] = "UART";

static void uart_rx_callback(void *user_data, uint8_t *data, size_t data_size,
                             void *pxTaskWoken) {
    csp_packet_t *packet = csp_buffer_get(data_size);

    if (packet == NULL) {
        csp_print("Failed to get buffer for RX packet\n");
        return;
    }

    // copy data to packet's frame begin area
    memcpy(packet->frame_begin, data, data_size);
    packet->frame_length = data_size;

    csp_print("RX: len=%u src=%u dst=%u sport=%u dport=%u pri=%u flags=0x%02X frame=[", 
              packet->frame_length, packet->id.src, packet->id.dst,
              packet->id.sport, packet->id.dport, packet->id.pri, 
              packet->id.flags);
    for(int i = 0; i < packet->frame_length; i++) 
        csp_print("%02X", packet->frame_begin[i]); 
    csp_print("]\n");

    // setup RX packet (strip CSP header)
    if (csp_id_strip(packet) != 0) {
        csp_print("failed to strip CSP header, discarding packet\n");
        csp_buffer_free(packet);
        uart_iface->rx_error++;
        return;
    }

    // update statistics
    uart_iface->rx++;
    uart_iface->rxbytes += data_size;

    csp_qfifo_write(packet, uart_iface, pxTaskWoken);
}

static int uart_tx(csp_iface_t *iface, uint16_t via, csp_packet_t *packet,
                   int from_me) {
    csp_id_prepend(packet);

    csp_print("TX: len=%u src=%u dst=%u sport=%u dport=%u pri=%u flags=0x%02X "
              "frame=[",
              packet->frame_length, packet->id.src, packet->id.dst,
              packet->id.sport, packet->id.dport, packet->id.pri,
              packet->id.flags);
    for (int i = 0; i < packet->frame_length; i++)
        csp_print("%02X", packet->frame_begin[i]);
    csp_print("]\n");

    if (csp_usart_write(uart_fd, packet->frame_begin, packet->frame_length) !=
        CSP_ERR_NONE) {
        iface->tx_error++;
        csp_buffer_free(packet);
        return CSP_ERR_DRIVER;
    }

    iface->tx++;
    iface->txbytes += packet->frame_length;

    csp_buffer_free(packet);

    return CSP_ERR_NONE;
}

void server_task(void) {
    csp_socket_t sock;
    csp_conn_t *conn;
    csp_packet_t *packet;

    // create socket and listen for incoming connections
    memset(&sock, 0, sizeof(sock));
    csp_bind(&sock, CSP_ANY);
    csp_listen(&sock, 5);

    while (1) {
        // wait for connection (timeout 1000ms)
        if ((conn = csp_accept(&sock, 1000)) == NULL) {
            continue;
        }

        // read pkts on connection
        while ((packet = csp_read(conn, 100)) != NULL) {
            /* Use the standard CSP service handler for all packets */
            /* This will automatically handle ping requests and other CSP
             * services */
            // csp_service_handler(packet);
            switch (packet->id.dport)
            {
                case CSP_PING:
                    csp_print("ping received\n");
                    break;
                default:
                    csp_print("pkt received on SERVER_PORT: %s\n", (char *) packet->data);
                    csp_buffer_free(packet);
                    break;
            }
        }

        csp_close(conn);
    }
}

void client_task(void) {
    // ping the server using standard csp_ping
    csp_print("[ping] to [%d]\n", dest_addr);

    int ping_result = csp_ping(dest_addr, 1000, 8, CSP_O_NONE);

    sleep(1);
}

static void *router_thread(void *param) {
    while (1) {
        csp_route_work();
    }
    return NULL;
}

static void *server_thread(void *param) {
    server_task();
    return NULL;
}

static void *client_thread(void *param) {
    while (1) {client_task();}
    return NULL;
}

static int create_thread(void *(*routine)(void *)) {
    pthread_t thread;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    int ret = pthread_create(&thread, &attr, routine, NULL);
    pthread_attr_destroy(&attr);

    return ret;
}

// init uart interface
static int init_uart_interface(void) {

    csp_usart_conf_t conf = {
        .device = uart_device,
        .baudrate = UART_BAUDRATE,
        .databits = 8,
        .stopbits = 1,
        .paritysetting = 0,
    };

    // open uart
    int error = csp_usart_open(&conf, uart_rx_callback, NULL, &uart_fd);
    if (error != CSP_ERR_NONE) {
        csp_print("failed to open uart: %d\n", error);
        return error;
    }

    // configure interface
    memset(uart_iface, 0, sizeof(*uart_iface));
    uart_iface->name = uart_ifname;
    uart_iface->addr = node_addr;
    uart_iface->netmask = csp_id_get_host_bits();
    uart_iface->is_default = 1;
    uart_iface->nexthop = uart_tx;

    // add interface to csp
    csp_iflist_add(uart_iface);

    return CSP_ERR_NONE;
}

int main(int argc, char *argv[]) {
    int opt;

    while ((opt = getopt(argc, argv, "a:d:u:h")) != -1) {
        switch (opt) {
        case 'a':
            node_addr = atoi(optarg);
            break;
        case 'd':
            dest_addr = atoi(optarg);
            break;
        case 'u':
            uart_device = optarg;
            break;
        case 'h':
            printf("Usage: %s [-a node_addr] [-d dest_addr] [-u uart_device]\n",
                   argv[0]);
            return 0;
        default:
            fprintf(
                stderr,
                "Usage: %s [-a node_addr] [-d dest_addr] [-u uart_device]\n",
                argv[0]);
            return 1;
        }
    }

    printf("csp uart node\n");
    printf("src address: %d\n", node_addr);
    printf("dest address: %d\n", dest_addr);
    printf("uart device: %s\n", uart_device);

    csp_init();

    if (init_uart_interface() != CSP_ERR_NONE) {
        csp_print("failed to initialize UART interface\n");
        return 1;
    }

    csp_rtable_set(dest_addr, -1, uart_iface, CSP_NO_VIA_ADDRESS);

    create_thread(router_thread);
    create_thread(server_thread);
    create_thread(client_thread);

    csp_print("connection table:\n");
    csp_conn_print_table();
    csp_print("interfaces:\n");
    csp_iflist_print();

    while (1) {
        sleep(10);
    }

    return 0;
}
