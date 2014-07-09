#ifndef CLIENT_LIST_H
#define CLIENT_LIST_H

typedef struct client_s al_obj_t;

typedef struct atomic_list_s
{
	atomic_int		head;
	atomic_bool		head_valid;
} atomic_list_t;

#ifdef CLIENT_H
#define HAVE_AOBJ_AND_ALIST
#endif

#endif // CLIENT_LIST_H