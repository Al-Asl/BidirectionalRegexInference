#ifndef OPEEATIONS_DEVICE_H
#define OPEEATIONS_DEVICE_H

#include <language_system.hpp>
#include <types.h>
#include <cuda_common.h>

__constant__ uint16_t deviceData[32768]; // 64kb

namespace rei
{

    class GuideTableDevice {
    public:
        class View {
        public:

            View(uint16_t* data, int columns, int rows) : data(data), columns(columns), rows(rows) {

            }

            HD PairRange<uint16_t> iterate(int rowIdx) const {
#ifdef GUIDE_TABLE_CONSTANT_MEMORY
                return PairRange<uint16_t>(deviceData, columns, rowIdx);
#else
                return PairRange<uint16_t>(data, columns, rowIdx);
#endif
            }

            HD rei::Pair<int> getSize() const {
                return { columns, rows };
            }

        private:
            uint16_t* data;
            int columns;
            int rows;
        };

        GuideTableDevice(const PairsTable<uint16_t>& table)
        {
            auto [columns, rows] = table.getSize();
            this->columns = columns;
            this->rows = rows;
            int bytes = columns * rows * sizeof(uint16_t);

#ifdef GUIDE_TABLE_CONSTANT_MEMORY
            checkCuda(cudaMemcpyToSymbol(deviceData, table.getData(), bytes));
            this->data = nullptr;
#else
            checkCuda(cudaMalloc(&data, bytes));
            checkCuda(cudaMemcpy(data, table.getData(), bytes, cudaMemcpyHostToDevice));
#endif

        }

        ~GuideTableDevice() {
#ifdef GUIDE_TABLE_CONSTANT_MEMORY
            uint16_t zeros[32768] = { 0 };
            checkCuda(cudaMemcpyToSymbol(deviceData, zeros, sizeof(zeros)));
#else
            checkCuda(cudaFree(data));
#endif
        }

        View getView() const {
            return View(data, columns, rows);
        }

    private:
        uint16_t* data;
        int columns;
        int rows;
    };
 
    __device__ void inline processQuestionDevice(CS& cs) {
        cs.setBitOn(0);
    }

    __device__ inline void processStarDevice(const GuideTableDevice::View& guideTable, int alphabetSize, CS& cs) {

        cs.setBitOn(0);
        int ix = alphabetSize + 1;
        int ICsize = guideTable.getSize().right;

        while (ix < ICsize)
        {
            if (!cs.getBit(ix)) {
                for (auto [left, right] : guideTable.iterate(ix)) {
                    if (cs.getBit(left) && cs.getBit(right)) { cs.setBitOn(ix); break; }
                }
            }
            ix++;
        }
    }

    __device__ inline void processConcatenateDevice(const GuideTableDevice::View& guideTable, int alphabetSize, const CS& left, const CS& right, CS& res) {

        if (left.getBit(0)) res |= right;
        if (right.getBit(0)) res |= left;

        int ix = alphabetSize + 1;
        int ICsize = guideTable.getSize().right;

        while (ix < ICsize)
        {
            // when CS have value that means one of parts contains phi, check above
            if (!res.getBit(ix)) {
                for (auto [l, r] : guideTable.iterate(ix))
                    if (left.getBit(l) && right.getBit(r)) { res.setBitOn(ix); break; }
            }
            ix++;
        }
    }

    __device__ inline void processOrDevice(const CS& left, const CS& right, CS& res) {
        res |= left;
        res |= right;
    }
}

#endif