#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "const.h"
#include "dbg.h"

typedef enum xml_data_type_e
{
	dt_invalid,
	dt_string,
	dt_xml,
	dt_config
} xml_data_type_t;

typedef union xml_data_u
{
	struct xml_s*		xml;
	char*				string;
	void*				ptr;
} xml_data_t;

typedef struct xml_s
{
	const char*			name;
	xml_data_t			data;
	xml_data_type_t		type;
	struct xml_s*		next;
} xml_t;

typedef struct xml_pool_s
{
	xml_t*	xml;
	xml_t*	end;
} xml_pool_t;

int xml_valid( xml_t* xml )
{
	return xml && xml->type != dt_invalid;
}

static int xml_valid_type( xml_t* xml, xml_data_type_t type )
{
	return xml && xml->type == type;
}

static const char* xml_get_typename( xml_t* xml )
{
	if( xml )
	{
		switch( xml->type )
		{
		case dt_invalid:
			return "invalid";
		case dt_string:
			return "string";
		case dt_xml:
			return "xml";
		case dt_config:
			return "config";
		}
	}
	return "unknown";
}

xml_t* xml_get_sub( xml_t* from, const char* name )
{
	xml_t* xml;

	if( from && ( from->type == dt_xml || from->type == dt_config ) )
	{
		xml = from->data.xml;

		if( name )
		{
			while( xml )
			{
				if( !strcmp( xml->name, name ) )
					break;
				xml = xml->next;
			}
		}
	}
	else
	{
		if( name )
			fprintf( stderr, "xml: can't get sub section %s:%s\n", from ? from->name : "null", name );
		xml = NULL;
	}

	return xml;
}

int xml_get_sub_count( xml_t* from )
{
	xml_t* xml;
	int count;

	count = 0;

	for( xml = xml_get_sub( from, NULL ); xml; xml = xml->next )
		++count;

	return count;
}

xml_t* xml_get_next( xml_t* xml )
{
	return xml->next;
}

const char* xml_get_name( xml_t* xml )
{
	return xml->name;
}

int xml_get_int( xml_t* from, const char* name )
{
	xml_t* xml;
	xml = xml_get_sub( from, name );
	return xml_valid( xml ) ? atoi( xml->data.string ) : XML_INVALID_INT;
}

int xml_get_bool( xml_t* from, const char* name )
{
	xml_t* xml;
	xml = xml_get_sub( from, name );
	return xml_valid( xml ) ? ( !strcmp( xml->data.string, "true" ) || xml->data.string[0] == '1' ) ? 1 : 0 : XML_INVALID_BOOL;
}

char* xml_get_string( xml_t* from, const char* name )
{
	xml_t* xml;
	xml = xml_get_sub( from, name );
	return xml_valid( xml ) ? xml->data.string : XML_INVALID_STRING;
}

static xml_t* new_xml( xml_pool_t* pool )
{
	xml_t* xml = pool->xml++;

	if( xml < pool->end )
		return xml;

	fprintf( stderr, "[config error] no space in pool available\n" );
	return NULL;
}

static void xml_print_spaces( int level )
{
	int i;

	for( i = 0; i < level; ++i )
		printf( "   " );
}

#ifdef NDEBUG
#define xml_print_spaces( x )	( ( void )0 )
#endif

static xml_t* xml_parse( char* data, xml_t* parent, xml_pool_t* pool, int level )
{
	xml_t *xml, *ret, **ptr;
	char *c, *in, buf[XML_MAXLEN + 1];
	int len;

	parent->type = level ? dt_string : dt_invalid;
	ret = ( xml_t *)data;
	ptr = &ret;
	
	xml_print_spaces( level );
	dbg( "xml_parse::%s\n", parent->name );

	for(;;)
	{
		for(; *data != '<'; ++data )
		{
			if( *data == '\0' )
				return ret;
		}

		c = strchr( ++data, '>' );

		if( !c )
		{
			parent->type = dt_invalid;
			fprintf( stderr, "[config error] invalid data\n" );
			break;
		}

		*c = '\0';
		//xml_print_spaces( level + 1 );
		//dbg( "xml_parse::subkey %s\n", data );
		in = ++c;

		len = snprintf( buf, sizeof buf - 1, "</%s>", data );
		c = strstr( c, buf );

		if( !c )
		{
			parent->type = dt_invalid;
			fprintf( stderr, "[config error] unclosed tag %s\n", data );
			break;
		}

		*c = '\0';
		c += len;
		parent->type = level ? dt_xml : dt_config;

		if( ( xml = new_xml( pool ) ) == NULL )
		{
			parent->type = dt_invalid;
			break;
		}

		xml->name		= data;
		xml->data.xml	= xml_parse( in, xml, pool, level + 1 );
		xml->next		= NULL;

		if( xml->type == dt_string )
		{
			xml_print_spaces( level + 2 );
			dbg( "xml_parse:: '%s'\n", *xml->data.string == '\n' ? "" : xml->data.string );
		}
		xml_print_spaces( level + 1 );
		dbg( "xml_parse::%s done (type: %s, sub: %i)\n", data, xml_get_typename( xml ), xml_get_sub_count( xml ) );

		*ptr			= xml;
		ptr				= &xml->next;
		data			= c;
	}

	return NULL;
}

config_t config_load( const char* path )
{
	FILE*		fp;
	int			file_size;
	char*		buf;
	xml_t*		cfg;
	xml_pool_t	xml_pool;

	fp = fopen( path, "rt" );

	if( fp == NULL )
	{
		perror( path ); //:errno
		return NULL;
	}

	fseek( fp, 0, SEEK_END );
	file_size = ftell( fp );
	fseek( fp, 0, SEEK_SET );
	dbg( "%s file size = %0.2f kb\n", path, ( float )file_size / 1024.0 );

	cfg = ( xml_t *)malloc( sizeof( xml_t ) + file_size + XML_MAXCOUNT * sizeof( xml_t ) );
	buf = ( char *)( cfg + 1 );
	xml_pool.xml = ( xml_t *)( buf + file_size );
	xml_pool.end = xml_pool.xml + XML_MAXCOUNT;

	fread( buf, 1, file_size, fp );
	fclose( fp );

	cfg->name		= path;
	cfg->data.xml	= xml_parse( buf, cfg, &xml_pool, 0 );
	cfg->next		= NULL;

	dbg( "xml_parse::%s done (type: %s, sub: %i)\n", path, xml_get_typename( cfg ), xml_get_sub_count( cfg ) );

	if( !xml_valid( cfg ) )
	{
		fprintf( stderr, "[config error] invalid data\n" );
		config_close( cfg );
		cfg = NULL;
	}
	
	printf( "Config loaded. Readed %i xml keys\n", xml_pool.xml + XML_MAXCOUNT - xml_pool.end );
	return cfg;
}

void config_close( config_t cfg )
{
	if( cfg && cfg->type == dt_config )
	{
		dbg( "closing config\n" );
		free( ( void *)cfg );
	}
}