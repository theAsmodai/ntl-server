#include <string.h>
#include <stdio.h>
#include <time.h>

#include "const.h"
#include "util.h"
#include "dbg.h"

#include "hash/md5.h"
#include "hash/sha1.h"
#include "hash/sha256.h"

int is_valid_mail( const char* data )
{
	int c, a = 0;
	dbg( "email [%s] is ", data );

	for( c = *data; c != '\0'; c = *( ++data ) )
	{
		if( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || c == '.' )
			continue;
		else if( c == '@' )
			a = 1;
		else
		{
			a = 0;
			break;
		}
	}

	dbg( "%s, last char = %c", a ? "valid" : "invalid", c );
	return a;
}

int is_valid_hwid( const char* data )
{
	int c;
	dbg( "hwid [%s] is ", data );

	for( c = *data; c != '\0'; c = *( ++data ) )
	{
		if( ( c < 'a' || c > 'z' ) && ( c < 'A' || c > 'Z' ) )
			continue;
		dbg( "invalid. last char = %c\n", c );
		return 0;
	}

	dbg( "valid.\n" );
	return 1;
}

int parse( char* line, char** argv, int max_args )
{
	int count = 0;

	while( *line )
	{
		// null whitespaces
		while( *line == ' ' || *line == '\t' || *line == '\n' || *line == '\r' )
			*line++ = '\0';

		if( *line )
		{
			argv[count++] = line; // save arg address

			if( count == max_args )
				break;

			// skip arg
			while( *line != '\0' && *line != ' ' && *line != '\t' && *line != '\n' && *line != '\r' )
				line++;
		}
	}

	return count;
}

unsigned msg_get_uint( msg_t* msg, int error )
{
	char* data;
	unsigned value;

	if( msg->readcount + 4 > msg->maxsize )
	{
		return error;
	}

	data = msg->data + msg->readcount;
	msg->readcount += 4;
#ifdef USE_NETWORK_BYTE_ORDER
	value = data[0] + ( data[1] << 8 ) + ( data[2] << 16 ) + ( data[3] << 24 );
#else
	value = *( unsigned int *)data;
#endif

	return value;
}

unsigned msg_get_ushort( msg_t* msg, int error )
{
	char* data;
	unsigned value;
		
	if( msg->readcount + 2 > msg->maxsize )
	{
		return error;
	}

	data = msg->data + msg->readcount;
	msg->readcount += 2;
#ifdef USE_NETWORK_BYTE_ORDER
	value = data[0] + ( data[1] << 8 );
#else
	value = *( unsigned short *)data;
#endif

	return value;
}

const char* msg_get_string( msg_t* msg, int maxlen )
{
	char *string, *end;
	int maxspace;

	string		= msg->data + msg->readcount;
	maxspace	= msg->maxsize - msg->readcount;
	end			= memchr( string, '\0', maxlen > maxspace ? maxspace : maxlen );

	if( !end ) return NULL;
	msg->readcount += ( end + 1 - string );

	return string;
}

static void hash_md5( const char* string, byte* result )
{
	MD5_CTX ctx;

	MD5_Init( &ctx );
	MD5_Update( &ctx, &string, strlen( string ) );
	MD5_Final( result, &ctx );
}

static void hash_sha1( const char* string, byte* result )
{
	SHA1Context ctx;

	SHA1Reset( &ctx );
	SHA1Input( &ctx, ( const uint8_t *)string, strlen( string ) );
	SHA1Result( &ctx, result );
}

static void hash_sha256( const char* string, byte* result )
{
	sha256_context ctx;

	sha256_starts( &ctx );
	sha256_update( &ctx, ( byte *)string, strlen( string ) );
	sha256_finish( &ctx, result );
}

hash_func_t get_hash_func( const char* name )
{
	if( !strcmp( name, "md5" ) )
		return hash_md5;
	else if( !strcmp( name, "sha1" ) )
		return hash_sha1;
	else if( !strcmp( name, "sha256" ) )
		return hash_sha256;

	fprintf( stderr, "unknown hash function: %s\n", name );
	return NULL;
}

int php_get_serialized( const char* blob, const char* var, char* value, int maxlen )
{
	const char *pos, *end;
	int len;

	if( ( pos = strstr( blob, var ) ) == NULL )
		return 0;

	pos = strstr( pos, ":\"" ) + 1;
	end = strchr( pos, '\"' );
	len = end - pos;

	if( len > maxlen )
		return 0;

	memcpy( value, pos, len );
	value[len] = '\0';

	return 1;
}