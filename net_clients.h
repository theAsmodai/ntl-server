#ifndef NET_CLIENTS
#define NET_CLIENTS

typedef struct net_clients_s
{
	struct atomic_list_s*			lists;
	const struct atomic_list_s*		end;
} net_clients_t;

#endif // NET_CLIENTS