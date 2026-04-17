#pragma once

#ifdef _MSC_VER
#if defined(_M_ARM64) || defined(_M_ARM64EC) || defined(_M_HYBRID_X86_ARM64)

// Windows SDK 10.0.26100 with MSVC v142 may reference _CountOneBits64
// before the toolchain provides it for ARM64 builds.
static __forceinline unsigned short __cdecl medicalpro_count_one_bits64(unsigned __int64 value)
{
    unsigned short count = 0;
    while (value != 0) {
        count = static_cast<unsigned short>(count + static_cast<unsigned short>(value & 1ULL));
        value >>= 1;
    }
    return count;
}

#ifndef _CountOneBits64
#define _CountOneBits64 medicalpro_count_one_bits64
#endif

#endif
#endif
