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

static const unsigned int cursor_data[] = {
     0xe8fdfdfd, 0x83e8e8e8, 0x40d3d3d3, 0x1ed0d0d0,
     0x15dddddd, 0x10ebebeb, 0x0af3f3f3, 0x05f8f8f8,
     0x02fbfbfb, 0x01fdfdfd, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0xd9fcfcfc, 0xefffffff, 0xd0f9f9f9, 0x96e2e2e2,
     0x51cccccc, 0x23cbcbcb, 0x15dadada, 0x0fe9e9e9,
     0x0af2f2f2, 0x05f8f8f8, 0x02fbfbfb, 0x02fdfdfd,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0xa1e0e0e0, 0xdaffffff, 0xedffffff, 0xd6ffffff,
     0xd0fafafa, 0x9be3e3e3, 0x56cccccc, 0x25c9c9c9,
     0x15d8d8d8, 0x10e7e7e7, 0x0af1f1f1, 0x06f7f7f7,
     0x03fbfbfb, 0x01fdfdfd, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x67b7b7b7, 0xd5fbfbfb, 0xd6ffffff, 0xebffffff,
     0xd2ffffff, 0xcfffffff, 0xccfdfdfd, 0x9fe7e7e7,
     0x5bcfcfcf, 0x28c9c9c9, 0x15d6d6d6, 0x10e7e7e7,
     0x0af1f1f1, 0x06f7f7f7, 0x03fbfbfb, 0x01fdfdfd,
     0x00fefefe, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x44a5a5a5, 0xb1d9d9d9, 0xd4ffffff, 0xd2ffffff,
     0xe9ffffff, 0xceffffff, 0xccffffff, 0xc9ffffff,
     0xc7fefefe, 0xa1ebebeb, 0x62d3d3d3, 0x2dcacaca,
     0x15d4d4d4, 0x0fe5e5e5, 0x0af1f1f1, 0x06f7f7f7,
     0x03fafafa, 0x01fdfdfd, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x36b3b3b3, 0x78ababab, 0xd1fdfdfd, 0xcfffffff,
     0xceffffff, 0xccffffff, 0xcaffffff, 0xc7ffffff,
     0xc5ffffff, 0xc2ffffff, 0xc0ffffff, 0xa7f0f0f0,
     0x69d6d6d6, 0x30c9c9c9, 0x15d3d3d3, 0x0fe4e4e4,
     0x0af0f0f0, 0x05f6f6f6, 0x03fbfbfb, 0x01fdfdfd,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x2ac9c9c9, 0x4e999999, 0xb4dcdcdc, 0xcdffffff,
     0xccffffff, 0xcaffffff, 0xc7ffffff, 0xc6ffffff,
     0xc3ffffff, 0xc1ffffff, 0xbfffffff, 0xbcffffff,
     0xb9ffffff, 0xa4f2f2f2, 0x68d8d8d8, 0x2fcacaca,
     0x14d3d3d3, 0x0ee5e5e5, 0x09f1f1f1, 0x05f7f7f7,
     0x03fbfbfb, 0x01fdfdfd, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x1ed9d9d9, 0x39aaaaaa, 0x80acacac, 0xcafefefe,
     0xc9ffffff, 0xc7ffffff, 0xc6ffffff, 0xc4ffffff,
     0xc1ffffff, 0xbfffffff, 0xbdffffff, 0xbaffffff,
     0xb8ffffff, 0xb6ffffff, 0xb3ffffff, 0x9ff3f3f3,
     0x67d9d9d9, 0x2ccbcbcb, 0x13d4d4d4, 0x0ee6e6e6,
     0x09f1f1f1, 0x05f7f7f7, 0x02fbfbfb, 0x01fefefe,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x14e5e5e5, 0x2dc3c3c3, 0x53959595, 0xb4e2e2e2,
     0xc7ffffff, 0xc5ffffff, 0xc3ffffff, 0xc1ffffff,
     0xbfffffff, 0xbeffffff, 0xbbffffff, 0xb9ffffff,
     0xb7ffffff, 0xb4ffffff, 0xb1ffffff, 0xafffffff,
     0xacffffff, 0x98f2f2f2, 0x5fd9d9d9, 0x29cbcbcb,
     0x10d6d6d6, 0x0ce7e7e7, 0x08f2f2f2, 0x04f8f8f8,
     0x02fcfcfc, 0x01fefefe, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x0deeeeee, 0x21d5d5d5, 0x3ca3a3a3, 0x85afafaf,
     0xc4ffffff, 0xc2ffffff, 0xc1ffffff, 0xbfffffff,
     0xbeffffff, 0xbbffffff, 0xb9ffffff, 0xb7ffffff,
     0xb5ffffff, 0xb2ffffff, 0xb0ffffff, 0xaeffffff,
     0xabffffff, 0xa9ffffff, 0xa6ffffff, 0x90f2f2f2,
     0x58d8d8d8, 0x23cccccc, 0x0fd8d8d8, 0x0be9e9e9,
     0x07f3f3f3, 0x03f9f9f9, 0x02fcfcfc, 0x01fefefe,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x08f5f5f5, 0x16e2e2e2, 0x2fbebebe, 0x57939393,
     0xb3e7e7e7, 0xc0ffffff, 0xbfffffff, 0xbdffffff,
     0xbbffffff, 0xb9ffffff, 0xb8ffffff, 0xb5ffffff,
     0xb3ffffff, 0xb0ffffff, 0xafffffff, 0xacffffff,
     0xa9ffffff, 0xa7ffffff, 0xa4ffffff, 0xa1ffffff,
     0xa0ffffff, 0x87f0f0f0, 0x4ed6d6d6, 0x1dcdcdcd,
     0x0ddcdcdc, 0x0aececec, 0x06f5f5f5, 0x03fafafa,
     0x01fdfdfd, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x05f9f9f9, 0x0eececec, 0x22d1d1d1, 0x3d9e9e9e,
     0x88b4b4b4, 0xbeffffff, 0xbcffffff, 0xbaffffff,
     0xb9ffffff, 0xb7ffffff, 0xb5ffffff, 0xb3ffffff,
     0xb1ffffff, 0xafffffff, 0xadffffff, 0xaaffffff,
     0xa8ffffff, 0xa6ffffff, 0xa3ffffff, 0xa1ffffff,
     0x9effffff, 0x9bffffff, 0x99ffffff, 0x7bececec,
     0x3fd3d3d3, 0x16d0d0d0, 0x0be1e1e1, 0x08f0f0f0,
     0x04f8f8f8, 0x02fcfcfc, 0x00fefefe, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x02fcfcfc, 0x08f4f4f4, 0x18dfdfdf, 0x30b9b9b9,
     0x5c949494, 0xb3efefef, 0xb9ffffff, 0xb8ffffff,
     0xb7ffffff, 0xb5ffffff, 0xb3ffffff, 0xb1ffffff,
     0xafffffff, 0xadffffff, 0xaaffffff, 0xa9ffffff,
     0xa6ffffff, 0xa4ffffff, 0xa1ffffff, 0x9fffffff,
     0x9cffffff, 0x9affffff, 0x98ffffff, 0x95ffffff,
     0x92fdfdfd, 0x6ae6e6e6, 0x2ed0d0d0, 0x0fd6d6d6,
     0x0ae9e9e9, 0x06f4f4f4, 0x02fafafa, 0x01fefefe,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x01fefefe, 0x04f8f8f8, 0x0feaeaea, 0x24cdcdcd,
     0x41989898, 0x8dbcbcbc, 0xb7ffffff, 0xb6ffffff,
     0xb4ffffff, 0xb2ffffff, 0xb0ffffff, 0xafffffff,
     0xadffffff, 0xabffffff, 0xa9ffffff, 0xa7ffffff,
     0xa4ffffff, 0xa2ffffff, 0xa0ffffff, 0x9dffffff,
     0x9bffffff, 0x99ffffff, 0x96ffffff, 0x93ffffff,
     0x91ffffff, 0x8effffff, 0x84f9f9f9, 0x51dedede,
     0x1ad0d0d0, 0x0ae1e1e1, 0x07f2f2f2, 0x02f9f9f9,
     0x00fdfdfd, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x02fcfcfc, 0x09f2f2f2, 0x18dddddd,
     0x31b3b3b3, 0x5f929292, 0xaff1f1f1, 0xb3ffffff,
     0xb1ffffff, 0xb0ffffff, 0xafffffff, 0xadffffff,
     0xaaffffff, 0xa9ffffff, 0xa7ffffff, 0xa5ffffff,
     0xa2ffffff, 0xa1ffffff, 0x9effffff, 0x9bffffff,
     0x9affffff, 0x97ffffff, 0x94ffffff, 0x92ffffff,
     0x90ffffff, 0x8dffffff, 0x8bffffff, 0x88ffffff,
     0x6ceeeeee, 0x2cd3d3d3, 0x0bdcdcdc, 0x07f0f0f0,
     0x02f9f9f9, 0x00fefefe, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x01fdfdfd, 0x05f7f7f7, 0x10e8e8e8,
     0x25c9c9c9, 0x42929292, 0x8bbebebe, 0xb0ffffff,
     0xafffffff, 0xaeffffff, 0xacffffff, 0xaaffffff,
     0xa9ffffff, 0xa7ffffff, 0xa5ffffff, 0xa2ffffff,
     0xa1ffffff, 0x9effffff, 0x9cffffff, 0x9affffff,
     0x98ffffff, 0x95ffffff, 0x92ffffff, 0x91ffffff,
     0x8effffff, 0x8bffffff, 0x89ffffff, 0x86ffffff,
     0x83ffffff, 0x78f8f8f8, 0x30d6d6d6, 0x0adddddd,
     0x07f2f2f2, 0x02fafafa, 0x00fefefe, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x02fbfbfb, 0x0af1f1f1,
     0x1adadada, 0x33adadad, 0x60909090, 0xaaf2f2f2,
     0xacffffff, 0xabffffff, 0xa9ffffff, 0xa8ffffff,
     0xa6ffffff, 0xa4ffffff, 0xa2ffffff, 0xa1ffffff,
     0x9fffffff, 0x9cffffff, 0x9affffff, 0x98ffffff,
     0x96ffffff, 0x93ffffff, 0x92ffffff, 0x8fffffff,
     0x8cffffff, 0x8affffff, 0x88ffffff, 0x85ffffff,
     0x83ffffff, 0x80ffffff, 0x72f6f6f6, 0x24d1d1d1,
     0x09e2e2e2, 0x05f5f5f5, 0x01fcfcfc, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x01fdfdfd, 0x05f7f7f7,
     0x11e6e6e6, 0x26c6c6c6, 0x428f8f8f, 0x87bcbcbc,
     0xa9ffffff, 0xa9ffffff, 0xa7ffffff, 0xa6ffffff,
     0xa4ffffff, 0xa2ffffff, 0xa1ffffff, 0x9effffff,
     0x9cffffff, 0x9affffff, 0x99ffffff, 0x96ffffff,
     0x94ffffff, 0x92ffffff, 0x90ffffff, 0x8dffffff,
     0x8bffffff, 0x88ffffff, 0x86ffffff, 0x83ffffff,
     0x81ffffff, 0x7effffff, 0x7cffffff, 0x60ebebeb,
     0x14d0d0d0, 0x08ebebeb, 0x03f9f9f9, 0x00fefefe,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x03fbfbfb,
     0x0af0f0f0, 0x1bd7d7d7, 0x33ababab, 0x5d8d8d8d,
     0xa2f1f1f1, 0xa6ffffff, 0xa4ffffff, 0xa3ffffff,
     0xa1ffffff, 0xa0ffffff, 0x9effffff, 0x9cffffff,
     0x9affffff, 0x99ffffff, 0x96ffffff, 0x94ffffff,
     0x92ffffff, 0x90ffffff, 0x8dffffff, 0x8bffffff,
     0x89ffffff, 0x87ffffff, 0x84ffffff, 0x82ffffff,
     0x7fffffff, 0x7dffffff, 0x7bffffff, 0x78fefefe,
     0x36d6d6d6, 0x0adcdcdc, 0x05f4f4f4, 0x01fcfcfc,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x01fdfdfd,
     0x06f6f6f6, 0x12e3e3e3, 0x27c3c3c3, 0x428c8c8c,
     0x82bababa, 0xa3ffffff, 0xa1ffffff, 0xa1ffffff,
     0x9fffffff, 0x9dffffff, 0x9bffffff, 0x9affffff,
     0x98ffffff, 0x96ffffff, 0x94ffffff, 0x92ffffff,
     0x90ffffff, 0x8effffff, 0x8bffffff, 0x8affffff,
     0x87ffffff, 0x85ffffff, 0x83ffffff, 0x80ffffff,
     0x7effffff, 0x7cffffff, 0x79ffffff, 0x76ffffff,
     0x59e8e8e8, 0x12d0d0d0, 0x07ededed, 0x02fafafa,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x03fafafa, 0x0bededed, 0x1bd5d5d5, 0x33a8a8a8,
     0x5a898989, 0x9df0f0f0, 0xa0ffffff, 0x9effffff,
     0x9cffffff, 0x9bffffff, 0x9affffff, 0x98ffffff,
     0x96ffffff, 0x94ffffff, 0x92ffffff, 0x90ffffff,
     0x8effffff, 0x8cffffff, 0x8affffff, 0x88ffffff,
     0x85ffffff, 0x83ffffff, 0x81ffffff, 0x7effffff,
     0x7cffffff, 0x7affffff, 0x77ffffff, 0x74ffffff,
     0x6df7f7f7, 0x1fcacaca, 0x09e5e5e5, 0x03f8f8f8,
     0x01fdfdfd, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x01fdfdfd, 0x06f5f5f5, 0x12e2e2e2, 0x26c2c2c2,
     0x40898989, 0x7db5b5b5, 0x9dffffff, 0x9bffffff,
     0x9affffff, 0x99ffffff, 0x97ffffff, 0x95ffffff,
     0x93ffffff, 0x92ffffff, 0x90ffffff, 0x8effffff,
     0x8cffffff, 0x8affffff, 0x88ffffff, 0x85ffffff,
     0x83ffffff, 0x81ffffff, 0x7fffffff, 0x7cffffff,
     0x7bffffff, 0x78ffffff, 0x75ffffff, 0x74ffffff,
     0x71fdfdfd, 0x2dcdcdcd, 0x0adedede, 0x04f5f5f5,
     0x01fdfdfd, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00fefefe, 0x03fafafa, 0x0bededed, 0x1bd3d3d3,
     0x32a7a7a7, 0x56848484, 0x95ededed, 0x99ffffff,
     0x98ffffff, 0x96ffffff, 0x94ffffff, 0x92ffffff,
     0x92ffffff, 0x90ffffff, 0x8dffffff, 0x8bffffff,
     0x8affffff, 0x88ffffff, 0x86ffffff, 0x83ffffff,
     0x82ffffff, 0x7fffffff, 0x7dffffff, 0x7bffffff,
     0x79ffffff, 0x76ffffff, 0x74ffffff, 0x72ffffff,
     0x6ffefefe, 0x35cfcfcf, 0x0bd8d8d8, 0x05f3f3f3,
     0x01fcfcfc, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x01fcfcfc, 0x06f5f5f5, 0x12e2e2e2,
     0x25c1c1c1, 0x3e888888, 0x75acacac, 0x96ffffff,
     0x95ffffff, 0x93ffffff, 0x92ffffff, 0x91ffffff,
     0x8fffffff, 0x8dffffff, 0x8bffffff, 0x8affffff,
     0x88ffffff, 0x85ffffff, 0x83ffffff, 0x82ffffff,
     0x80ffffff, 0x7dffffff, 0x7cffffff, 0x79ffffff,
     0x77ffffff, 0x74ffffff, 0x72ffffff, 0x70ffffff,
     0x6dfefefe, 0x35cccccc, 0x0cd6d6d6, 0x05f2f2f2,
     0x01fcfcfc, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x01fefefe, 0x04f9f9f9, 0x0bececec,
     0x1ad3d3d3, 0x30a8a8a8, 0x517e7e7e, 0x8ce5e5e5,
     0x92ffffff, 0x91ffffff, 0x90ffffff, 0x8effffff,
     0x8cffffff, 0x8bffffff, 0x89ffffff, 0x87ffffff,
     0x85ffffff, 0x83ffffff, 0x82ffffff, 0x80ffffff,
     0x7dffffff, 0x7cffffff, 0x79ffffff, 0x77ffffff,
     0x74ffffff, 0x73ffffff, 0x70ffffff, 0x6effffff,
     0x6cfdfdfd, 0x2fc4c4c4, 0x0dd7d7d7, 0x05f2f2f2,
     0x01fcfcfc, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x01fcfcfc, 0x06f5f5f5,
     0x11e2e2e2, 0x24c1c1c1, 0x3b898989, 0x6b9f9f9f,
     0x90ffffff, 0x8effffff, 0x8dffffff, 0x8bffffff,
     0x8affffff, 0x88ffffff, 0x87ffffff, 0x85ffffff,
     0x83ffffff, 0x81ffffff, 0x7fffffff, 0x7dffffff,
     0x7cffffff, 0x7affffff, 0x77ffffff, 0x75ffffff,
     0x73ffffff, 0x71ffffff, 0x6effffff, 0x6dffffff,
     0x65f5f5f5, 0x23bbbbbb, 0x0cdbdbdb, 0x05f4f4f4,
     0x01fcfcfc, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x01fefefe, 0x03f9f9f9,
     0x0aededed, 0x18d4d4d4, 0x2eaaaaaa, 0x4a797979,
     0x81d6d6d6, 0x8bffffff, 0x8bffffff, 0x89ffffff,
     0x88ffffff, 0x86ffffff, 0x84ffffff, 0x83ffffff,
     0x81ffffff, 0x7fffffff, 0x7dffffff, 0x7cffffff,
     0x79ffffff, 0x77ffffff, 0x75ffffff, 0x73ffffff,
     0x71ffffff, 0x6fffffff, 0x6dffffff, 0x6bffffff,
     0x53dfdfdf, 0x19bababa, 0x0be2e2e2, 0x03f5f5f5,
     0x01fdfdfd, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x01fcfcfc,
     0x06f5f5f5, 0x0fe3e3e3, 0x21c2c2c2, 0x378e8e8e,
     0x5e8b8b8b, 0x88fafafa, 0x88ffffff, 0x86ffffff,
     0x85ffffff, 0x83ffffff, 0x82ffffff, 0x80ffffff,
     0x7effffff, 0x7cffffff, 0x7bffffff, 0x79ffffff,
     0x77ffffff, 0x75ffffff, 0x73ffffff, 0x71ffffff,
     0x6fffffff, 0x6dffffff, 0x6bffffff, 0x68ffffff,
     0x39bebebe, 0x12c3c3c3, 0x08e8e8e8, 0x02f8f8f8,
     0x00fefefe, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x01fefefe,
     0x03fafafa, 0x09eeeeee, 0x16d6d6d6, 0x2aafafaf,
     0x42777777, 0x71b8b8b8, 0x85ffffff, 0x83ffffff,
     0x83ffffff, 0x81ffffff, 0x7fffffff, 0x7effffff,
     0x7cffffff, 0x7bffffff, 0x79ffffff, 0x77ffffff,
     0x74ffffff, 0x73ffffff, 0x71ffffff, 0x6fffffff,
     0x6dffffff, 0x6bffffff, 0x69ffffff, 0x57e0e0e0,
     0x20ababab, 0x0fd5d5d5, 0x06efefef, 0x02fbfbfb,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x01fdfdfd, 0x05f6f6f6, 0x0ee5e5e5, 0x1dc7c7c7,
     0x32999999, 0x4e757575, 0x7ce0e0e0, 0x81ffffff,
     0x80ffffff, 0x7effffff, 0x7dffffff, 0x7cffffff,
     0x7affffff, 0x78ffffff, 0x76ffffff, 0x74ffffff,
     0x73ffffff, 0x71ffffff, 0x6fffffff, 0x6dffffff,
     0x6bffffff, 0x69ffffff, 0x62f4f4f4, 0x32a9a9a9,
     0x16bdbdbd, 0x0be3e3e3, 0x04f5f5f5, 0x01fcfcfc,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x02fafafa, 0x07f1f1f1, 0x12dcdcdc,
     0x23bababa, 0x37878787, 0x56808080, 0x7be9e9e9,
     0x7dffffff, 0x7cffffff, 0x7bffffff, 0x79ffffff,
     0x77ffffff, 0x75ffffff, 0x74ffffff, 0x72ffffff,
     0x70ffffff, 0x6effffff, 0x6dffffff, 0x6bffffff,
     0x69ffffff, 0x64f6f6f6, 0x3dacacac, 0x1cababab,
     0x0fd6d6d6, 0x06eeeeee, 0x02f9f9f9, 0x00fefefe,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x01fdfdfd, 0x04f8f8f8, 0x0aebebeb,
     0x16d3d3d3, 0x27b1b1b1, 0x397e7e7e, 0x54777777,
     0x72d2d2d2, 0x7affffff, 0x78ffffff, 0x76ffffff,
     0x74ffffff, 0x74ffffff, 0x72ffffff, 0x70ffffff,
     0x6effffff, 0x6dffffff, 0x6bffffff, 0x68ffffff,
     0x5ee3e3e3, 0x3d9e9e9e, 0x1fa1a1a1, 0x13cdcdcd,
     0x09e7e7e7, 0x03f6f6f6, 0x01fdfdfd, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x01fcfcfc, 0x05f5f5f5,
     0x0ce6e6e6, 0x18cdcdcd, 0x28adadad, 0x37808080,
     0x49666666, 0x629b9b9b, 0x70dfdfdf, 0x74fefefe,
     0x73ffffff, 0x71ffffff, 0x6fffffff, 0x6dffffff,
     0x6cffffff, 0x6affffff, 0x63e6e6e6, 0x4fb1b1b1,
     0x34898989, 0x219f9f9f, 0x14c8c8c8, 0x0ae3e3e3,
     0x04f3f3f3, 0x01fbfbfb, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00fefefe, 0x02fafafa,
     0x06f3f3f3, 0x0de4e4e4, 0x17cecece, 0x24b3b3b3,
     0x31919191, 0x3d6d6d6d, 0x49666666, 0x56838383,
     0x5ea4a4a4, 0x61bababa, 0x61c2c2c2, 0x5ebdbdbd,
     0x57acacac, 0x4a929292, 0x397d7d7d, 0x2a888888,
     0x1eababab, 0x14cbcbcb, 0x0be1e1e1, 0x05f1f1f1,
     0x02fafafa, 0x00fefefe, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00fefefe,
     0x02fafafa, 0x06f3f3f3, 0x0be6e6e6, 0x13d5d5d5,
     0x1dc0c0c0, 0x26a9a9a9, 0x2f929292, 0x347b7b7b,
     0x386c6c6c, 0x3a6b6b6b, 0x3a6e6e6e, 0x36717171,
     0x30787878, 0x298c8c8c, 0x21a5a5a5, 0x19bebebe,
     0x11d3d3d3, 0x0ae5e5e5, 0x05f2f2f2, 0x02fafafa,
     0x00fefefe, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00fefefe, 0x02fbfbfb, 0x04f5f5f5, 0x08ededed,
     0x0de0e0e0, 0x14d2d2d2, 0x19c4c4c4, 0x1eb8b8b8,
     0x22afafaf, 0x23a9a9a9, 0x22a9a9a9, 0x20aeaeae,
     0x1cb8b8b8, 0x17c4c4c4, 0x11d2d2d2, 0x0ce0e0e0,
     0x07ececec, 0x04f5f5f5, 0x01fbfbfb, 0x00fefefe,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00fefefe, 0x01fcfcfc, 0x03f9f9f9,
     0x05f4f4f4, 0x08ededed, 0x0be5e5e5, 0x0ededede,
     0x0fd9d9d9, 0x11d6d6d6, 0x10d6d6d6, 0x0fd9d9d9,
     0x0ddedede, 0x09e5e5e5, 0x07ededed, 0x04f4f4f4,
     0x02f9f9f9, 0x01fcfcfc, 0x00fefefe, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00fefefe,
     0x01fcfcfc, 0x02fafafa, 0x03f7f7f7, 0x04f4f4f4,
     0x05f2f2f2, 0x06f1f1f1, 0x06f1f1f1, 0x05f2f2f2,
     0x04f4f4f4, 0x03f7f7f7, 0x02fafafa, 0x01fcfcfc,
     0x00fdfdfd, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00fefefe, 0x01fdfdfd, 0x01fcfcfc,
     0x01fcfcfc, 0x01fcfcfc, 0x01fcfcfc, 0x01fcfcfc,
     0x01fcfcfc, 0x01fdfdfd, 0x00fefefe, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
     0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
};
