#ifndef DBG_H
#define DBG_H

#ifdef NDEBUG
#define dbg( str, ... )		( ( void )0 )
#else
int __cdecl printf( const char* fmt, ... );
#define dbg( str, ... )		printf( "[dbg] " str, __VA_ARGS__ )
#endif

#endif // DBG_H