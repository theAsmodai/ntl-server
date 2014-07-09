#ifndef UTIL_H
#define UTIL_H

typedef struct msg_s
{
	char*	data;
	int		readcount;
	int		maxsize;
} msg_t;

int is_valid_mail( const char* data );
int is_valid_hwid( const char* data );
int parse( char* line, char** argv, int max_args ); // parses line; argv - array of args, return count of args

unsigned msg_get_uint( struct msg_s* msg, int error );
unsigned msg_get_ushort( struct msg_s* msg, int error );
const char* msg_get_string( struct msg_s* msg, int maxlen );

typedef void( *hash_func_t )( const char *, byte *);
hash_func_t get_hash_func( const char* name );

int php_get_serialized( const char* blob, const char* var, char* value, int maxlen );

#endif // UTIL_H