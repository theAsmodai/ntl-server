#include <stdlib.h>
#include "client.h"
#include "mem.h"

// I will write it later

client_t* mem_alloc_client()
{
	return ( struct client_s *)malloc( sizeof( client_t ) );
}

void mem_free_client( client_t* client )
{
	free( ( void *)&client );
}