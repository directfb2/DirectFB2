#!/bin/sh
#
#  This file is part of DirectFB.
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public
#  License as published by the Free Software Foundation; either
#  version 2.1 of the License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with this library; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA

mknames() {
ENUM=$1
PREFIX=$2
NULL=$3
NAME=$4
VALUE=$5
HEADER=$6

cat << EOF

struct DFB${NAME}Name {
     ${ENUM} ${VALUE};
     const char *name;
};

#define DirectFB${NAME}Names(Identifier) struct DFB${NAME}Name Identifier[] = { \\
EOF

egrep "^ +${PREFIX}_[0-9A-Za-z_]+[ ,]" $HEADER | grep -v ${PREFIX}_${NULL} | perl -p -e "s/^\\s*(${PREFIX}_)([\\w_]+)[ ,].*/     \\{ \\1\\2, \\\"\\2\\\" \\}, \\\\/"

cat << EOF
     { ($ENUM) ${PREFIX}_${NULL}, "${NULL}" } \\
};
EOF
}

echo \#ifndef __DIRECTFB_KEYNAMES_H__
echo \#define __DIRECTFB_KEYNAMES_H__
echo
echo \#include \<directfb_keyboard.h\>
mknames DFBInputDeviceKeySymbol     DIKS NULL    KeySymbol     symbol     $1 | grep -v DIKS_ENTER
mknames DFBInputDeviceKeyIdentifier DIKI UNKNOWN KeyIdentifier identifier $1 | grep -v DIKI_NUMBER_OF_KEYS | grep -v DIKI_KEYDEF_END
echo
echo \#endif
