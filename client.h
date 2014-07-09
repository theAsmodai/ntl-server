#ifndef CLIENT_H
#define CLIENT_H

#include <stdatomic.h>
#include "const.h"
#include "net_clients.h"

typedef struct player_s
{
	char		name[MAX_PLAYER_NAME];
	word		zt;
	word		server;
} player_t;

typedef struct client_s
{
	ip_t		ip;
	long		conntime;
	player_t*	player;
	atomic_int	next;
} client_t;

struct atomic_list_s;
struct user_s; 
struct ntl_s;
struct msg_s;

const client_t* client_find( struct net_clients_s clients, ip_t ip);
void client_add( struct atomic_list_s* list, ip_t ip );
void clients_check_timeout( struct ntl_s* ntl, long timeout );
int client_read_message( socket_t sock, struct msg_s* msg, struct ntl_s* ntl );
void client_connected( struct ntl_s* ntl, struct user_s* user );

#endif // CLIENT_H