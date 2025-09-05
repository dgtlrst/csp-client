#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <csp/csp.h>
#include <csp/csp_debug.h>
#include <csp/interfaces/csp_if_kiss.h>
#include <csp/drivers/usart.h>

#define SERVER_PORT 10
#define KISS_BAUDRATE 115200

static uint8_t node_addr = 2;
static uint8_t dest_addr = 1;
static const char *kiss_device = "/tmp/pty1";
static csp_iface_t *kiss_iface = NULL;

void server_task(void) {
    csp_socket_t sock;
    csp_conn_t *conn;
    csp_packet_t *packet;

    // create socket and listen for incoming connections
    memset(&sock, 0, sizeof(sock));
    csp_bind(&sock, CSP_ANY);
    csp_listen(&sock, 5);

    csp_print("kiss server task started\n");

    while (1) {
        // wait for connection (timeout 1000ms)
        if ((conn = csp_accept(&sock, 1000)) == NULL) {
            continue;
        }

        csp_print("kiss server accepted connection\n");

        // read pkts on connection
        while ((packet = csp_read(conn, 100)) != NULL) {
            switch (packet->id.dport)
            {
                case CSP_PING:
                    csp_print("kiss ping received\n");
                    csp_buffer_free(packet);
                    break;
                default:
                    csp_print("kiss pkt received on SERVER_PORT: %s\n", (char *) packet->data);
                    csp_buffer_free(packet);
                    break;
            }
        }

        csp_close(conn);
    }
}

void client_task(void) {
    // ping the server using standard csp_ping
    csp_print("kiss ping to [%d]\n", dest_addr);

    int ping_result = csp_ping(dest_addr, 1000, 8, CSP_O_NONE);
    csp_print("kiss ping result: %d ms\n", ping_result);

    sleep(1);
}

static void *router_thread(void *param) {
    csp_print("kiss router thread started\n");
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

// init kiss interface
static int init_kiss_interface(void) {
    csp_usart_conf_t conf = {
        .device = kiss_device,
        .baudrate = KISS_BAUDRATE,
        .databits = 8,
        .stopbits = 1,
        .paritysetting = 0,
    };

    csp_print("kiss opening device: %s at %d baud\n", kiss_device, KISS_BAUDRATE);

    // open kiss interface using libcsp convenience function
    int error = csp_usart_open_and_add_kiss_interface(&conf, CSP_IF_KISS_DEFAULT_NAME, node_addr, &kiss_iface);
    if (error != CSP_ERR_NONE) {
        csp_print("kiss failed to open interface: %d\n", error);
        return error;
    }

    csp_print("kiss interface opened successfully\n");

    // set as default interface
    kiss_iface->is_default = 1;

    return CSP_ERR_NONE;
}

int main(int argc, char *argv[]) {
    int opt;

    while ((opt = getopt(argc, argv, "a:d:k:h")) != -1) {
        switch (opt) {
        case 'a':
            node_addr = atoi(optarg);
            break;
        case 'd':
            dest_addr = atoi(optarg);
            break;
        case 'k':
            kiss_device = optarg;
            break;
        case 'h':
            printf("Usage: %s [-a node_addr] [-d dest_addr] [-k kiss_device]\n",
                   argv[0]);
            return 0;
        default:
            fprintf(
                stderr,
                "Usage: %s [-a node_addr] [-d dest_addr] [-k kiss_device]\n",
                argv[0]);
            return 1;
        }
    }

    printf("csp kiss node\n");
    printf("src address: %d\n", node_addr);
    printf("dest address: %d\n", dest_addr);
    printf("kiss device: %s\n", kiss_device);

    csp_init();

    if (init_kiss_interface() != CSP_ERR_NONE) {
        csp_print("kiss failed to initialize interface\n");
        return 1;
    }

    csp_rtable_set(dest_addr, -1, kiss_iface, CSP_NO_VIA_ADDRESS);

    create_thread(router_thread);
    create_thread(server_thread);
    create_thread(client_thread);

    csp_print("kiss connection table:\n");
    csp_conn_print_table();
    csp_print("kiss interfaces:\n");
    csp_iflist_print();

    while (1) {
        sleep(10);
    }

    return 0;
}
