#ifndef MEM_H
#define MEM_H

typedef struct mem_s
{
	void*	blocks[256];
	int		blocks_count;
} mem_t;

struct client_s* mem_alloc_client();
void mem_free_client( struct client_s* client );
void mem_defrag( mem_t* mem, struct net_clients_s* clients );

#endif // MEM_H