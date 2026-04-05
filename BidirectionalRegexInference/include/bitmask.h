#ifndef BITMASK_H
#define BITMASK_H

#include <cstdint>
#include <iostream>
#include <iomanip>
#include <functional>
#include <bit>
#include <pair.h>

template <class T>
using Pair = rei::Pair<T>;

#define RELAX_UNIQUENESS_CHECK_TYPE 2

namespace rei
{
    template <typename T>
    struct BitmaskView {
    public:

        static int getChuncksSize(int bitsSize) {
            return (bitsSize - 1) / (sizeof(T) * 8) + 1;
        }

        static constexpr int chunckBitSize = sizeof(T) * 8;

        BitmaskView(T* ptr, int size) : ptr(ptr) , size(size) { }

        BitmaskView static createInvalid() { return BitmaskView(nullptr, 0); }

        bool isValid() const { return ptr != nullptr; }

        BitmaskView& setChunck(T value, int idx) {
            ptr[idx] = value;
            return *this;
        }

        uint64_t getChunck(int idx) const {
            return ptr[idx];
        }

        BitmaskView& toggleBit(int bitIndex) {
            int eIdx = bitIndex / chunckBitSize;
            int bIdx = bitIndex % chunckBitSize;
            ptr[eIdx] ^= (1ULL << bIdx);
            return *this;
        }

        BitmaskView& setBitOn(int bitIndex) {
            int eIdx = bitIndex / chunckBitSize;
            int bIdx = bitIndex % chunckBitSize;
            ptr[eIdx] |= (1ULL << bIdx);
            return *this;
        }

        BitmaskView& setBitOff(int bitIndex) {
            int eIdx = bitIndex / chunckBitSize;
            int bIdx = bitIndex % chunckBitSize;
            ptr[eIdx] &= ~(1ULL << bIdx);
            return *this;
        }

        bool getBit(int bitIndex) const {
            int eIdx = bitIndex / chunckBitSize;
            int bIdx = bitIndex % chunckBitSize;
            return (ptr[eIdx] >> bIdx) & 1ULL;
        }

        BitmaskView& operator|=(const BitmaskView& other) {
            for (int i = 0; i < size; i++) {
                ptr[i] |= other.ptr[i];
            }
            return *this;
        }

        BitmaskView& operator&=(const BitmaskView& other) {
            for (int i = 0; i < size; i++) {
                ptr[i] &= other.ptr[i];
            }
            return *this;
        }

        inline bool intersects(const BitmaskView& other) const {
            for (int i = 0; i < size; ++i) {
                if ((ptr[i] & other.ptr[i]) != 0) {
                    return true;
                }
            }
            return false;
        }

        BitmaskView& operator^=(const BitmaskView& other) {
            for (int i = 0; i < size; i++) {
                ptr[i] ^= other.ptr[i];
            }
            return *this;
        }

        bool containsAll(const BitmaskView& other) const {
            for (int i = 0; i < size; i++) {
                if ((ptr[i] & other.ptr[i]) != other.ptr[i]) {
                    return false;
                }
            }
            return true;
        }

        bool containsNone(const BitmaskView& other) const {
            for (int i = 0; i < size; i++) {
                if ((ptr[i] & other.ptr[i]) != 0) {
                    return false;
                }
            }
            return true;
        }

        bool isEmpty() const {
            for (int i = 0; i < size; i++) {
                if (ptr[i])
                    return false;
            }
            return true;
        }

        Pair<uint64_t> getHash128() const {

            if (size == 2)
            {
                return {ptr[0], ptr[1]};
            }

            uint64_t lCS = 0, hCS = 0;
#if RELAX_UNIQUENESS_CHECK_TYPE == 0
            const int stride = (size * 64) / 126;

            int j = 0;
            for (int i = 0; i < size; ++i) {
                for (int k = 0; k < 64; k += stride, ++j) {
                    if (j < 63) {
                        if (ptr[i] & ((uint64_t)1 << k)) lCS |= (uint64_t)1 << j;
                    }
                    else if (j < 126) {
                        if (ptr[i] & ((uint64_t)1 << k)) hCS |= (uint64_t)1 << (j - 63);
                    }
                    else break;
                }
            }
#elif RELAX_UNIQUENESS_CHECK_TYPE == 1
            int j = 0;
            for (int i = 0; i < size; ++i) {
                uint64_t bitPtr = 1;
                int maxbitsForThisTrace = (126 * 64 + 64 * size) / 64 * size;
                for (int k = 0; k < maxbitsForThisTrace; ++k, ++j, bitPtr <<= 1) {
                    if (j < 63) {
                        if (ptr[i] & bitPtr) lCS |= (uint64_t)1 << j;
                    }
                    else if (j < 126) {
                        if (ptr[i] & bitPtr) hCS |= (uint64_t)1 << (j - 63);
                    }
                    else break;
                }
            }
#elif RELAX_UNIQUENESS_CHECK_TYPE == 2
            for (int i = 0; i < size; ++i) {
                uint64_t x = ptr[i];
                x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
                x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
                x = x ^ (x >> 31);
                if (i < size / 2) hCS ^= x; else lCS ^= x;
            }
#else
            int j = 0;
            for (int i = 0; i < size; ++i) {
                uint64_t bitPtr = 1;
                for (int k = 0; k < 64; ++k, ++j, bitPtr <<= 1) {
                    if (j < 63) {
                        if (ptr[i] & bitPtr) lCS |= (uint64_t)1 << j;
                    }
                    else if (j < 126) {
                        if (ptr[i] & bitPtr) hCS |= (uint64_t)1 << (j - 63);
                    }
                    else break;
                }
            }
#endif

            return { lCS, hCS };
        }

        uint64_t getHash() const {

            if (size == 1)
                return std::hash<uint64_t>{}(ptr[0]);

            auto [low, high] = getHash128();

            std::size_t h1 = std::hash<uint64_t>{}(low);
            std::size_t h2 = std::hash<uint64_t>{}(high);
            return h1 ^ (h2 << 1);
        }

        void copyTo(uint64_t* destBuffer) const {
            std::memcpy(destBuffer, ptr, size * sizeof(uint64_t));
        }

        void copyTo(BitmaskView& bitmask) const {
            std::memcpy(bitmask.ptr, ptr, size * sizeof(uint64_t));
        }

        BitmaskView& copy(const uint64_t* destBuffer) {
            std::memcpy(ptr, destBuffer, size * sizeof(uint64_t));
            return *this;
        }

        BitmaskView& copy(const BitmaskView& bitmask) {
            std::memcpy(ptr, bitmask.ptr, size * sizeof(uint64_t));
            return *this;
        }

        BitmaskView& clear() {
            std::fill(ptr, ptr + size, 0);
            return *this;
        }

        uint64_t popCount() const {
            uint64_t pc{};
            for (size_t i = 0; i < size; i++)
                pc += std::popcount(ptr[i]);
            return pc;
        }

        std::vector<int> getBits() const {
            std::vector<int> res;

            for (int chunckIdx = 0; chunckIdx < size; ++chunckIdx) {
                uint64_t chunk = ptr[chunckIdx];

                if (chunk == 0) continue;

                for (int bitIdx = 0; bitIdx < chunckBitSize; ++bitIdx) {
                    if (chunk & (1ULL << bitIdx))
                        res.push_back(chunckIdx * 64 + bitIdx);
                }
            }

            return res;
        }

        BitmaskView& invert(){
            for (int i = 0; i < size; ++i) {
                ptr[i] = ~ptr[i];
            }
            return *this;
        }

        bool operator==(const BitmaskView& other) const {
            for (int i = 0; i < size; ++i) {
                if (ptr[i] != other.ptr[i]) { return false; }
            }
            return true;
        }

        bool operator!=(const BitmaskView& other) const {
            for (int i = 0; i < size; ++i) {
                if (ptr[i] != other.ptr[i]) { return true; }
            }
            return false;
        }

        inline operator bool() const {
            for (int i = 0; i < size; ++i) {
                if (ptr[i] != 0) { return true; }
            }
            return false;
        }

        inline bool operator!() const {
            return !static_cast<bool>(*this);
        }

        BitmaskView& operator--() {
            for (int i = 0; i < size; ++i) {
                if (ptr[i] != 0) {
                    --ptr[i];
                    break;
                }

                ptr[i] = (T)-1;
            }
            return *this;
        }

        BitmaskView& operator++() {
            for (int i = 0; i < size; ++i) {
                ++ptr[i];
                if (ptr[i] != 0) {
                    break;
                }
            }
            return *this;
        }

        BitmaskView& operator>>=(int shift) {

            if (shift < 0) {
                return *this <<= -shift;
            }

            int wordShift = shift / chunckBitSize;
            int bitShift = shift % chunckBitSize;

            if (wordShift >= size) {
                for (int i = 0; i < size; ++i) { ptr[i] = 0; }
                return *this;
            }

            for (int i = 0; i < size - wordShift; ++i) {
                ptr[i] = ptr[i + wordShift];
            }
            for (int i = size - wordShift; i < size; ++i) {
                ptr[i] = 0;
            }

            if (bitShift > 0) {
                for (int i = 0; i < size - 1; ++i) {
                    ptr[i] >>= bitShift;
                    ptr[i] |= ptr[i + 1] << (chunckBitSize - bitShift);
                }
                ptr[size - 1] >>= bitShift;
            }

            return *this;
        }

        BitmaskView& operator<<=(int shift) {

            if (shift < 0) {
                return *this >>= -shift;
            }

            int wordShift = shift / chunckBitSize;
            int bitShift = shift % chunckBitSize;

            if (wordShift >= size) {
                for (int i = 0; i < size; ++i) ptr[i] = 0;
                return *this;
            }

            for (int i = size - 1; i >= wordShift; --i) {
                ptr[i] = ptr[i - wordShift];
            }

            for (int i = wordShift - 1; i >= 0; --i) {
                ptr[i] = 0;
            }

            if (bitShift > 0) {
                for (int i = size - 1; i > 0; --i) {
                    ptr[i] <<= bitShift;
                    ptr[i] |= ptr[i - 1] >> (chunckBitSize - bitShift);
                }
                ptr[0] <<= bitShift;
            }

            return *this;
        }

        int getSize() const { return size; }

        friend std::ostream& operator<<(std::ostream& os, const BitmaskView& mask) {
            os << std::hex << "0x";
            for (int i = size - 1; i >= 0; --i) {
                os << std::setfill('0') << std::setw(16) << mask.ptr[i];
            }
            os << std::dec;
            return os;
        }

    private:
        T* ptr;
        int size;
    };

    template<typename T>
    class BitmaskBufferView {
    public:

        BitmaskBufferView() = default;

        BitmaskBufferView(T* ptr, int elementsCount, int numChunkPerElement, bool isFull = false)
            : ptr(ptr), elementsCount(elementsCount), numChunkPerElement(numChunkPerElement), _count(isFull ? elementsCount : 0) {
        }

        BitmaskView<T> at(int index) const {
            return  BitmaskView(ptr + index * numChunkPerElement, numChunkPerElement);
        }

        BitmaskView<T> operator[](int index) const {
            return BitmaskView(ptr + index * numChunkPerElement, numChunkPerElement);
        }

        T* data() { return ptr; }
        int size() const { return elementsCount; }
        int getNumChuncksPerElement() const { return numChunkPerElement; }

        BitmaskBufferView getView(int start, int elementsCount, bool isFull = false) { 
            return BitmaskBufferView(ptr + start * numChunkPerElement, elementsCount, numChunkPerElement, isFull);
        }

        int count() const { return _count; }
        BitmaskView<T> append() { return BitmaskView(ptr + _count++ * numChunkPerElement, numChunkPerElement); }
        void removeLast() { _count--; }
        bool reset() { _count = 0; }

        bool isFull() const { return _count == elementsCount; }
        bool isEmpty() const { return _count == 0; }

    private:
        T* ptr;
        int elementsCount;
        int numChunkPerElement;
        int _count;
    };
}

#endif //BITMASK_H