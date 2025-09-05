#include <stdlib.h>
#include <unistd.h>
#include <csp/csp.h>
#include <csp/csp_id.h>
#ifdef CSP_HAVE_LIBSOCKETCAN
#include <csp/drivers/can_socketcan.h>
#include <linux/can.h>
#endif
#include <pthread.h>
#include <errno.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>

#define SERVER_PORT (10U)

static uint8_t destination_addr = 1;
static uint8_t source_addr = 2;
static unsigned int successful_ping = 0;

int router_start(void);
int client_start(void);

#ifdef CSP_HAVE_LIBSOCKETCAN
typedef struct {
    char name[CSP_IFLIST_NAME_MAX + 1];
    csp_iface_t iface;
    csp_can_interface_data_t ifdata;
    pthread_t rx_thread;
    int socket;
} can_ctx_t;
#endif

// server thread
void server(void) {
    csp_print("server online\n");

    // init a socket
    csp_socket_t sock = {0};

    // bind to all ports
    csp_bind(&sock, CSP_ANY);

    // 10 conn request can be queued on the socket
    csp_listen(&sock, 10);

    while (1) {
        csp_conn_t *conn;
        csp_packet_t *packet;

        // wait for 1 second for connection requests
        if ((conn = csp_accept(&sock, 1000)) == NULL) {
            csp_print("{%p} %s()#%lu no incomming connections\n", gettid(),
                      __FUNCTION__, __LINE__);
            continue;
        }

        int port = csp_conn_dport(conn);
        int src = csp_conn_src(conn);

        csp_print("{%p} %s()#%lu connection accepted from %d on port %d\n",
                  gettid(), __FUNCTION__, __LINE__, src, port);

        while ((packet = csp_read(conn, 200)) != NULL) {
            csp_print("%s\n", packet->data);

            switch (port) {
                case SERVER_PORT:
                    /* Process packet here */
                    csp_print("pkt received on port [%d]: %s\n", SERVER_PORT, (char *)packet->data);

                    csp_buffer_free(packet);
                    break;
                default:
                    csp_service_handler(packet);
                    break;
            }

            csp_buffer_free(packet);
        }

        csp_close(conn);
    }
    return;
}

// client thread
void client(void) {
    csp_print("client online\n");

    unsigned int count = 'A';

    // send ping to server, timeout 1000 mS, ping size 8 bytes */
    int result = csp_ping(destination_addr, 1000, 8, CSP_O_NONE);
    csp_print("ping address: %u, result %d [mS]\n", destination_addr, result);

    if (result > 0) {
        ++successful_ping;
    }

    /* connect to host on 'destination_addr', port SERVER_PORT with regular
     * UDP-like protocol and 1000 ms timeout */
    // csp_conn_t * conn = csp_connect(CSP_PRIO_NORM, destination_addr,
    // SERVER_PORT, 1000, CSP_O_NONE); 	if (conn == NULL) {
    //      /* Connect failed */
    //      csp_print("connection failure\n");
    //      return;
    // 	}

    // 	/* get packet buffer for message/data */
    // 	csp_packet_t * packet = csp_buffer_get(100);
    // 	if (packet == NULL) {
    // 		/* Could not get buffer element */
    // 		csp_print("failed to get csp buffer\n");
    // 		return;
    // 	}

    // 	/* copy data to packet */
    //  // print sending packet
    //  csp_print("sending packet \"%s\"\n", packet->data);
    //  memcpy(packet->data, "hello world", 12);
    //  memcpy(packet->data + 12, &count, 1);
    //  memset(packet->data + 13, 0, 1);
    //  count++;

    //  // set packet length
    //  packet->length = (strlen((char *) packet->data) + 1); // include 0-termination

    //  // send packet
    //  csp_send(conn, packet);

    //  // close connection
    //  csp_print("closing connection\n");
    // 	csp_close(conn);
    // // }

    csp_print("client thread complete\n");
    return;
}

static int csp_pthread_create(void *(*routine)(void *)) {
    // create pthreads
    pthread_attr_t attributes;
    pthread_t handle;
    int ret;

    if (pthread_attr_init(&attributes) != 0) {
        csp_print("%s#%d: pthread_attr_init() failed\n", __FUNCTION__,
                  __LINE__);
        return CSP_ERR_NOMEM;
    }

    // no need to join with thread to free its resources
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

// router thread
static void *task_router(void *param) {
    while (1) {csp_route_work();}
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

int main(int argc, char *argv[]) {
    const char *can_device = "can_alpha";
    bool client_mode = true;
    int rtable = 0;

    csp_print("init dev > %s\r\nsrc > [%u]\r\ndst > [%u]\r\n", can_device,
              source_addr, destination_addr);

    csp_init();
    router_start();

    csp_iface_t *default_iface = NULL;

    /* init CAN interface */
#ifdef CSP_HAVE_LIBSOCKETCAN
    csp_print("if there is RTNETLINK: the operation is not permitted -> this is because the can device is already UP\r\n");

    if (can_device) {
        int error = csp_can_socketcan_open_and_add_interface(can_device, can_device, source_addr, 1000000, true, &default_iface);

        if (error != CSP_ERR_NONE) {
            csp_print("failed to initialise [%s], error: %d\n", can_device, error);
            exit(EXIT_FAILURE);
        }
    }
#else
    if (can_device) {
        csp_print("CAN support not available - libsocketcan not found during build\n");
        exit(EXIT_FAILURE);
    }
#endif

    // set routing table
    csp_rtable_set(destination_addr, -1, default_iface, CSP_NO_VIA_ADDRESS);

    // start threads
    server_start();
    client_start();

    while (1) {
        if (successful_ping < 5) {
            csp_print("client successfully pinged the server %u times\n", successful_ping);
            exit(EXIT_FAILURE);
        }
        csp_print("Client successfully pinged the server %u times\n", successful_ping);
        exit(EXIT_SUCCESS);
    }
    return 0;
}
