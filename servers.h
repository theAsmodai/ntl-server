#ifndef SERVERS_H
#define SERVERS_H

#include "const.h"

#ifdef __windows__
	typedef struct server_pipe_s
	{
		pipe_handle_t write;
		pipe_handle_t read;
	} server_pipe_t;
#else
	typedef pipe_handle_t server_pipe_t;
#endif

typedef union rcon_u
{
	struct
	{
		socket_t	sock;
		char		header[MAX_SERVER_PASSWORD + 1 + 4];
		int			header_len;
	} net;

	struct
	{
		server_pipe_t in;
		server_pipe_t out;
#ifdef __windows__
		thread_handle_t console_reader;
#endif
	} streams;
} rcon_t;

struct ntl_s;
struct xml_s;

typedef struct server_s
{
	ip_t			ip;
	int				port;
	int				local;
	int				online;
	char			version[MAX_VERSION_LEN + 1];
	char			id[MAX_SERVER_ID + 1];
	char			name[MAX_SERVER_NAME + 1];
	process_t		process;
	rcon_t			rcon;
	struct ntl_s*	ntl;
} server_t;

server_t* server_find( server_t* servers, int count, ip_t ip, int port );
server_t* server_find_id( server_t* servers, int count, const char* id );
int server_command( server_t* server, const char* fmt, ... );
int server_init( server_t* server, struct xml_s* cfg );

#endif // SERVERS_H