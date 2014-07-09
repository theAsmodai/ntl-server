#ifndef NTL_H
#define NTL_H

#include <stdatomic.h>
#include "const.h"

enum thread_signal_e
{
	ts_no,
	ts_pause,
	ts_exit
};

struct net_s;
struct db_s;
struct server_s;
struct thread_s;
struct server_s;
struct pipe_data_s;

typedef struct ntl_s
{
	atomic_int				threads_signal;
	lock_t					threads_lock;

	struct net_s*			net;
	struct db_s*			db;

	struct server_s*		servers;
	int						servers_count;

	struct thread_s*		threads;
	int						threads_count;

	struct server_s*		console;

#ifdef __windows__
	void*					hwnd;

	struct pipe_data_s*		pipes_data;

	void**					pipe_events;
	int						pipe_events_count;
#endif
} ntl_t;

void ntl_print( ntl_t* ntl, const char* fmt, ... );

#endif // NTL_H