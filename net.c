#ifdef __windows__
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#pragma comment( lib, "ws2_32.lib" )
#else
	#define _GNU_SOURCE
	#include <unistd.h>
	#include <fcntl.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <arpa/inet.h>
	#include <netinet/in.h>
	#include <errno.h>
	#include <netdb.h>
#endif

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "client.h"
#include "client_list.h"
#include "protocol.h"
#include "servers.h"
#include "net.h"
#include "ntl.h"
#include "sys.h"
#include "dbg.h"

static void net_error( const char* err_msg )
{
#ifdef __windows__
	fprintf( stderr, "%s: error %i\n", err_msg, WSAGetLastError() );
	#define ETIMEDOUT               WSAETIMEDOUT
#else
	perror( err_msg );
	#define SOCKET_ERROR		-1
	#define INVALID_SOCKET		SOCKET_ERROR
	#define SD_BOTH				SHUT_RDWR
#endif
}

static int _inet_aton( cp_arg, addr )
const char *cp_arg;
struct in_addr *addr;
{
	register const byte *cp = cp_arg;
	register dword val;
	register int base;
	register dword n;
	register byte c;
	dword parts[4];
	register dword *pp = parts;

	for(;;)
	{
		/*
		* Collect number up to ``.''.
		* Values are specified as for C:
		* 0x=hex, 0=octal, other=decimal.
		*/
		val = 0; base = 10;
		if( *cp == '0' )
		{
			if( *++cp == 'x' || *cp == 'X' )
				base = 16, cp++;
			else
				base = 8;
		}
		while( ( c = *cp ) != '\0' )
		{
			if( isascii( c ) && isdigit( c ) )
			{
				val = ( val * base ) + ( c - '0' );
				cp++;
				continue;
			}
			if( base == 16 && isascii( c ) && isxdigit( c ) )
			{
				val = ( val << 4 ) +
					( c + 10 - ( islower( c ) ? 'a' : 'A' ) );
				cp++;
				continue;
			}
			break;
		}
		if( *cp == '.' )
		{
			/*
			* Internet format:
			*	a.b.c.d
			*	a.b.c	(with c treated as 16-bits)
			*	a.b	(with b treated as 24 bits)
			*/
			if( pp >= parts + 3 || val > 0xff )
				return ( 0 );
			*pp++ = val, cp++;
		}
		else
			break;
	}
	/*
	* Check for trailing characters.
	*/
	if( *cp && ( !isascii( *cp ) || !isspace( *cp ) ) )
		return ( 0 );
	/*
	* Concoct the address according to
	* the number of parts specified.
	*/
	n = pp - parts + 1;
	switch( n )
	{
	case 1:				/* a -- 32 bits */
		break;

	case 2:				/* a.b -- 8.24 bits */
		if( val > 0xffffff )
			return ( 0 );
		val |= parts[0] << 24;
		break;

	case 3:				/* a.b.c -- 8.8.16 bits */
		if( val > 0xffff )
			return ( 0 );
		val |= ( parts[0] << 24 ) | ( parts[1] << 16 );
		break;

	case 4:				/* a.b.c.d -- 8.8.8.8 bits */
		if( val > 0xff )
			return ( 0 );
		val |= ( parts[0] << 24 ) | ( parts[1] << 16 ) | ( parts[2] << 8 );
		break;
	}
	if( addr )
		addr->s_addr = val; //htonl( val ); why htonl???
	return ( 1 );
}

static struct in_addr net_host_to_addr( const char* host )
{
	struct in_addr addr;
	struct hostent* h;

	addr.s_addr = INADDR_ANY;

	if( host && !_inet_aton( host, &addr ) )
	{
		if( ( h = gethostbyname( host ) ) == NULL )
		{
			switch( h_errno )
			{
			case HOST_NOT_FOUND:
				fprintf( stderr, "gethostbyname: host %s not found\n", host );
				break;

			case NO_DATA:
				fprintf( stderr, "gethostbyname: no data for host %s\n", host );
				break;

			default:
				fprintf( stderr, "gethostbyname: error %i for host %s\n", h_errno, host );
			}

			addr.s_addr = INVALID_IP;
		}
		else
		{
			addr.s_addr = *( ip_t *)h->h_addr_list[0];
			dbg( "host %s = %i.%i.%i.%i\n", host, IP_TO_ARGS( addr.s_addr ) );
		}
	}

	return addr;
}

static socket_t net_listen( const char* host, int port )
{
	struct sockaddr_in addr;
	socket_t listen_sock;

	if( !port )
	{
		fprintf( stderr, "net_listen: invalid port\n" );
		return 0;
	}

	memset( &addr, 0, sizeof addr );
	addr.sin_family	= AF_INET;
	addr.sin_port	= htons( port );
	addr.sin_addr	= net_host_to_addr( host );

	if( ( listen_sock = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ) == INVALID_SOCKET )
	{
		net_error( "socket open" );
		return 0;
	}

	if( net_setnonblocking( listen_sock ) == 0 )
	{
		net_closesocket( listen_sock );
		return 0;
	}

	if( bind( listen_sock, ( struct sockaddr *)&addr, sizeof addr ) == SOCKET_ERROR )
	{
		net_closesocket( listen_sock );
		net_error( "bind" );
		return 0;
	}

	if( listen( listen_sock, 3 ) == SOCKET_ERROR )
	{
		net_closesocket( listen_sock );
		net_error( "listen" );
		return 0;
	}

	printf( "Started listening on %i.%i.%i.%i:%i\n", IP_TO_ARGS( addr.sin_addr.s_addr ), addr.sin_port );
	return listen_sock;
}

int net_init( net_t* net, const char* host, int port, int threads_count )
{
#ifdef __windows__
	int err;
	struct WSAData wsa;

	if( ( err = WSAStartup( WINSOCK_VERSION, &wsa ) ) != NO_ERROR )
	{
		fprintf( stderr, "WSAStartup failed with error: %i\n", err );
		return 0;
	}
#endif

	net->listen_sock		= net_listen( host, port );
	net->clients.lists		= ( struct atomic_list_s *)calloc( threads_count, sizeof( atomic_list_t ) );
	net->clients.end		= net->clients.lists + threads_count;

	return net->listen_sock ? 1 : 0;
}

void net_close( net_t* net )
{
	if( net )
	{
		net_closesocket( net->listen_sock );
		free( ( void *)net->clients.lists );
		memset( ( void * )net, 0, sizeof( net_t ) );
	}
#ifdef __windows__
	WSACleanup();
#endif
}

int net_recv( socket_t sock, char* data, int len )
{
	//you can implement decryption here
	return recv( sock, data, len, 0 );
}

int net_send( socket_t sock, const char* data, int len )
{
	//you can implement encryption here
	return send( sock, data, len, 0 );
}

int net_closesocket( socket_t sock )
{
#ifdef __windows__
	return closesocket( sock );
#else
	return close( sock );
#endif
}

#ifdef __windows__
static LRESULT CALLBACK net_socket_wnd_proc( HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam )
{
	ntl_t* ntl;
	thread_t *t, *threads_end;
	socket_t conn_sock;

	if( iMsg <= WM_USER )
		return DefWindowProc( hWnd, iMsg, wParam, lParam );

	switch( WSAGETSELECTEVENT( lParam ) )
	{
	case FD_ACCEPT: // new connection
		ntl = ( ntl_t * )GetWindowLongPtrA( hWnd, GWL_USERDATA );
		conn_sock = net_accept( ntl->net, 0 );
		WSAAsyncSelect( conn_sock, hWnd, iMsg, FD_READ | FD_CLOSE );
		break;

	case FD_READ: // incoming data
		WSAAsyncSelect( wParam, hWnd, iMsg, FD_WRITE | FD_CLOSE ); // only 1 message per connect
		ntl = ( ntl_t * )GetWindowLongPtrA( hWnd, GWL_USERDATA );
		threads_end = ntl->threads + ntl->threads_count;

		// find free thread to handle this message
		for(;;)
		{
			for( t = ntl->threads; t < threads_end; ++t )
			{
				if( !atomic_load( &t->conn_sock ) )
				{
					atomic_store( &t->conn_sock, wParam );
					return EXIT_SUCCESS;
				}
			}
			Sleep( THREAD_CHECK_FREE );
		}
		break;

	case FD_WRITE: // end of writing
	case FD_CLOSE:
		net_closesocket( wParam );
		break;
	}

	return EXIT_SUCCESS;
}

int net_run( net_t* net, ntl_t* ntl )
{
	HWND hWnd;

	hWnd = sys_create_window( net_socket_wnd_proc );
	SetWindowLongPtr( hWnd, GWL_USERDATA, ( long )ntl );

	if( WSAAsyncSelect( net->listen_sock, hWnd, WM_SOCKET_MSG, FD_ACCEPT ) == SOCKET_ERROR )
	{
		fprintf( stderr, "WSAAsyncSelect: error %i\n", WSAGetLastError() );
		return 0;
	}

	ntl->hwnd = ( void *)hWnd;
	return 1;
}
#else
int net_run( net_t* net, ntl_t* ntl )
{

}
#endif

socket_t net_accept( net_t* net, int thread_id )
{
	socket_t conn_sock;
	struct sockaddr_in addr;
	const client_t* client;
	int addrlen;

	addrlen = sizeof addr;
	conn_sock = accept( net->listen_sock, ( struct sockaddr *)&addr, &addrlen );

	if( conn_sock == INVALID_SOCKET )
	{
#ifdef __windows__
		if( WSAGetLastError() != WSAEWOULDBLOCK )
#else
		if( errno != EAGAIN && errno != EWOULDBLOCK )
#endif
			net_error( "client accept" );
		return 0;
	}

	client = client_find( net->clients, addr.sin_addr.s_addr );

	if( client )
	{
		if( !client->player )
		{
			dbg( "client %i.%i.%i.%i not accepted\n", IP_TO_ARGS( addr.sin_addr.s_addr ) );
			net_closesocket( conn_sock );
			conn_sock = 0;
		}
	}
	else
		client_add( net->clients.lists + thread_id, addr.sin_addr.s_addr );

	return conn_sock;
}

int net_setnonblocking( socket_t sock )
{
#ifdef __windows__
	int err;
	int nonblocking = 1;

	if( ( err = ioctlsocket( sock, FIONBIO, &nonblocking ) ) != NO_ERROR )
	{
		fprintf( stderr, "setnonblocking error %i\n", err );
		return 0;
	}
#else
	if( fcntl( sock, F_SETFL, fcntl( sock, F_GETFD, 0 ) | O_NONBLOCK ) == -1 )
	{
		perror( "setnonblocking" );
		return 0;
	}
#endif

	return 1;
}

static void net_set_timeout( socket_t sock, dword msec )
{
#ifdef __windows__
	setsockopt( sock, SOL_SOCKET, SO_SNDTIMEO, ( const char *)&msec, sizeof( dword ) );
	setsockopt( sock, SOL_SOCKET, SO_RCVTIMEO, ( const char *)&msec, sizeof( dword ) );
#else
	struct timeval timeout;

	timeout.tv_sec = 0;
	timeout.tv_usec = msec * 1000;

	setsockopt( sock, SOL_SOCKET, SO_SNDTIMEO, &msec, sizeof( dword ) );
	setsockopt( sock, SOL_SOCKET, SO_RCVTIMEO, &msec, sizeof( dword ) );
#endif
}

int net_server_connect( server_t* server )
{
	struct sockaddr_in addr;
	int res, echo;

	if( server->online )
		return 1;

	memset( &addr, 0, sizeof addr );
	addr.sin_family = AF_INET;
	addr.sin_port = htons( server->port );
	addr.sin_addr.s_addr = server->ip;

	if( connect( server->rcon.net.sock, ( struct sockaddr *)&addr, sizeof( struct sockaddr_in ) ) == SOCKET_ERROR )
	{
		net_error( "connect" );
		return 0;
	}

	net_set_timeout( server->rcon.net.sock, SRV_CONN_TIMEOUT );
	echo = ntl_echo;

	switch( net_send( server->rcon.net.sock, ( const char *)&echo, sizeof echo ) )
	{
	case -1:
		if( h_errno != ETIMEDOUT )
			net_error( "send echo" );
	default:
		shutdown( server->rcon.net.sock, SD_BOTH );
		return 0;

	case sizeof echo:
		break;
	}

	switch( net_recv( server->rcon.net.sock, ( char *)&echo, sizeof echo ) )
	{
	case -1:
		if( h_errno != ETIMEDOUT )
			net_error( "send echo" );
	default:
		shutdown( server->rcon.net.sock, SD_BOTH );
		return 0;

	case sizeof echo:
		break;
	}

	if( ( server->online = echo == ntl_echo ) != 0 )
		ntl_print( server->ntl, "[SERVERS]: Successfully connected to server %s.\n", server->id );

	shutdown( server->rcon.net.sock, SD_BOTH );
	return server->online;
}

int net_server_command( server_t* server, const char* command )
{
	char buf[MAX_SRVCMD_LEN];
	int header_len, command_len, full_len;

	header_len = server->rcon.net.header_len;
	command_len = strlen( command ) + 1;
	full_len = header_len + command_len;

	if( full_len > sizeof buf )
	{
		fprintf( stderr, "net_send_command: command too long (%i)\n", full_len );
		return 0;
	}

	memcpy( buf, server->rcon.net.header, header_len );
	memcpy( buf + header_len, command, command_len );
	dbg( "server command: %s", command ); // no \n
	
	return net_send( server->rcon.net.sock, buf, full_len );
}

int net_send_answer( socket_t sock, int code )
{
	int msg[2];

	msg[0] = ntl_answer;
	msg[1] = code;

	return net_send( sock, ( const char *)msg, sizeof msg );
}

ip_t net_get_ip( socket_t sock )
{
	struct in_addr addr;
	int len;

	len = sizeof addr;

	if( getpeername( sock, ( struct sockaddr *)&addr, &len ) == SOCKET_ERROR )
	{
		net_error( "getpeername" );
		return INVALID_IP;
	}

	return addr.s_addr;
}

ip_t net_host_to_ip( const char* host )
{
	return net_host_to_addr( host ).s_addr;
}