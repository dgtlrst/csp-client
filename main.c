#include <stdlib.h>
#include <unistd.h>

#include <csp/csp.h>
#include <csp/drivers/can_socketcan.h>

#include <csp/csp_id.h>
#include <linux/can.h>
#include <pthread.h>

#include <errno.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>

static pthread_t rx_thread_handle = 0;

typedef struct {
    char name[CSP_IFLIST_NAME_MAX + 1];
    csp_iface_t iface;
    csp_can_interface_data_t ifdata;
    pthread_t rx_thread;
    int socket;
} can_ctx_t;

/* These three functions must be provided in arch specific way */
int router_start(void);
int client_start(void);

/* Server port, the port the server listens on for incoming connections from the
 * client. */
#define SERVER_PORT 10

/* Commandline options */
static uint8_t destination_addr = 1;
static uint8_t source_addr = 2;
static unsigned int successful_ping = 0;

/* Server task receiving requests from client task */
void server(void) {
    csp_print("Server task started\n");

    /* Create socket with no specific socket options, e.g. accepts CRC32, HMAC,
     * etc. if enabled during compilation */
    csp_socket_t sock = {0};

    /* Bind socket to all ports, e.g. all incoming connections will be handled
     * here */
    csp_bind(&sock, CSP_ANY);

    /* Create a backlog of 10 connections, i.e. up to 10 new connections can be
     * queued */
    csp_listen(&sock, 10);

    /* Wait for connections and then process packets on the connection */
    while (1) {

        /* Wait for a new connection, 10000 mS timeout */
        csp_conn_t *conn;
        if ((conn = csp_accept(&sock, 1000)) == NULL) {
            /* timeout */
            csp_print("{%p} %s()#%lu No incomming connections\n", gettid(),
                      __FUNCTION__, __LINE__);
            continue;
        }

        int port = csp_conn_dport(conn);
        int src = csp_conn_src(conn);
        csp_print("{%p} %s()#%lu Connection accepted from %d on port %d\n",
                  gettid(), __FUNCTION__, __LINE__, src, port);
        /* Read packets on connection, timout is 100 mS */
        csp_packet_t *packet;
        while ((packet = csp_read(conn, 200)) != NULL) {
            csp_print("%s\n", packet->data);

            switch (port) {
            case SERVER_PORT:
                /* Process packet here */
                csp_print("Packet received on SERVER_PORT(%d): %s\n",
                          SERVER_PORT, (char *)packet->data);
                csp_buffer_free(packet);
                break;

            default:
                csp_service_handler(packet);
                break;
            }
            csp_buffer_free(packet);
        }

        /* Close current connection */
        csp_close(conn);
    }
    return;
}

/* Client task sending requests to server task */
void client(void) {

    csp_print("Client task started\n");

    unsigned int count = 'A';

    // /* Send ping to server, timeout 1000 mS, ping size 8 bytes */
    int result = csp_ping(destination_addr, 1000, 8, CSP_O_NONE);
    csp_print("Ping address: %u, result %d [mS]\n", destination_addr, result);

    if (result > 0) {
        ++successful_ping;
    }

    /* Send data packet (string) to server */

    /* 1. Connect to host on 'destination_addr', port SERVER_PORT with regular
     * UDP-like protocol and 1000 ms timeout */
    // 	csp_conn_t * conn = csp_connect(CSP_PRIO_NORM, destination_addr,
    // SERVER_PORT, 1000, CSP_O_NONE); 	if (conn == NULL) {
    // 		/* Connect failed */
    // 		csp_print("Connection failed\n");
    // 		return;
    // 	}

    // 	/* 2. Get packet buffer for message/data */
    // 	csp_packet_t * packet = csp_buffer_get(100);
    // 	if (packet == NULL) {
    // 		/* Could not get buffer element */
    // 		csp_print("Failed to get CSP buffer\n");
    // 		return;
    // 	}

    // 	/* 3. Copy data to packet */
    //     // print sending packet
    //     csp_print("Sending packet \"%s\"\n", packet->data);
    //     memcpy(packet->data, "Hello world ", 12);
    //     memcpy(packet->data + 12, &count, 1);
    //     memset(packet->data + 13, 0, 1);
    //     count++;

    // 	/* 4. Set packet length */
    // 	packet->length = (strlen((char *) packet->data) + 1); /* include the 0
    // termination */

    // 	/* 5. Send packet */
    // 	csp_send(conn, packet);

    // 	/* 6. Close connection */
    //     csp_print("closing csp connection\n");
    // 	csp_close(conn);
    // // }

    csp_print("Client task finished\n");
    return;
}

static int csp_pthread_create(void *(*routine)(void *)) {

    pthread_attr_t attributes;
    pthread_t handle;
    int ret;

    csp_print("starting csp_thread_create");

    if (pthread_attr_init(&attributes) != 0) {
        csp_print("%s#%d: pthread_attr_init() failed\n", __FUNCTION__,
                  __LINE__);
        return CSP_ERR_NOMEM;
    }
    /* no need to join with thread to free its resources */
    pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);

    ret = pthread_create(&handle, &attributes, routine, NULL);
    pthread_attr_destroy(&attributes);

    if (ret != 0) {
        csp_print("%s#%d: pthread_create() failed, error: %s\n", __FUNCTION__,
                  __LINE__, strerror(errno));
        return ret;
    }

    return CSP_ERR_NONE;
}

static void *task_router(void *param) {

    /* Here there be routing */

    while (1) {

        csp_route_work();
    }

    return NULL;
}

int router_start(void) { return csp_pthread_create(task_router); }

static void *task_client(void *param) {
    client();
    return NULL;
}

static void *server_task(void *param) {
    server();
    return NULL;
}

int client_start(void) { return csp_pthread_create(task_client); }

int server_start(void) { return csp_pthread_create(server_task); }

/* main - initialization of CSP and start of client task */
int main(int argc, char *argv[]) {

    const char *can_device = "can_alpha";
    bool client_mode = true;
    int rtable = 0;

    csp_print("init dev > %s\r\nsrc > [%u]\r\ndst > [%u]\r\n", can_device,
              source_addr, destination_addr);

    /* Init CSP */
    csp_init();

    /* Start router */
    router_start();

    /* Add
     interface(s) */
    csp_iface_t *default_iface = NULL;

    /* Initialise CAN interface */
    csp_print(
        "If there is RTNETLINK: Operation not permitted -> this is because "
        "the can device is already UP\r\n");
    if (can_device) {
        int error = csp_can_socketcan_open_and_add_interface(
            can_device, can_device, source_addr, 1000000, true, &default_iface);

        if (error != CSP_ERR_NONE) {
            csp_print("failed to initialise [%s], error: %d\n", can_device,
                      error);
            exit(EXIT_FAILURE);
        }
    }

    /* Set routing table */
    csp_rtable_set(destination_addr, -1, default_iface, CSP_NO_VIA_ADDRESS);

    /* Start client thread */
    server_start();
    client_start();

    /* Wait for execution to end (ctrl+c) */
    while (1) {
        sleep(1);

        /* Test mode, check that server & client can exchange packets */
        if (successful_ping < 5) {
            csp_print("Client successfully pinged the server %u times\n",
                      successful_ping);
            exit(EXIT_FAILURE);
        }
        csp_print("Client successfully pinged the server %u times\n",
                  successful_ping);
        exit(EXIT_SUCCESS);
    }

    return 0;
}
