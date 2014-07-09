#ifndef ATOMIC_LIST_H
#define ATOMIC_LIST_H

#ifndef HAVE_AOBJ_AND_ALIST
#error "atomic list types not defined"
#endif

#ifndef _STRONG_INLINE
#define _STRONG_INLINE __attribute__((always_inline))
#endif

#include <stdatomic.h>

_STRONG_INLINE al_obj_t* al_head( const atomic_list_t* list )
{
	return ( al_obj_t *)atomic_load( &list->head );
}

_STRONG_INLINE void al_set_head( atomic_list_t* list, al_obj_t* obj )
{
	atomic_store( &list->head, ( int )obj );
}

_STRONG_INLINE int al_head_valid( const atomic_list_t* list )
{
	return atomic_load( &list->head_valid );
}

_STRONG_INLINE void al_init( atomic_list_t* list )
{
	al_set_head( list, NULL );
	atomic_store( &list->head_valid, false );
}

_STRONG_INLINE al_obj_t* al_next( const al_obj_t* obj )
{
	return ( al_obj_t *)atomic_load( &obj->next );
}

_STRONG_INLINE void al_set_next( al_obj_t* obj, al_obj_t* next )
{
	atomic_store( &obj->next, ( int )next );
}

_STRONG_INLINE void al_push( atomic_list_t* list, al_obj_t* obj )
{
	al_set_next( obj, al_head( list ) );
	al_set_head( list, obj );
}

_STRONG_INLINE void al_remove( al_obj_t* prev, al_obj_t* obj )
{
	al_set_next( prev, al_next( obj ) );
}

#endif // ATOMIC_LIST_H