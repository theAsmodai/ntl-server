#ifndef SYS_H
#define SYS_H

#include <stdatomic.h>
#include "const.h"

typedef struct thread_s
{
	atomic_bool				paused;
	atomic_bool				timeout;
	atomic_int				conn_sock;
	struct ntl_s*			ntl;
	thread_handle_t			handle;
	char					__dontusebuf[CPU_CACHE_LINE - 5 * sizeof( int )];
} thread_t;

struct ntl_s;

void sys_lock_init( lock_t lock );
void sys_lock_deinit( lock_t lock );
void sys_lock( lock_t lock );
void sys_unlock( lock_t lock );
int sys_wait_unlock( lock_t lock, unsigned msec );
thread_handle_t sys_create_thread( void* handler, void* arg );
int sys_create_workthread( thread_t* thread, thread_routine_t handler );
int sys_get_cpu_cores();
void sys_sleep( dword msec );

int sys_run_server( char* cmdline, server_t* server );
int sys_write_server_console( server_t* server, const char* command );
int sys_read_server_console( server_t* server, char* output, int maxlen );

#ifdef _WIN32
#ifdef DECLARE_HANDLE
HWND sys_create_window( WNDPROC wnd_proc );
#endif
#endif

#endif // SYS_H