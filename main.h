#ifndef MAIN_H
#define MAIN_H

#include "atomic.h"

typedef struct ntl_s
{
	atomic_flag		threads_pause;
	atomic_flag		threads_stop;
	mutex_t			threads_lock;

	net_t			net;
	servers_t		servers;
	MYSQL			mysql;
} ntl_t;

#endif // MAIN_H