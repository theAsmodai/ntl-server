#ifndef CONST_H
#define CONST_H

#define CONFIG_FILE				"config.xml"
#define LOG_FILE				"ntl.log"
#define CONNECT_TIMEOUT			15

#define NO_ANSWER				0

#define CPU_CACHE_LINE			64

#define MAX_SALT_LEN			63
#define MAX_HWID_LEN			31
#define MAX_EMAIL_LEN			31
#define MAX_PASS_LEN			31
#define MAX_DIGEST_LEN			31
#define MAX_PLAYER_NAME			20
#define	MAX_VERSION_LEN			7
#define MAX_SERVER_NAME			127
#define MAX_SERVER_ID			15
#define MAX_SERVER_PASSWORD		31
#define MAX_SRVCMD_LEN			512

#define MAX_CMDLINE_LEN			511
#define MAX_CMDLINE_ARGS		32

#define MAX_INPUT_LEN			256
#define MAX_INPUT_CMD_LEN		32

#define CONSOLE_BUFSIZE			8096

#define STRING( x )				#x

#define SQL_QUERY_MAXLEN		512

#define NTL_BANS_TABLE			"ntl_bans"
#define NTL_USERS_TABLE			"ntl_users"
#define XF_USERS_TABLE			"xf_user"
#define XF_PASSWORDS_TABLE		"xf_user_authenticate"

#define XML_MAXCOUNT			256
#define XML_MAXLEN				63
#define XML_INVALID_INT			0
#define XML_INVALID_BOOL		-1
#define XML_INVALID_STRING		NULL

#define SRV_CONN_TIMEOUT		1000

#define MAX_EVENTS				16

#define THREAD_TIMEOUT			400
#define THREAD_CHECK_FREE		10
#define THREAD_NO_CLIENT		0

//#define USE_NETWORK_BYTE_ORDER
#define INVALID_IP				0

typedef unsigned char		byte;
typedef unsigned short		word;
typedef unsigned long		dword;

#ifdef _WIN32
	typedef _W64 unsigned int	UINT_PTR, *PUINT_PTR;
	typedef UINT_PTR			socket_t;
	typedef void*				lock_t;
	typedef void*				thread_handle_t;
	typedef void*				pipe_handle_t;
	typedef void*				process_t;
	#define WM_SOCKET_MSG		( WM_USER + 1 )
	#define snprintf			_snprintf
	#define vsnprintf			_vsnprintf
	#define CALLBACK			__stdcall
#elif __linux__
	typedef _Atomic _Bool		atomic_bool;
	typedef int socket_t;
	typedef _Atomic _Bool		lock_t;
	typedef long				thread_handle_t;
	typedef int					pipe_handle_t;
	typedef long				process_t;
	#define CALLBACK
#endif

struct thread_s;
typedef unsigned int		ip_t;
typedef int ( CALLBACK *thread_routine_t )( struct thread_s * );

#if MAX_PASS_LEN < 31
	#error "MAX_PASS_LEN can't be shorter 31"
#endif

#endif