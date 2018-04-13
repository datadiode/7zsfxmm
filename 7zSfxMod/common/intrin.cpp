/*
	Copyright (C) 2018 Jochen Neubeck

	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

// Work around MSVC no longer respecting #pragma intrinsic(memcpy, memset)

#include <intrin.h>

#define FAVOR_SMALL_CODE // probably also faster for small chunks of memory

extern "C"
{
	void *memcpy(void *dst, void const *src, size_t len)
	{
#ifdef FAVOR_SMALL_CODE
		__movsb(static_cast<unsigned char *>(dst), static_cast<unsigned char const *>(src), len);
#else
		unsigned char *u = static_cast<unsigned char *>(dst);
		unsigned char const *v = static_cast<unsigned char const *>(src);
#ifdef _WIN64
		__movsq(reinterpret_cast<unsigned __int64 *>(u), reinterpret_cast<unsigned __int64 const *>(v), len >> 3);
		u += len & ~7; v += len & ~7; len &= 7;
#endif
		__movsd(reinterpret_cast<unsigned long *>(u), reinterpret_cast<unsigned long const *>(v), len >> 2);
		u += len & ~3; v += len & ~3; len &= 3;
		__movsw(reinterpret_cast<unsigned short *>(u), reinterpret_cast<unsigned short const *>(v), len >> 1);
		u += len & ~1; v += len & ~1; len &= 1;
		__movsb(reinterpret_cast<unsigned char *>(u), reinterpret_cast<unsigned char const *>(v), len);
#endif
		return dst;
	}
	void *memset(void *dst, int c, size_t len)
	{
#ifdef FAVOR_SMALL_CODE
		__stosb(static_cast<unsigned char *>(dst), static_cast<unsigned char>(c), len);
#else
		unsigned char *u = static_cast<unsigned char *>(dst);
		union
		{
			unsigned char b[1];
			unsigned short w[1];
			unsigned long d[1];
#ifdef _WIN64
			unsigned __int64 q[1];
#endif
		} v;
		v.b[0] = static_cast<unsigned char>(c);
		v.b[1] = v.b[0];
		v.w[1] = v.w[0];
#ifdef _WIN64
		v.d[1] = v.d[0];
		__stosq(reinterpret_cast<unsigned __int64 *>(u), *v.q, len >> 3);
		u += len & ~7; len &= 7;
#endif
		__stosd(reinterpret_cast<unsigned long *>(u), *v.d, len >> 2);
		u += len & ~3; len &= 3;
		__stosw(reinterpret_cast<unsigned short *>(u), *v.w, len >> 1);
		u += len & ~1; len &= 1;
		__stosb(reinterpret_cast<unsigned char *>(u), *v.b, len);
#endif
		return dst;
	}
}
