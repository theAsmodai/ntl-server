#ifdef __windows__
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
typedef void* ( *PTHREAD_START_ROUTINE )( void * );
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "servers.h"
#include "util.h"
#include "sys.h"
#include "dbg.h"

void sys_lock_init( lock_t lock )
{
#ifdef __windows__
	InitializeCriticalSection( ( LPCRITICAL_SECTION )lock );
#endif
}

void sys_lock_deinit( lock_t lock )
{
#ifdef __windows__
	DeleteCriticalSection( ( LPCRITICAL_SECTION )lock );
#endif
}

void sys_lock( lock_t lock )
{
#ifdef __windows__
	EnterCriticalSection( ( LPCRITICAL_SECTION )lock );
#else
	atomic_store( &lock, 1 );
#endif
}

void sys_unlock( lock_t lock )
{
#ifdef __windows__
	LeaveCriticalSection( ( LPCRITICAL_SECTION )lock );
#else
	atomic_store( &lock, 0 );
#endif
}

int sys_wait_unlock( lock_t lock, unsigned msec )
{
#ifdef __windows__
	return WaitForSingleObject( lock, msec ) == WAIT_OBJECT_0 ? 1 : 0;
#else // :(
	int i;

	for( i = 0; i < msec; ++i )
	{
		if( atomic_load( &lock ) )
			usleep( 1000 );
		else
			return 1;
	}

	return 0;
#endif
}

#ifdef __windows__
HWND sys_create_window( WNDPROC wnd_proc )
{
	WNDCLASS wndclass;
	HWND window;
	char* classname;

	classname = "NTL.Async";

	wndclass.style = CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc = wnd_proc;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.hInstance = NULL;
	wndclass.hIcon = LoadIcon( NULL, IDI_APPLICATION );
	wndclass.hCursor = LoadCursor( NULL, IDC_ARROW );
	wndclass.hbrBackground = ( HBRUSH )GetStockObject( WHITE_BRUSH );
	wndclass.lpszMenuName = NULL;
	wndclass.lpszClassName = classname;

	if( RegisterClass( &wndclass ) == 0 )
	{
		fprintf( stderr, "RegisterClass error %i\n", GetLastError( ) );
		return NULL;
	}

	if( ( window = CreateWindow( classname, "", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, NULL, NULL ) ) == NULL )
	{
		fprintf( stderr, "CreateWindow error %i\n", GetLastError( ) );
		return NULL;
	}

	return window;
}
#endif

thread_handle_t sys_create_thread( void* handler, void* arg )
{
	thread_handle_t handle;
#ifdef _WIN32
	handle = CreateThread( NULL, 0, ( PTHREAD_START_ROUTINE )handler, ( void * )arg, 0, NULL );
#else
	pthread_create( &handle, NULL, ( PTHREAD_START_ROUTINE )handler, ( void * )arg );
#endif
	return handle;
}

int sys_create_workthread( thread_t* thread, thread_routine_t handler )
{
#ifdef _WIN32
	thread->handle = CreateThread( NULL, 0, ( PTHREAD_START_ROUTINE )handler, ( void * )thread, 0, NULL );
	return thread->handle != NULL;
#else
	return pthread_create( &thread->handle, NULL, ( PTHREAD_START_ROUTINE )handler, ( void * )thread );
#endif
}

int sys_get_cpu_cores( )
{
#ifdef __windows__
	SYSTEM_INFO sysinfo;
	GetSystemInfo( &sysinfo );
	dbg( "cpu cores = %i\n", sysinfo.dwNumberOfProcessors );
	return sysinfo.dwNumberOfProcessors;
#else
	dbg( "cpu cores = %i\n", sysconf( _SC_NPROCESSORS_ONLN ) );
	return sysconf( _SC_NPROCESSORS_ONLN );
#endif
}

void sys_sleep( dword msec )
{
#ifdef __windows__
	Sleep( msec );
#else
	usleep( msec * 1000 );
#endif
}

#ifdef __windows__
int __stdcall pipe_reader_thread( struct server_s* server );

int sys_run_server( char* cmdline, server_t* server )
{
	SECURITY_ATTRIBUTES saAttr;
	PROCESS_INFORMATION piProcInfo;
	STARTUPINFO siStartInfo;
	BOOL bSuccess;

	// Set the bInheritHandle flag so pipe handles are inherited. 
	saAttr.nLength = sizeof( SECURITY_ATTRIBUTES );
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	// Create a pipe for the child process's STDOUT. 
	if( !CreatePipe( &server->rcon.streams.out.read, &server->rcon.streams.out.write, &saAttr, 0 ) )
	{
		fprintf( stderr, "Can't CreatePipe for stdout\n" );
		return 0;
	}

	// Ensure the read handle to the pipe for STDOUT is not inherited.
	if( !SetHandleInformation( server->rcon.streams.out.read, HANDLE_FLAG_INHERIT, 0 ) )
	{
		fprintf( stderr, "stdout pipe not inherited\n" );
		return 0;
	}

	// Create a pipe for the child process's STDIN. 
	if( !CreatePipe( &server->rcon.streams.in.read, &server->rcon.streams.in.write, &saAttr, 0 ) )
	{
		fprintf( stderr, "Can't CreatePipe for stdin\n" );
		return 0;
	}

	// Ensure the write handle to the pipe for STDIN is not inherited. 
	if( !SetHandleInformation( server->rcon.streams.in.write, HANDLE_FLAG_INHERIT, 0 ) )
	{
		fprintf( stderr, "stdin pipe not inherited\n" );
		return 0;
	}

	// Set up members of the PROCESS_INFORMATION structure. 
	ZeroMemory( &piProcInfo, sizeof( PROCESS_INFORMATION ) );

	// Set up members of the STARTUPINFO structure. 
	// This structure specifies the STDIN and STDOUT handles for redirection.
	ZeroMemory( &siStartInfo, sizeof( STARTUPINFO ) );
	siStartInfo.cb = sizeof( STARTUPINFO );
	siStartInfo.hStdError = server->rcon.streams.out.write;
	siStartInfo.hStdOutput = server->rcon.streams.out.write;
	siStartInfo.hStdInput = server->rcon.streams.in.read;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	// Create the child process. 
	bSuccess = CreateProcess( NULL,
							  cmdline,       // command line 
							  NULL,          // process security attributes 
							  NULL,          // primary thread security attributes 
							  TRUE,          // handles are inherited 
							  0,             // creation flags 
							  NULL,          // use parent's environment 
							  NULL,          // use parent's current directory 
							  &siStartInfo,  // STARTUPINFO pointer 
							  &piProcInfo );  // receives PROCESS_INFORMATION 

	// If an error occurs, exit the application. 
	if( !bSuccess )
	{
		fprintf( stderr, "can't create thread\n" );
		return 0;
	}
	else
	{
		// Close handles to the child process and its primary thread.
		// Some applications might keep these handles to monitor the status
		// of the child process, for example. 
		server->process = piProcInfo.hProcess;
		CloseHandle( piProcInfo.hThread );

		server->rcon.streams.console_reader = sys_create_thread( ( void * )pipe_reader_thread, ( void * )server );
	}

	return 1;
}

int sys_write_server_console( server_t* server, const char* command )
{
	DWORD dwWritten;
	WriteFile( server->rcon.streams.in.write, command, strlen( command ), &dwWritten, NULL );
	return dwWritten;
}

int sys_read_server_console( server_t* server, char* output, int maxlen )
{
	DWORD dwRead;
	ReadFile( server->rcon.streams.out.read, output, maxlen, &dwRead, NULL );
	return dwRead;
}
#else
int sys_run_server( char* cmdline, server_t* server )
{
	char buf[MAX_CMDLINE_LEN + 1];
	char* argv[MAX_CMDLINE_ARGS];
	int argc;
	int stdin_fd[2];
	int stdout_fd[2];
	pid_t pid;

	enum
	{
		_READ,
		_WRITE
	};

	strncpy( buf, cmdline, sizeof buf - 1 );
	argc = parse( buf, argv, MAX_CMDLINE_ARGS - 1 );
	argv[argc] = NULL;

	if( pipe( stdin_fd ) == -1 || pipe( stdout_fd ) == -1 )
	{
		perror( "pipe" );
		return -1;
	}

	pid = fork( );

	switch( pid )
	{
	case -1: // error
		perror( "fork" );
		return -1;

	case 0: // we child process
		close( stdin_fd[_WRITE] );
		dup2( stdin_fd[_READ], _READ );
		close( stdout_fd[_READ] );
		dup2( stdout_fd[_WRITE], _WRITE );

		execv( argv[0], argv + 1 ); // switch to mc server. can return only -1 if error
		perror( "exec" );
		exit( EXIT_FAILURE );

	default: // we parent
		server->process = pid;
	}

	server->rcon.streams.in = stdin_fd[_WRITE];
	server->rcon.streams.out = stdout_fd[_READ];
	return 1;
}

int sys_write_server_console( server_t* server, const char* command )
{
	return write( server->rcon.streams.in, command, strlen( command ) );
}

int sys_read_server_console( server_t* server, char* output, int maxlen )
{
	return read( server->rcon.streams.out, output, maxlen );
}
#endif