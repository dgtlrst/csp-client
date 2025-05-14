#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>

#include <csp/csp.h>
#include <csp/csp_debug.h>
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
static void uart_rx_callback(void *user_data, uint8_t *data, size_t data_size, void *pxTaskWoken) {
    /* Create a packet from the received data */
    csp_packet_t *packet = csp_buffer_get(data_size);
    if (packet == NULL) {
        csp_print("Failed to get buffer for RX packet\n");
        return;
    }

    /* Copy data to packet */
    memcpy(packet->data, data, data_size);
    packet->length = data_size;

    /* Process the packet */
    csp_qfifo_write(packet, uart_iface, pxTaskWoken);
}

/* Server task */
void server_task(void) {
    csp_socket_t sock;
    csp_conn_t *conn;
    csp_packet_t *packet;

    /* Create socket and listen for incoming connections */
    csp_socket_init(&sock);
    csp_bind(&sock, CSP_ANY);
    csp_listen(&sock, 5);

    csp_print("Server listening on port %d\n", SERVER_PORT);

    while (1) {
        /* Wait for connection (timeout 1000ms) */
        if ((conn = csp_accept(&sock, 1000)) == NULL) {
            continue;
        }

        /* Read packets on connection */
        while ((packet = csp_read(conn, 100)) != NULL) {
            csp_print("Packet received: %s\n", (char *)packet->data);
            csp_buffer_free(packet);
        }

        /* Close connection */
        csp_close(conn);
    }
}

/* Client task */
void client_task(void) {
    csp_conn_t *conn;
    csp_packet_t *packet;

    /* Ping the server */
    int ping_result = csp_ping(dest_addr, 1000, 8, CSP_O_NONE);
    csp_print("Ping result: %d ms\n", ping_result);

    /* Connect to server */
    conn = csp_connect(CSP_PRIO_NORM, dest_addr, SERVER_PORT, 1000, CSP_O_NONE);
    if (conn == NULL) {
        csp_print("Connection failed\n");
        return;
    }

    /* Create packet */
    packet = csp_buffer_get(100);
    if (packet == NULL) {
        csp_print("Failed to get buffer\n");
        csp_close(conn);
        return;
    }

    /* Fill packet with data */
    const char *msg = "Hello from CSP UART node";
    memcpy(packet->data, msg, strlen(msg) + 1);
    packet->length = strlen(msg) + 1;

    /* Send packet */
    if (csp_send(conn, packet) != CSP_ERR_NONE) {
        csp_print("Send failed\n");
        csp_buffer_free(packet);
    }

    /* Close connection */
    csp_close(conn);
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
        sleep(5);  /* Send message every 5 seconds */
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
    uart_iface->netmask = CSP_ID_HOST_MASK;
    uart_iface->is_default = 1;

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
                printf("Usage: %s [-a node_addr] [-d dest_addr] [-u uart_device]\n", argv[0]);
                return 0;
            default:
                fprintf(stderr, "Usage: %s [-a node_addr] [-d dest_addr] [-u uart_device]\n", argv[0]);
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
