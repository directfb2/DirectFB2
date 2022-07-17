/*
   This file is part of DirectFB.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
*/

#ifndef __DIRECT__LIST_H__
#define __DIRECT__LIST_H__

#include <direct/debug.h>

/**********************************************************************************************************************/

struct __D_DirectLink {
     int         magic;

     DirectLink *next;
     DirectLink *prev; /* The prev pointer of the first element always points
                          to the last element of the list, for fast appending. */
};

/**********************************************************************************************************************/

static __inline__ void
direct_list_prepend( DirectLink **list,
                     DirectLink  *link )
{
     DirectLink *first;

     D_ASSERT( list != NULL );
     D_ASSERT( link != NULL );

     first = *list;

     link->next = first;

     if (first) {
          D_MAGIC_ASSERT( first, DirectLink );

          link->prev = first->prev;

          first->prev = link;
     }
     else
          link->prev = link;

     *list = link;

     D_MAGIC_SET( link, DirectLink );
}

static __inline__ void
direct_list_append( DirectLink **list,
                    DirectLink  *link )
{
     DirectLink *first;

     D_ASSERT( list != NULL );
     D_ASSERT( link != NULL );

     first = *list;

     link->next = NULL;

     if (first) {
          DirectLink *last = first->prev;

          D_MAGIC_ASSERT( first, DirectLink );
          D_MAGIC_ASSERT( last, DirectLink );

          link->prev = last;

          last->next = first->prev = link;
     }
     else
          *list = link->prev = link;

     D_MAGIC_SET( link, DirectLink );
}

static __inline__ void
direct_list_insert( DirectLink **list,
                    DirectLink  *link,
                    DirectLink  *before )
{
     DirectLink *first;

     D_ASSERT( list != NULL );
     D_ASSERT( link != NULL );

     first = *list;

     D_MAGIC_ASSERT_IF( first, DirectLink );
     D_MAGIC_ASSERT_IF( before, DirectLink );

     if (first == before) {
          direct_list_prepend( list, link );
     }
     else if (first == NULL || before == NULL) {
          direct_list_append( list, link );
     }
     else {
          DirectLink *prev = before->prev;

          D_MAGIC_ASSERT( prev, DirectLink );

          prev->next   = link;

          link->prev   = prev;
          link->next   = before;

          before->prev = link;

          D_MAGIC_SET( link, DirectLink );
     }
}

static __inline__ bool
direct_list_contains_element_EXPENSIVE( DirectLink *list,
                                        DirectLink *link )
{
     D_MAGIC_ASSERT_IF( list, DirectLink );

     while (list) {
          if (list == link)
               return true;

          list = list->next;
     }

     return false;
}

static __inline__ int
direct_list_count_elements_EXPENSIVE( DirectLink *list )
{
     int count = 0;

     while (list) {
          D_MAGIC_ASSERT( list, DirectLink );

          count++;

          list = list->next;
     }

     return count;
}

static __inline__ bool
direct_list_remove( DirectLink **list,
                    DirectLink  *link )
{
     DirectLink *next;
     DirectLink *prev;

     D_ASSERT( list != NULL );

     D_ASSERT( direct_list_contains_element_EXPENSIVE( *list, link ) );

     D_MAGIC_ASSERT( *list, DirectLink );
     D_MAGIC_ASSERT( link, DirectLink );

     next = link->next;
     prev = link->prev;

     if (next) {
          D_MAGIC_ASSERT( next, DirectLink );

          next->prev = prev;
     }
     else
         (*list)->prev = prev;

     if (link == *list)
          *list = next;
     else {
          D_MAGIC_ASSERT( prev, DirectLink );

          prev->next = next;
     }

     link->next = link->prev = NULL;

     D_MAGIC_CLEAR( link );

     return true;
}

static __inline__ void
direct_list_move_to_front( DirectLink **list,
                           DirectLink  *link )
{
     DirectLink *next;
     DirectLink *prev;
     DirectLink *first;

     D_ASSERT( list != NULL );

     first = *list;

     D_ASSERT( direct_list_contains_element_EXPENSIVE( first, link ) );

     D_MAGIC_ASSERT( first, DirectLink );
     D_MAGIC_ASSERT( link, DirectLink );

     if (first == link)
          return;

     next = link->next;
     prev = link->prev;

     D_MAGIC_ASSERT_IF( next, DirectLink );
     D_MAGIC_ASSERT( prev, DirectLink );

     if (next) {
          next->prev = prev;

          link->prev = first->prev;
     }
     else
          link->prev = prev;

     prev->next = next;

     link->next = first;

     first->prev = link;

     *list = link;
}

static __inline__ void *
direct_list_get_last( DirectLink *list )
{
     D_MAGIC_ASSERT_IF( list, DirectLink );

     if (list) {
          D_MAGIC_ASSERT( list->prev, DirectLink );

          return list->prev;
     }

     return NULL;
}

/**********************************************************************************************************************/

#define direct_list_check_link(link)                                                                     \
     ({                                                                                                  \
          D_MAGIC_ASSERT_IF( link, DirectLink );                                                         \
          link != NULL;                                                                                  \
     })

#define direct_list_foreach(elem,list)                                                                   \
     for (elem = (__typeof__(elem)) (list);                                                              \
          direct_list_check_link( (DirectLink*) (elem) );                                                \
          elem = (__typeof__(elem)) (((DirectLink*) (elem))->next))

#define direct_list_foreach_safe(elem,temp,list)                                                         \
     for (elem = (__typeof__(elem)) (list),                                                              \
          temp = ((__typeof__(temp)) (elem) ? (__typeof__(temp)) (((DirectLink*) (elem))->next) : NULL); \
          direct_list_check_link( (DirectLink*) (elem) );                                                \
          elem = (__typeof__(elem)) (temp),                                                              \
          temp = ((__typeof__(temp)) (elem) ? (__typeof__(temp)) (((DirectLink*) (elem))->next) : NULL))

#endif
