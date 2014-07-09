#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "protocol.h"
#include "servers.h"
#include "config.h"
#include "sys.h"
#include "net.h"

#define TRY_READ_INT( x )		if( !memcmp( xml.key, #x, sizeof #x ) server->##x = atoi( xml.key );
#define TRY_READ_BOOL( x )		if( !memcmp( xml.key, #x, sizeof #x ) server->##x = memcmp( xml.key, "true", 4 ) ? 0 : 1;
#define TRY_READ_FLOAT( x )		if( !memcmp( xml.key, #x, sizeof #x ) server->##x = atof( xml.key );
#define TRY_READ_STRING( x )	if( !memcmp( xml.key, #x, sizeof #x ) strncpy( server->##x, xml.key, sizeof server->##x - 1 );

server_t* server_find( server_t* servers, int count, ip_t ip, int port )
{
	server_t *srv, *end;

	for( srv = servers, end = srv + count; srv < end; ++srv )
	{
		if( ( srv->ip == ip || !srv->ip ) && srv->port == port )
			return srv;
	}

	return NULL;
}

server_t* server_find_id( server_t* servers, int count, const char* id )
{
	server_t *srv, *end;

	for( srv = servers, end = srv + count; srv < end; ++srv )
	{
		if( !strcmp( srv->id, id ) )
			return srv;
	}

	return NULL;
}

int server_command( server_t* server, const char* fmt, ... )
{
	char buf[MAX_SRVCMD_LEN];
	va_list argptr;
	int len;

	va_start( argptr, fmt );
	len = vsnprintf( buf, sizeof buf - 3, ( void *)fmt, argptr );
	va_end( argptr );

	buf[len] = '\r'; buf[len + 1] = '\n'; buf[len + 2] = '\0';

	if( server->process )
		return sys_write_server_console( server, buf );
	
	return net_server_command( server, buf );
}

int server_init( server_t* server, config_t cfg )
{
	char* sz;

	strncpy( server->id, xml_get_name( cfg ), sizeof server->id - 1 );

	server->ip = net_host_to_ip( xml_get_string( cfg, "host" ) );
	server->port = xml_get_int( cfg, "port" );

	if( !server->port )
	{
		fprintf( stderr, "invalid server %s for '%s'\n", "port", server->id );
		return 0;
	}

	if( ( sz = xml_get_string( cfg, "version" ) ) == XML_INVALID_STRING )
	{
		fprintf( stderr, "invalid server %s for '%s'\n", "version", server->id );
		return 0;
	}
	strncpy( server->version, sz, MAX_VERSION_LEN );

	if( ( sz = xml_get_string( cfg, "name" ) ) == XML_INVALID_STRING )
	{
		fprintf( stderr, "invalid server %s for '%s'\n", "name", server->id );
		return 0;
	}
	strncpy( server->name, sz, MAX_SERVER_NAME );

	server->local = xml_get_bool( cfg, "local" );

	if( server->local )
	{
		sz = xml_get_string( cfg, "launch_params" );

		if( !sz )
		{
			fprintf( stderr, "no launch_params for '%s'\n", server->id );
			return 0;
		}

		memset( &server->rcon.streams, 0, sizeof server->rcon.streams );
		sys_run_server( sz, server );
		server->online = 1;
		printf( "launched server '%s'\n", server->id );
	}
	else
	{
		if( !server->ip )
		{
			fprintf( stderr, "invalid server %s for '%s'\n", "ip", server->id );
			return 0;
		}

		if( !( sz = xml_get_string( cfg, "rc_password" ) ) )
		{
			fprintf( stderr, "invalid server %s for '%s'\n", "remote console password", server->id );
			return 0;
		}

		*( int * )server->rcon.net.header = ntl_command;
		server->rcon.net.header_len = snprintf( server->rcon.net.header + 4, MAX_SERVER_PASSWORD, "%s", sz ) + 4;
		
		if( net_server_connect( server ) )
			printf( "connected to '%s'\n", server->id );
		else
			printf( "can't connect to '%s'\n", server->id );
	}

	return 1;
}