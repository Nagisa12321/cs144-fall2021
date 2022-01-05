#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint64_t round = static_cast<uint64_t>(UINT32_MAX) + 1;
    uint32_t tmp = n % round;
    return WrappingInt32(tmp + isn.raw_value());
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint64_t round = static_cast<uint64_t>(UINT32_MAX) + 1;
    uint64_t n_64 = n.raw_value(), isn_64 = isn.raw_value();
    uint64_t first_num;
    if (n_64 < isn_64) first_num = n_64 + round - isn_64;
    else first_num = n_64 - isn_64; 

    if (checkpoint <= first_num)
        return first_num;

    uint64_t k1, k2;
    k1 = static_cast<long double>(checkpoint - first_num) / round;
    k2 = k1 + 1;
    
    uint64_t r1, r2;
    r1 = first_num + k1 * round;
    r2 = first_num + k2 * round;

    uint64_t d1, d2;
    d1 = checkpoint - r1;
    d2 = r2 - checkpoint;
    if (d1 < d2) return r1;
    else return r2;
}
