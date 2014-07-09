#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#ifdef __linux__
#include <sys/epoll.h>
#include <stdbool.h>
#include <unistd.h>
#define EPOLL_ERROR -1
#endif

#include "client.h"
#include "net.h"
#include "const.h"
#include "config.h"
#include "database.h"
#include "servers.h"
#include "util.h"
#include "sys.h"
#include "ntl.h"

#ifdef __windows__
static int CALLBACK main_worker_thread( thread_t* thread )
{
	socket_t conn_sock;
	char buf[128];
	time_t curtime, last_connection;
	ntl_t* ntl;
	msg_t msg;
	net_t net;

	// copy for perfomance
	ntl = thread->ntl;
	memcpy( &net, ntl->net, sizeof net );
	msg.data = buf;
	last_connection = 0;
	
	// handle connections
	for(;;)
	{
		// check global signals for work threads
		switch( atomic_load( &ntl->threads_signal ) )
		{
		case ts_no:
			break;

		case ts_pause:
			atomic_store( &thread->paused, true );
			sys_wait_unlock( ntl->threads_lock, THREAD_TIMEOUT );
			atomic_store( &thread->paused, false );
			break;

		case ts_exit:
			return EXIT_SUCCESS;
		}

		if( conn_sock )
		{
			conn_sock = THREAD_NO_CLIENT;
		}
		else if( atomic_load( &thread->timeout ) )
		{
			sys_sleep( THREAD_TIMEOUT / 4 );
		}
		else
		{
			curtime = time( NULL );

			if( curtime - last_connection > THREAD_TIMEOUT )
				atomic_store( &thread->timeout, 1 );

			sys_sleep( THREAD_TIMEOUT / 50 );
		}

		// check for ready connection with message
		if( ( conn_sock = ( socket_t )atomic_load( &thread->conn_sock ) ) != THREAD_NO_CLIENT )
		{
			atomic_store( &thread->timeout, 0 );
			last_connection = time( NULL );
			msg.readcount = 0;
			msg.maxsize = net_recv( conn_sock, buf, sizeof buf );
			
			if( !client_read_message( conn_sock, &msg, ntl ) )
				net_closesocket( conn_sock );
			
			// set ready for next message
			atomic_store( &thread->conn_sock, THREAD_NO_CLIENT );
		}

		sys_sleep( 100 );
	}

	return EXIT_SUCCESS;
}

int CALLBACK pipe_reader_thread( struct server_s* server )
{
	char buf[8192];
	server_t* console;
	pipe_handle_t console_hdl;
	int bytes;

	console = server->ntl->console;
	console_hdl = server->ntl->console->rcon.streams.out.read;

	while( server->online )
	{
		bytes = sys_read_server_console( server, buf, sizeof buf );

		// if we connected to server console, forward to stdout
		if( console == server )
			fwrite( buf, 1, bytes, stdout );
	}

	return EXIT_SUCCESS;
}
#else
static int CALLBACK main_worker_thread( thread_t* thread )
{
	struct epoll_event ev, events[MAX_EVENTS], *pev, *end;
	socket_t conn_sock;
	int evcount, epollfd, n, count, thread_id;
	char buf[128];
	ntl_t* ntl;
	msg_t msg;
	net_t net;

	// copy for perfomance
	ntl = thread->ntl;
	memcpy( &net, ntl->net, sizeof net );
	msg.data = buf;
	thread_id = thread - ntl->threads;

	// create epoll
	if( ( epollfd = epoll_create1( 0 ) ) == EPOLL_ERROR )
	{
		perror( "epoll_create" );
		return EXIT_FAILURE;
	}

	// add main socket to epoll for incoming
	ev.events	= EPOLLIN;
	ev.data.fd	= net.listen_sock;

	if( epoll_ctl( epollfd, EPOLL_CTL_ADD, net.listen_sock, &ev ) == EPOLL_ERROR )
	{
		perror( "epoll_ctl::listen_sock" );
		return EXIT_FAILURE;
	}

	// handle connections
	for(;;)
	{
		// check global signals for work threads
		switch( atomic_load( &ntl->threads_signal ) )
		{
		case ts_no:
			break;

		case ts_pause:
			atomic_store( &thread->paused, true );
			sys_wait_unlock( ntl->threads_lock, THREAD_TIMEOUT );
			atomic_store( &thread->paused, false );
			break;

		case ts_exit:
			return EXIT_SUCCESS;
		}

		// wait for events
		if( ( evcount = epoll_wait( epollfd, events, sizeof events, THREAD_TIMEOUT ) ) == EPOLL_ERROR )
		{
			perror( "epoll_wait" );
			return EXIT_FAILURE;
		}

		// timeout without new events
		if( evcount == 0 )
		{
			atomic_store( &thread->timeout, true );
			continue;
		}

		// else have events
		atomic_store( &thread->timeout, false );

		for( pev = events, end = pev + evcount; pev < end; ++pev )
		{
			// new connection to main sock
			if( pev->data.fd == net.listen_sock )
			{
				// accept client with antiflood check
				if( ( conn_sock = net_accept( &net, thread_id ) ) == 0 )
					continue;

				// set nonblocking and add to epoll for reading
				if( net_setnonblocking( conn_sock ) )
				{
					ev.events	= EPOLLIN | EPOLLET;
					ev.data.fd	= conn_sock;

					if( epoll_ctl( epollfd, EPOLL_CTL_ADD, conn_sock, &ev ) == -1 )
					{
						perror( "epoll_ctl::conn_sock" );
						return EXIT_FAILURE;
					}
				}
			}
			else
			{
				// from connected client
				if( pev->events & EPOLLIN ) // we waited incoming message completition
				{
					count = net_recv( pev->data.fd, buf, sizeof buf );

					if( count == -1 )
						perror( "net_recv::conn_sock" );

					else if( count ) // have data
					{
						msg.readcount	= 0;
						msg.maxsize		= count;

						if( client_read_message( pev->data.fd, &msg, ntl ) )
						{
							ev.events	= EPOLLOUT | EPOLLET; // now we wait out message completition for socket close
							ev.data.fd	= conn_sock;

							if( epoll_ctl( epollfd, EPOLL_CTL_MOD, pev->data.fd, &ev ) == -1 ) // EPOLLIN -> EPOLLOUT
							{
								perror( "epoll_ctl::conn_sock" );
								return EXIT_FAILURE;
							}
						}
						continue;
					}
				}
				// else EPOLLOUT (or disconnected, or error)
				net_closesocket( pev->data.fd );
			}
		}
	}

	return EXIT_SUCCESS;
}

static int CALLBACK pipe_reader_thread( ntl_t* ntl )
{
	struct epoll_event ev, *events;
	int evcount, epollfd, n, console_fd, cnt;
	char buf[CONSOLE_BUFSIZE];

	// create epoll
	if( ( epollfd = epoll_create1( 0 ) ) == EPOLL_ERROR )
	{
		perror( "epoll_create" );
		return EXIT_FAILURE;
	}

	events = calloc( ntl->servers_count, sizeof( struct epoll_event ) );
	ev.events = EPOLLIN; // we need only read. close() removes fd from all epolls.

	// add servers stdout to epoll
	for( n = 0, evcount = 0; n < ntl->servers_count; ++n )
	{
		if( ntl->servers[n].process )
		{
			ev.data.fd = ntl->servers[n].rcon.streams.out;

			if( epoll_ctl( epollfd, EPOLL_CTL_ADD, ntl->servers[n].rcon.streams.out, &ev ) == EPOLL_ERROR )
			{
				perror( "epoll_ctl::pipe" );
				return EXIT_FAILURE;
			}
		}
	}

	// handle output
	for(;;)
	{
		// check global signals for threads
		if( atomic_load( &ntl->threads_signal ) == ts_exit )
			return EXIT_SUCCESS;

		// wait for events
		if( ( evcount = epoll_wait( epollfd, events, sizeof events, THREAD_TIMEOUT ) ) == EPOLL_ERROR )
		{
			perror( "epoll_wait" );
			return EXIT_FAILURE;
		}

		// timeout without new events
		if( evcount == 0 )
			continue;

		console_fd = ntl->console->rcon.streams.out;

		for( n = 0; n < evcount; ++n )
		{
			cnt = read( events[n].data.fd, buf, sizeof buf - 1 );

			// if we connected to server console, forward to stdout
			if( events[n].data.fd == console_fd )
				write( STDOUT_FILENO, buf, cnt );
		}
	}

	return EXIT_SUCCESS;
}
#endif

static int CALLBACK servers_check_thread( ntl_t* ntl )
{
	server_t *srv, *end;
	end = ntl->servers + ntl->servers_count;

	for( srv = ntl->servers; srv < end; ++srv )
	{
		if( atomic_load( &ntl->threads_signal ) == ts_exit )
			return EXIT_SUCCESS;

		if( srv->local || srv->online )
			continue;

		net_server_connect( srv );
	}

	return EXIT_SUCCESS;
}

static int CALLBACK service_thread( ntl_t* ntl )
{
	server_t *srv, *s_end;
	//thread_t *thread, *t_end;
	time_t last_check;
	int all_paused, count;

	s_end = ntl->servers + ntl->servers_count;
	//t_end = ntl->threads + ntl->threads_count;

	while( 1 )
	{
		if( atomic_load( &ntl->threads_signal ) == ts_exit )
			return EXIT_SUCCESS;

		clients_check_timeout( ntl, CONNECT_TIMEOUT );

		/*sys_lock( ntl->threads_lock );
		atomic_store( &ntl->threads_signal, ts_pause );

		all_paused = 1;
		for( thread = ntl->threads; thread < t_end; ++thread )
		{
			if( atomic_load( &thread->paused ) == 0 )
			{
				all_paused = 0;
				break;
			}			
		}

		if( all_paused )
		{
			atomic_store( &ntl->threads_signal, ts_no );
			//mem_defrag();
			sys_unlock( &ntl->threads_lock );
		}*/

		count = 0;
		for( srv = ntl->servers; srv < s_end; ++srv )
		{
			if( !srv->local && !srv->online )
				++count;
		}

		if( count )
		{
			sys_create_thread( ( void * )servers_check_thread, ( void * )ntl );
		}

		sys_sleep( THREAD_TIMEOUT );
	}
}

int main()
{
	int threads_count, i, running, exit_code;
	config_t cfg, settings, servers, srv;
	ntl_t ntl;
	net_t net;
	char line[MAX_INPUT_LEN];

	exit_code = EXIT_FAILURE;
	cfg = settings = NULL;
	memset( ( void * )&ntl, 0, sizeof( ntl_t ) );
	memset( ( void * )&net, 0, sizeof( net_t ) );

	do // loading
	{
		sys_lock_init( &ntl.threads_lock );

		// load config
		if( ( cfg = config_load( CONFIG_FILE ) ) == NULL )
			break;

		// get settings section
		if( ( settings = xml_get_sub( cfg, "settings" ) ) == NULL )
			break;

		// get threads count from config, or set equal cpu cores
		if( ( threads_count = xml_get_int( settings, "threads" ) ) == 0 )
			threads_count = sys_get_cpu_cores();

		// connect to database
		if( ( ntl.db = db_init( settings ) ) == NULL )
			break;

		// init network
		if( !net_init( &net, xml_get_string( settings, "host" ), xml_get_int( settings, "port" ), threads_count ) )
			break;

		// link net to ntl
		ntl.net = &net;

		// get settings section
		if( ( servers = xml_get_sub( cfg, "servers" ) ) == NULL )
			break;

		// init servers
		ntl.servers_count	= xml_get_sub_count( servers );
		ntl.servers			= ( server_t * )calloc( threads_count, sizeof( server_t ) );
		srv					= xml_get_sub( servers, NULL );
		i					= 0;
		running				= 0;

		do
		{
			if( !server_init( ntl.servers + i, srv ) )
				fprintf( stderr, "can't init server '%s'\n", xml_get_name( srv ) );
			else
			{
				ntl.servers[i].ntl = &ntl;
				++running;
			}
		}
		while( ( srv = xml_get_next( srv ) ) != NULL );

		printf( "Started service %i of %i servers\n", running, ntl.servers_count );

		// start working threads
		ntl.threads_count	= threads_count;
		ntl.threads			= ( thread_t * )calloc( threads_count, sizeof( thread_t ) );

		for( i = 0; i < threads_count; ++i )
		{
			atomic_store( &ntl.threads[i].paused, false );
			atomic_store( &ntl.threads[i].timeout, false );
			atomic_store( &ntl.threads[i].conn_sock, 0 );
			ntl.threads[i].ntl = &ntl;

			if( !sys_create_workthread( &ntl.threads[i], main_worker_thread ) )
				break;
		}
		if( i != threads_count )
			break;

		printf( "Runned %i work threads\n", threads_count );

		// run service threads


		// finally run auth server
		if( !net_run( &net, &ntl ) )
			break;

		// server started properly
		exit_code = EXIT_SUCCESS;

		// wait for console commands
		while( fgets( line, sizeof line, stdin ) > 0 )
		{
			if( !strncmp( line, "switch", 6 ) )
			{
				ntl.console = server_find_id( ntl.servers, ntl.servers_count, line + 6 + 1 );
				printf( "console swithed to %s\n", ntl.console ? ntl.console->id : "main console" );
			}
			else if( ntl.console ) // connected to server console
			{
				server_command( ntl.console, line ); // forward to server console
			}
			else // main console
			{
				if( !strncmp( line, "stop", 4 ) )
				{
					
				}
			}
		}
	} while( 0 );

	// deinit
	sys_lock_deinit( &ntl.threads_lock );
	config_close( cfg );
	db_close( ntl.db );
	net_close( &net );

	if( exit_code == EXIT_FAILURE )
	{
		puts( "press enter to exit..." );
		getchar();
	}

	return exit_code;
}

void ntl_print( ntl_t* ntl, const char* fmt, ... )
{
	va_list argptr;
	char date[64];
	time_t td;
	FILE* fp;

	time( &td );
	strftime( date, sizeof date, "%m/%d/%Y - %H:%M:%S", localtime( &td ) );
	printf( "%s [INFO]: ", strchr( date, '-' ) + 2 );
	va_start( argptr, fmt );

	if( ntl->console == NULL )
		vprintf( fmt, argptr );

	if(( fp = fopen( LOG_FILE, "a+" ) ))
	{
		vfprintf( fp, "%s [INFO]: ", date );
		vfprintf( fp, fmt, argptr );
		fclose( fp );
	}

	va_end( argptr );
}