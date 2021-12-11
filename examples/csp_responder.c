/*
Cubesat Space Protocol - A small network-layer protocol designed for Cubesats
Copyright (C) 2012 GomSpace ApS (http://www.gomspace.com)
Copyright (C) 2012 AAUSAT3 Project (http://aausat3.space.aau.dk)

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <csp/csp.h>
#include <csp/arch/csp_thread.h>
#include <csp/drivers/can_socketcan.h>

/* Server port, the port the server listens on for incoming connections from the client. */
#define MY_SERVER_PORT		10

/* Commandline options */
static uint8_t server_address = 255;

static unsigned int server_received = 0;

static enum Response { 
    REFLECT = 0,
    TESTMESSAGE
} state = REFLECT;


void reflect_packet(csp_packet_t *packet){
    csp_sendto(CSP_PRIO_CRITICAL, packet->id.src, packet->id.sport, packet->id.dport, CSP_O_NONE, packet, 1000);
}

void answer_with_testmessage(csp_packet_t *packet){
    const char msg[] = "This is a test message!";
    memcpy(packet->data, &msg, sizeof(msg));
    packet->length = sizeof(msg);
    csp_sendto(CSP_PRIO_CRITICAL, packet->id.src, packet->id.sport, packet->id.dport, CSP_O_NONE, packet, 1000);
}

/* Server task - handles requests from clients */
CSP_DEFINE_TASK(task_server) {

	csp_log_info("Server task started");

	/* Create socket with no specific socket options, e.g. accepts CRC32, HMAC, XTEA, etc. if enabled during compilation */
	csp_socket_t *sock = csp_socket(CSP_SO_NONE);
    

	/* Bind socket to all ports, e.g. all incoming connections will be handled here */
	csp_bind(sock, CSP_ANY);

	/* Create a backlog of 10 connections, i.e. up to 10 new connections can be queued */
	csp_listen(sock, 10);

	/* Wait for connections and then process packets on the connection */
	while (1) {

		/* Wait for a new connection, 10000 mS timeout */
		csp_conn_t *conn;
		if ((conn = csp_accept(sock, 10000)) == NULL) {
			/* timeout */
			continue;
		}
        
		/* Read packets on connection, timout is 100 mS */
		csp_packet_t *packet;
		while ((packet = csp_read(conn, 50)) != NULL) {
			switch (csp_conn_dport(conn)) {
			case MY_SERVER_PORT:
				/* Process packet here */
				csp_log_info("Packet received on MY_SERVER_PORT: %s from src: %d:%d", (char *) packet->data, packet->id.src, packet->id.sport);
                if(state == REFLECT){
                    reflect_packet(packet);
                } else if (state == TESTMESSAGE){
                    answer_with_testmessage(packet);
                } else {
                    csp_buffer_free(packet);
                }
				++server_received;
				break;

			default:
				/* Call the default CSP service handler, handle pings, buffer use, etc. */
				csp_service_handler(conn, packet);
				break;
			}
		}

		/* Close current connection */
		csp_close(conn);

	}

	return CSP_TASK_RETURN;

}
/* End of server task */

/* main - initialization of CSP and start of server/client tasks */
int main(int argc, char * argv[]) {

    uint8_t address = 1;
    csp_debug_level_t debug_level = CSP_INFO;
#if (CSP_HAVE_LIBSOCKETCAN)
    const char * can_device = NULL;
#endif
    const char * rtable = NULL;
    int opt;
    while ((opt = getopt(argc, argv, "a:d:r:c:tR:h:q:m")) != -1) {
        switch (opt) {
            case 'a':
                address = atoi(optarg);
                break;
            case 'd':
                debug_level = atoi(optarg);
                break;
            case 'r':
                server_address = atoi(optarg);
                break;
#if (CSP_HAVE_LIBSOCKETCAN)
            case 'c':
                can_device = optarg;
                break;
#endif
            case 'R':
                rtable = optarg;
                break;
            case 'q':
                state = REFLECT;
                break;
            case 'm':
                state = TESTMESSAGE;
                break;
            default:
                printf("Usage:\n"
                       " -a <address>     local CSP address\n"
                       " -d <debug-level> debug level, 0 - 6\n"
                       " -r <address>     run client against server address\n"
                       " -c <can-device>  add CAN device\n"
                       " -q               return the same packet to the sender\n"
                       " -m               return a hardcoded testmessage to sender\n"
                       " -R <rtable>      set routing table\n");
                exit(1);
                break;
        }
    }

    /* enable/disable debug levels */
    for (csp_debug_level_t i = 0; i <= CSP_LOCK; ++i) {
        csp_debug_set_level(i, (i <= debug_level) ? true : false);
    }

    csp_log_info("Initialising CSP");

    /* Init CSP with address and default settings */
    csp_conf_t csp_conf;
    csp_conf_get_defaults(&csp_conf);
    csp_conf.address = address;
    int error = csp_init(&csp_conf);
    if (error != CSP_ERR_NONE) {
        csp_log_error("csp_init() failed, error: %d", error);
        exit(1);
    }

    /* Start router task with 10000 bytes of stack (priority is only supported on FreeRTOS) */
    csp_route_start_task(500, 0);

    /* Add interface(s) */
    csp_iface_t * default_iface = NULL;
#if (CSP_HAVE_LIBSOCKETCAN)
    if (can_device) {
        error = csp_can_socketcan_open_and_add_interface(can_device, CSP_IF_CAN_DEFAULT_NAME, 0, false, &default_iface);
        if (error != CSP_ERR_NONE) {
            csp_log_error("failed to add CAN interface [%s], error: %d", can_device, error);
            exit(1);
        }
    }
#endif
    if (rtable) {
        error = csp_rtable_load(rtable);
        if (error < 1) {
            csp_log_error("csp_rtable_load(%s) failed, error: %d", rtable, error);
            exit(1);
        }
    } else if (default_iface) {
        csp_rtable_set(CSP_DEFAULT_ROUTE, 0, default_iface, CSP_NO_VIA_ADDRESS);
    } else {
        /* no interfaces configured - run server and client in process, using loopback interface */
        server_address = address;
    }

    printf("Connection table\r\n");
    csp_conn_print_table();

    printf("Interfaces\r\n");
    csp_route_print_interfaces();

    printf("Route table\r\n");
    csp_route_print_table();

    /* Start server thread */
    if ((server_address == 255) ||  /* no server address specified, I must be server */
        (default_iface == NULL)) {  /* no interfaces specified -> run server & client via loopback */
        csp_thread_create(task_server, "SERVER", 1000, NULL, 0, NULL);
    }
    /* Wait for execution to end (ctrl+c) */
    while(1) {
        csp_sleep_ms(3000);
    }

    return 0;
}
