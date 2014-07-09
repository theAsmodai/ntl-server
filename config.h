#ifndef CONFIG_H
#define CONFIG_H

struct xml_s;
typedef struct xml_s* config_t;

int				xml_valid( struct xml_s* xml );
struct xml_s*	xml_get_sub( struct xml_s* xml, const char* name );
int				xml_get_sub_count( struct xml_s* xml );
struct xml_s*	xml_get_next( struct xml_s* xml );
const char*		xml_get_name( struct xml_s* xml );
int				xml_get_int( struct xml_s* xml, const char* name );
int				xml_get_bool( struct xml_s* xml, const char* name );
char*			xml_get_string( struct xml_s* xml, const char* name );

config_t		config_load( const char* path );
void			config_close( config_t cfg );

#endif // CONFIG_H