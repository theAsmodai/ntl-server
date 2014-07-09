#ifndef NET_H
#define NET_H

#include "const.h"
#include "net_clients.h"

#define IP_TO_ARGS( u )	u >> 24, ( u >> 16 ) & 0xFF, ( u >> 8 ) & 0xFF, u & 0xFF

typedef struct net_s
{
	socket_t		listen_sock;
	net_clients_t	clients;
} net_t;

struct server_s;

int net_init( net_t* net, const char* host, int port, int threads_count );
void net_close( net_t* net );
int net_recv( socket_t sock, char* data, int len );
int net_send( socket_t sock, const char* data, int len );
int net_closesocket( socket_t sock );
int net_run( net_t* net, struct ntl_s* ntl );
socket_t net_accept( net_t* net, int thread_id );
int net_setnonblocking( socket_t sock );
int net_server_connect( struct server_s* server );
int net_server_command( struct server_s* server, const char* command );
int net_send_answer( socket_t sock, int code );
ip_t net_get_ip( socket_t sock );
ip_t net_host_to_ip( const char* host );

#endif // NET_H