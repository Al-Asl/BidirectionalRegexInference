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

        HD Pair(const Pair& other)
            : left(other.left), right(other.right) {
        }

        HD Pair(Pair&& other) noexcept
            : left(std::move(other.left)),
            right(std::move(other.right)) {
        }

        HD Pair& operator=(const Pair& other) {
            if (this != &other) {
                left = other.left;
                right = other.right;
            }
            return *this;
        }

        HD Pair& operator=(Pair&& other) noexcept {
            if (this != &other) {
                left = std::move(other.left);
                right = std::move(other.right);
            }
            return *this;
        }

        HD inline bool operator==(const Pair& other) const {
            return left == other.left && right == other.right;
        }

        HD inline bool operator!=(const Pair& other) const {
            return !(*this == other);
        }

    };
}

namespace std {
    template<typename T>
    struct hash<rei::Pair<T>> {
        size_t operator()(const rei::Pair<T>& p) const {
            return std::hash<T>{}(p.left)
                ^ (std::hash<T>{}(p.right) << 1);
        }
    };
}

#endif // Pair