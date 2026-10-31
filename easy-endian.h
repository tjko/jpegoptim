/* easy-endian.h
 *
 * Copyright (c) 2026 Timo Kokkonen
 * All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of JPEGoptim.
 *
 * JPEGoptim is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * JPEGoptim is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with JPEGoptim. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef EASY_ENDIAN_H
#define EASY_ENDIAN_H 1


#if defined(HAVE_ENDIAN_H)
 #include <endian.h>
#elif defined(HAVE_SYS_ENDIAN_H)
 #include <sys/endian.h>
#else
 #if defined(__APPLE__)
  #include <libkern/OSByteOrder.h>

  #define htobe16(x) OSSwapHostToBigInt16(x)
  #define htole16(x) OSSwapHostToLittleInt16(x)
  #define be16toh(x) OSSwapBigToHostInt16(x)
  #define le16toh(x) OSSwapLittleToHostInt16(x)

  #define htobe32(x) OSSwapHostToBigInt32(x)
  #define htole32(x) OSSwapHostToLittleInt32(x)
  #define be32toh(x) OSSwapBigToHostInt32(x)
  #define le32toh(x) OSSwapLittleToHostInt32(x)

  #define htobe64(x) OSSwapHostToBigInt64(x)
  #define htole64(x) OSSwapHostToLittleInt64(x)
  #define be64toh(x) OSSwapBigToHostInt64(x)
  #define le64toh(x) OSSwapLittleToHostInt64(x)
 #elif defined(__WINDOWS__) || defined(_WIN32)
  #if defined(_MSC_VER) && !defined(__clang__)
   #include <stdlib.h>
   #define ByteSwap16(x) _byteswap_ushort(x)
   #define ByteSwap32(x) _byteswap_ulong(x)
   #define ByteSwap64(x) _byteswap_uint64(x)
  #else
   #define ByteSwap16(x) __builtin_bswap16(x)
   #define ByteSwap32(x) __builtin_bswap32(x)
   #define ByteSwap64(x) __builtin_bswap64(x)
  #endif
  #if defined(__MINGW32__)
   #include <sys/param.h>
  #endif
  #ifndef BYTE_ORDER
   #error "BYTE_ORDER not defined"
  #endif
  #if BYTE_ORDER == LITTLE_ENDIAN
   #define htobe16(x) ByteSwap16(x)
   #define htole16(x) (x)
   #define be16toh(x) ByteSwap16(x)
   #define le16toh(x) (x)

   #define htobe32(x) ByteSwap32(x)
   #define htole32(x) (x)
   #define be32toh(x) ByteSwap32(x)
   #define le32toh(x) (x)

   #define htobe64(x) ByteSwap64(x)
   #define htole64(x) (x)
   #define be64toh(x) ByteSwap64(x)
   #define le64toh(x) (x)
  #else
   #define htobe16(x) (x)
   #define htole16(x) ByteSwap16(x)
   #define be16toh(x) (x)
   #define le16toh(x) ByteSwap16(x)

   #define htobe32(x) (x)
   #define htole32(x) ByteSwap32(x)
   #define be32toh(x) (x)
   #define le32toh(x) ByteSwap32(x)

   #define htobe64(x) (x)
   #define htole64(x) ByteSwap64(x)
   #define be64toh(x) (x)
   #define le64toh(x) ByteSwap64(x)
  #endif
 #else
  #error "currently unsupported platform"
 #endif
#endif


#endif /* EASY_ENDIAN_H */

/* eof :-) */
