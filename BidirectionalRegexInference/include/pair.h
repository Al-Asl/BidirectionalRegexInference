#ifndef PAIR_H
#define PAIR_H

#define HD

namespace rei {

    /// <summary>
    /// pair of two element that share the same type. unlike the built-in tuple can be used on both host and device
    /// </summary>
    template<typename T>
    struct Pair {
        T left;
        T right;

        HD inline Pair(T left, T right)
            : left(left), right(right) {
        }
    };

}

#endif // Pair