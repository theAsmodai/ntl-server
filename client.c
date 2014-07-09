#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "client.h"
#include "client_list.h"
#include "atomic_list.h"
#include "database.h"
#include "protocol.h"
#include "servers.h"
#include "const.h"
#include "util.h"
#include "mem.h"
#include "ntl.h"
#include "net.h"
#include "dbg.h"

const client_t* client_find( const struct net_clients_s clients, ip_t ip )
{
	const atomic_list_t*	list;
	const client_t*			cl;

	for( list = clients.lists; list < clients.end; ++list )
	{
		cl = al_head( list );

		if( cl->ip == ip )
		{
			return al_head_valid( list ) ? cl : NULL;
		}

		while( cl = al_next( cl ), cl )
		{
			if( cl->ip == ip )
				return cl;
		}
	}

	return NULL;
}

void client_add( atomic_list_t* list, unsigned ip )
{
	client_t* cl	= mem_alloc_client();
	cl->ip			= ip;
	cl->player		= NULL;
	cl->conntime	= time( NULL );

	al_push( list, cl );
	atomic_store( &list->head_valid, true );
	dbg( "client added: %i.%i.%i.%i time %i\n", IP_TO_ARGS( ip ), cl->conntime );
}

void clients_check_timeout( struct ntl_s* ntl, long timeout )
{
	const atomic_list_t*	list;
	const atomic_list_t*	end;
	client_t*				prev;
	client_t*				cl;
	player_t*				player;
	long					remove;
	long					div3remove;

	remove = time( NULL ) - timeout;
	div3remove = remove / 3;

	for( list = ntl->net->clients.lists, end = ntl->net->clients.end; list < end; ++list )
	{
		prev = al_head( list );

		// check timeout for head. we can't safety remove it, only set invalid flag.
		if( prev->conntime <= remove )
			atomic_store( &list->head_valid, false );

		// check other clients and unlink timeouted. workthreads can write only head client.
		for( cl = al_next( prev ); cl; prev = cl, cl = al_next( prev ) )
		{
			if( cl->conntime <= ( cl->player ? remove : div3remove ) )
			{
				dbg( "client remove: %i.%i.%i.%i time %i now %i\n", cl->ip & 0xFF000000, cl->ip & 0xFF0000, cl->ip & 0xFF00, cl->ip & 0xFF, cl->conntime, time( NULL ) );
				al_remove( prev, cl );
				player = cl->player;
				mem_free_client( cl );

				if( cl->player )
				{
					server_command( ntl->servers + player->server, "whitelist remove %s", player->name );
					free( ( void *)player );
				}
			}
		}
	}
}

int client_read_message( socket_t sock, msg_t* msg, ntl_t* ntl )
{
	ip_t			sv_ip;
	int				sv_port;
	server_t*		server;
	user_t			user;
	const char*		hwid;
	int				res;

	dbg( "msg begin reading: len %i\n", msg->maxsize );

	switch( msg_get_uint( msg, 0 ) )
	{
	case ntl_login:
		if( ( hwid = msg_get_string( msg, MAX_HWID_LEN ) ) == NULL )
			return NO_ANSWER;

		if( db_is_hwid_banned( ntl->db, hwid ) )
			return net_send_answer( sock, ntle_you_are_banned );

		if( !( sv_ip = msg_get_uint( msg, 0 ) ) || !( sv_port = msg_get_ushort( msg, 0 ) ) )
			return NO_ANSWER;

		if( !( server = server_find( ntl->servers, ntl->servers_count, sv_ip, sv_port ) ) )
			return net_send_answer( sock, ntle_invalid_server );

		user.server = server - ntl->servers;

		if(
			( user.login	= msg_get_string( msg, MAX_PLAYER_NAME ) ) != NULL &&
			( user.password = msg_get_string( msg, MAX_PASS_LEN ) ) != NULL
		  )
		{
			if( ( res = db_login_user( ntl->db, &user ) ) == ntle_no_error )
			{
				ntl_print( ntl, "%s logged in.\n", user.login );
				client_connected( ntl, &user );
			}
			else
				ntl_print( ntl, "%s login rejected.\n", user.login );

			return net_send_answer( sock, res );
		}
		break;

	case ntl_register:
		if( ( hwid = msg_get_string( msg, MAX_HWID_LEN ) ) == NULL )
			return NO_ANSWER;

		if( db_is_hwid_banned( ntl->db, hwid ) )
			return net_send_answer( sock, ntle_you_are_banned );

		if(
			( user.login	= msg_get_string( msg, MAX_PLAYER_NAME ) ) != NULL &&
			( user.password	= msg_get_string( msg, MAX_PASS_LEN ) ) != NULL &&
			( user.mail		= msg_get_string( msg, MAX_EMAIL_LEN ) ) != NULL
		  )
		{
			user.hour = time( NULL ) / ( 60 * 60 );
			user.ip = net_get_ip( sock );

			if( ( res = db_register_user( ntl->db, &user ) ) == ntle_no_error )
				ntl_print( ntl, "New registration: login: %s email: %s.\n", user.login, user.mail );
			
			return net_send_answer( sock, res );
		}
		break;

	default:
		break;
	}

	return NO_ANSWER;
}

void client_connected( ntl_t* ntl, user_t* user )
{
	client_t* cl;
	player_t* player;

	server_command( ntl->servers + user->server, "whitelist add %s", user->login );

	cl = ( client_t * )client_find( ntl->net->clients, user->ip ); // :( ,h

	if( !cl ) // maybe impossible
		return;

	player = ( player_t *)malloc( sizeof( player_t ) );
	strcpy( player->name, user->login );
	player->zt		= 0;
	player->server	= user->server;

	cl->player = player;
}