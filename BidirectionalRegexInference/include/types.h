#ifndef TYPES_H
#define TYPES_H

#include <bitmask.h>

#if CS_BIT_COUNT == 0
using CS = rei::bitmask<2>;
#elif CS_BIT_COUNT == 1
using CS = rei::bitmask<4>;
#elif CS_BIT_COUNT == 2
using CS = rei::bitmask<8>;
#elif CS_BIT_COUNT == 3
using CS = rei::bitmask<16>;
#elif CS_BIT_COUNT == 4
using CS = rei::bitmask<32>;
#else
using CS = rei::bitmask<64>;
#endif

#endif // end TYPES_H