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

#ifndef __DIRECT__SERIAL_H__
#define __DIRECT__SERIAL_H__

#include <direct/debug.h>

D_DEBUG_DOMAIN( Direct_Serial, "Direct/Serial", "Direct Serial" );

/**********************************************************************************************************************/

typedef struct {
     int           magic;

     u32           overflow;

     unsigned long value;

     int           waiting;
     int           wakeup;
} DirectSerial;

/**********************************************************************************************************************/

static __inline__ void
direct_serial_init( DirectSerial *serial )
{
     D_DEBUG_AT( Direct_Serial, "%s( %p )\n", __FUNCTION__, serial );

     D_ASSERT( serial != NULL );

     serial->value    = 0;
     serial->overflow = 0;
     serial->waiting  = 0;

     D_MAGIC_SET( serial, DirectSerial );
}

static __inline__ void
direct_serial_deinit( DirectSerial *serial )
{
     D_MAGIC_ASSERT( serial, DirectSerial );

     D_DEBUG_AT( Direct_Serial, "%s( %p ) <- %lu\n", __FUNCTION__, serial, serial->value );

     D_ASSUME( serial->waiting == 0 );

     D_MAGIC_CLEAR( serial );
}

static __inline__ void
direct_serial_increase( DirectSerial *serial )
{
     D_MAGIC_ASSERT( serial, DirectSerial );

     D_DEBUG_AT( Direct_Serial, "%s( %p ) <- %lu ++\n", __FUNCTION__, serial, serial->value );

     if (!++serial->value)
          serial->overflow++;

     D_DEBUG_AT( Direct_Serial, "  -> %lu\n", serial->value );
}

static __inline__ void
direct_serial_copy( DirectSerial       *serial,
                    const DirectSerial *source )
{
     D_MAGIC_ASSERT( serial, DirectSerial );
     D_MAGIC_ASSERT( source, DirectSerial );

     D_DEBUG_AT( Direct_Serial, "%s( %p, %p ) <- %lu = %lu\n", __FUNCTION__,
                 serial, source, source->value, serial->value );

     serial->value    = source->value;
     serial->overflow = source->overflow;
}

static __inline__ bool
direct_serial_check( const DirectSerial *serial,
                     const DirectSerial *source )
{
     D_MAGIC_ASSERT( serial, DirectSerial );
     D_MAGIC_ASSERT( source, DirectSerial );

     D_DEBUG_AT( Direct_Serial, "%s( %p, %p ) <- %lu == %lu\n", __FUNCTION__,
                 serial, source, serial->value, source->value );

     if (serial->overflow < source->overflow)
          return false;
     else if (serial->overflow == source->overflow && serial->value < source->value)
          return false;

     return true;
}

static __inline__ bool
direct_serial_update( DirectSerial       *serial,
                      const DirectSerial *source )
{
     D_MAGIC_ASSERT( serial, DirectSerial );
     D_MAGIC_ASSERT( source, DirectSerial );

     D_DEBUG_AT( Direct_Serial, "%s( %p, %p ) <- %lu <-= %lu\n", __FUNCTION__,
                 serial, source, serial->value, source->value );

     if (serial->overflow < source->overflow) {
          serial->overflow = source->overflow;
          serial->value    = source->value;

          return true;
     }
     else if (serial->overflow == source->overflow && serial->value < source->value) {
          serial->value = source->value;

          return true;
     }

     return false;
}

#endif
