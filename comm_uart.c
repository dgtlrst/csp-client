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

/* Configuration */
#define SERVER_PORT 10
#define UART_BAUDRATE 115200

/* Global variables */
static uint8_t node_addr = 2;
static uint8_t dest_addr = 1;
static const char *uart_device = "/tmp/pty1";
static csp_usart_fd_t uart_fd;

/* Static allocation for interface */
static csp_iface_t uart_iface_data;
static csp_iface_t *uart_iface = &uart_iface_data;
static char uart_ifname[16] = "UART";

/* Custom UART interface callback */
static void uart_rx_callback(void *user_data, uint8_t *data, size_t data_size,
                             void *pxTaskWoken) {
    /* Create a packet from the received data */
    csp_packet_t *packet = csp_buffer_get(data_size);
    if (packet == NULL) {
        csp_print("Failed to get buffer for RX packet\n");
        return;
    }

    /* Copy data to packet's frame begin area */
    memcpy(packet->frame_begin, data, data_size);
    packet->frame_length = data_size;

    /* Setup RX packet (strip CSP header) */
    if (csp_id_strip(packet) != 0) {
        csp_print("Failed to strip CSP header, discarding packet\n");
        csp_buffer_free(packet);
        uart_iface->rx_error++;
        return;
    }

    /* Update statistics */
    uart_iface->rx++;
    uart_iface->rxbytes += data_size;

    /* Process the packet */
    csp_qfifo_write(packet, uart_iface, pxTaskWoken);
}

/* UART interface transmit function */
static int uart_tx(csp_iface_t *iface, uint16_t via, csp_packet_t *packet,
                   int from_me) {
    /* Add CSP header */
    csp_id_prepend(packet);

    /* Write data to UART */
    if (csp_usart_write(uart_fd, packet->frame_begin, packet->frame_length) !=
        CSP_ERR_NONE) {
        iface->tx_error++;
        csp_buffer_free(packet);
        return CSP_ERR_DRIVER;
    }

    /* Update statistics */
    iface->tx++;
    iface->txbytes += packet->frame_length;

    /* Free packet */
    csp_buffer_free(packet);

    return CSP_ERR_NONE;
}

/* Server task */
void server_task(void) {
    csp_socket_t sock;
    csp_conn_t *conn;
    csp_packet_t *packet;

    /* Create socket and listen for incoming connections */
    memset(&sock, 0, sizeof(sock));
    csp_bind(&sock, CSP_ANY);
    csp_listen(&sock, 5);

    while (1) {
        /* Wait for connection (timeout 1000ms) */
        if ((conn = csp_accept(&sock, 1000)) == NULL) {
            continue;
        }

        /* Read packets on connection */
        while ((packet = csp_read(conn, 100)) != NULL) {
            /* Use the standard CSP service handler for all packets */
            /* This will automatically handle ping requests and other CSP
             * services */
            csp_service_handler(packet);
        }

        /* Close connection */
        csp_close(conn);
    }
}

/* Client task */
void client_task(void) {
    /* Ping the server using standard csp_ping */
    csp_print("[ping] to [%d]\n", dest_addr);
    int ping_result = csp_ping(dest_addr, 1000, 8, CSP_O_NONE);

    /* Sleep for 1 second to maintain a 1-second ping rate */
    sleep(1);
}

/* Thread functions */
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
    while (1) {
        client_task();
        /* No additional sleep needed - client_task already sleeps for 1 second
         */
    }
    return NULL;
}

/* Create a thread */
static int create_thread(void *(*routine)(void *)) {
    pthread_t thread;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    int ret = pthread_create(&thread, &attr, routine, NULL);
    pthread_attr_destroy(&attr);

    return ret;
}

/* Initialize UART interface */
static int init_uart_interface(void) {
    /* Configure UART */
    csp_usart_conf_t conf = {
        .device = uart_device,
        .baudrate = UART_BAUDRATE,
        .databits = 8,
        .stopbits = 1,
        .paritysetting = 0,
    };

    /* Open UART */
    int error = csp_usart_open(&conf, uart_rx_callback, NULL, &uart_fd);
    if (error != CSP_ERR_NONE) {
        csp_print("Failed to open UART: %d\n", error);
        return error;
    }

    /* Configure interface */
    memset(uart_iface, 0, sizeof(*uart_iface));
    uart_iface->name = uart_ifname;
    uart_iface->addr = node_addr;
    uart_iface->netmask = csp_id_get_host_bits();
    uart_iface->is_default = 1;
    uart_iface->nexthop = uart_tx;

    /* Add interface to CSP */
    csp_iflist_add(uart_iface);

    return CSP_ERR_NONE;
}

int main(int argc, char *argv[]) {
    /* Parse command line arguments */
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

    /* Print configuration */
    printf("CSP UART Node\n");
    printf("Node address: %d\n", node_addr);
    printf("Destination: %d\n", dest_addr);
    printf("UART device: %s\n", uart_device);

    /* Initialize CSP */
    csp_init();

    /* Initialize UART interface */
    if (init_uart_interface() != CSP_ERR_NONE) {
        csp_print("Failed to initialize UART interface\n");
        return 1;
    }

    /* Set up routing */
    csp_rtable_set(dest_addr, -1, uart_iface, CSP_NO_VIA_ADDRESS);

    /* Start threads */
    create_thread(router_thread);
    create_thread(server_thread);
    create_thread(client_thread);

    /* Print connection table and interfaces */
    csp_print("Connection table:\n");
    csp_conn_print_table();

    csp_print("Interfaces:\n");
    csp_iflist_print();

    /* Main loop */
    while (1) {
        sleep(10);
    }

    return 0;
}
