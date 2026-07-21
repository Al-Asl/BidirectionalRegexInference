#include "bottom_up_device.h"

#include<map>

#include <thrust/copy.h>
#include <thrust/remove.h>
#include <thrust/device_ptr.h>
#include <warpcore/hash_set.cuh>
#include <remove_stride_groups.h>

using namespace rei;

#define THREAD_COUNT 128
#define UINT64_PER_THREAD 64

#define EXCEED_SHORTAGE_COST 10
#define LOG_OPERATIONS

#ifdef LOG_OPERATIONS
    #define LOG_OP(context, cost, op_string, dif) \
            int tbc = dif; \
            if (tbc) printf("Cost %-2d | (%s) | AllREs: %-11llu | StoredREs: %-10d | ToBeChecked: %-10d \n", \
                cost, op_string.c_str() ,context.allREs, context.lastIdx, tbc);
#else
    #define LOG_OP(context, cost, op_string, dif)
#endif

#define CREATE_CS(name, size) \
    std::vector<uint64_t> name##_data(size,0); \
    CS name(name##_data.data(), size); \

template <typename T>
class DeviceAppendBuffer {

private:
    T* d_data = nullptr;
    int* d_count = nullptr;
    int* count = nullptr;
    int m_capacity = 0;

public:
    struct View {
        T* m_data;
        int* m_count;
        int m_capacity;

        __device__ bool push_back(const T& value) {
            int insert_idx = atomicAdd(m_count, 1);
            if (insert_idx < m_capacity) {
                m_data[insert_idx] = value;
                return true;
            }
            return false;
        }

        __device__ int size() const {
            int current_count = *m_count;
            return current_count > m_capacity ? m_capacity : current_count;
        }
        __device__ T& operator[](int index) { return m_data[index]; }
        __device__ const T& operator[](int index) const { return m_data[index]; }
    };

    DeviceAppendBuffer(int capacity) : m_capacity(capacity) {
        cudaMalloc(&d_data, m_capacity * sizeof(T));
        cudaMalloc(&d_count, sizeof(int));
        count = new int[1];
        clear();
    }

    ~DeviceAppendBuffer() {
        cudaFree(d_data);
        cudaFree(d_count);
    }

    void clear() {
        count[0] = 0;
        cudaMemcpy(d_count, count, sizeof(int), cudaMemcpyHostToDevice);
    }

    View getview() {
        return View{d_data, d_count, m_capacity};
    }

    int previousSize() const {
        return count[0];
    }

    int capacity() const {
        return m_capacity;
    }

    int size() const {
        cudaMemcpy(count, d_count, sizeof(int), cudaMemcpyDeviceToHost);
        return count[0] > m_capacity ? m_capacity : count[0];
    }

    void copyToHost(T* host_destination, int elements_to_copy) {
        if (elements_to_copy > m_capacity) elements_to_copy = m_capacity;
        cudaMemcpy(host_destination, d_data, elements_to_copy * sizeof(T), cudaMemcpyDeviceToHost);
    }
};

struct DeviceHashSet
{
    using hash_set_t = warpcore::HashSet<
        uint64_t,         // key type
        uint64_t(0) - 1,  // empty key
        uint64_t(0) - 2,  // tombstone key
        warpcore::probing_schemes::QuadraticProbing<warpcore::hashers::MurmurHash <uint64_t>>>;

    __host__ DeviceHashSet(int capacity) : cHashSet(capacity), iHashSet(capacity) {}

    inline __device__ bool insert(uint64_t high, uint64_t low) {
        return insert(high, low, warpcore::cg::tiled_partition<1>(warpcore::cg::this_thread_block()));
    }

    inline __device__ bool insert(uint64_t high, uint64_t low, const warpcore::cg::thread_block_tile<1>& group) {
        int H = cHashSet.insert(high, group);
        int L = cHashSet.insert(low, group);
        H = (H > 0) ? H : -H;
        L = (L > 0) ? L : -L;
        uint64_t HL = H; HL <<= 32; HL |= L;
        return !(iHashSet.insert(HL, group) > 0);
    }

private:
    hash_set_t cHashSet;
    hash_set_t iHashSet;
};

// Initializing context with empty, epsilon and alphabet before starting the enumeration
__global__ void init_context(DeviceHashSet d_visited, CSBuffer d_langCache, int alphabetSize)
{
    // Adding empty to the hashSet
    d_visited.insert(0,0);

    // Adding eps to the hashSet
    d_visited.insert(0,1);

    // Adding alphabet to the hashSet
    for (int i = 0; i < alphabetSize; ++i) {
        auto cs = d_langCache[i];
        cs.clear().setBitOn(i + 1);
        auto [low, high] = cs.getHash128();
        d_visited.insert(high, low);
    }
}

class Context {
public:

    static rei::Pair<uint64_t> getCacheCapacity(uint64_t memory_size, int ICSize, double tempRatio = 0.5) {

        auto chunksPerCS = CS::getChuncksSize(ICSize);

        uint64_t cacheCapacity = memory_size / (
            (sizeof(uint64_t) * chunksPerCS + sizeof(int) * 2 + sizeof(uint64_t) * 4) +
            (sizeof(uint64_t) * chunksPerCS + sizeof(int) * 2) * tempRatio);

        return { cacheCapacity , (uint64_t)(cacheCapacity * tempRatio) };
    }

    class View
    {
    public:
        View(Context& context)
            : d_langCache(context.d_langCache),
            d_temp_langCache(context.d_temp_langCache),
            d_visited(context.d_visited),
            d_temp_leftIdx(context.d_temp_leftIdx),
            d_temp_rightIdx(context.d_temp_rightIdx),
            foundedIds(context.foundedIds.getview()),
            onTheFly(context.onTheFly),
            posBits(context.posBits),
            negBits(context.negBits)
        {
        }

        CSBuffer d_langCache;
        CSBuffer d_temp_langCache;
        DeviceHashSet d_visited;
        int* d_temp_leftIdx;
        int* d_temp_rightIdx;

        DeviceAppendBuffer<int>::View foundedIds;

        bool onTheFly;
        CS posBits, negBits;

        __device__ inline void insert(const CS& cs, int tid, int ldx, int rdx = 0) {
            insert(cs, tid, ldx, rdx, warpcore::cg::tiled_partition<1>(warpcore::cg::this_thread_block()));
        }

        __device__ inline void insert(const CS& cs, int tid, int ldx, int rdx, const warpcore::cg::thread_block_tile<1>& group) {

            if (onTheFly) {

                if (cs.containsAll(posBits) && cs.containsNone(negBits)) {
                    foundedIds.push_back(tid);
                    d_temp_langCache[tid].copy(cs);
                    d_temp_leftIdx[tid] = ldx;
                    d_temp_rightIdx[tid] = rdx;
                }
            }
            else {
                auto [low, high] = cs.getHash128();
                if (d_visited.insert(high, low)) {
                    d_temp_langCache[tid].copy(cs);
                    d_temp_leftIdx[tid] = ldx;
                    d_temp_rightIdx[tid] = rdx;
                    if (cs.containsAll(posBits) && cs.containsNone(negBits))
                    {
                        foundedIds.push_back(tid);
                    }
                }
                else {
                    d_temp_langCache[tid].clear().invert();
                    d_temp_leftIdx[tid] = -1;
                    d_temp_rightIdx[tid] = -1;
                }
            }
        }
    };

    Context(const rei::LanguageSystem& languageSystem, const rei::InputParams& inputParams, int cache_capacity, int temp_cache_capacity)
        : cache_capacity(cache_capacity), temp_cache_capacity(temp_cache_capacity), posBits(posBits), negBits(negBits),
        d_visited(cache_capacity * 2), foundedIds(inputParams.n) {

        auto chunksPerCS = CS::getChuncksSize(languageSystem.getIC().size());

        checkCuda(cudaMalloc(&d_langCache_data, cache_capacity * sizeof(uint64_t) * chunksPerCS));
        checkCuda(cudaMalloc(&d_temp_langCache_data, temp_cache_capacity * sizeof(uint64_t) * chunksPerCS));
        checkCuda(cudaMalloc(&d_leftIdx, cache_capacity * sizeof(int)));
        checkCuda(cudaMalloc(&d_rightIdx, cache_capacity * sizeof(int)));
        checkCuda(cudaMalloc(&d_temp_leftIdx, temp_cache_capacity * sizeof(int)));
        checkCuda(cudaMalloc(&d_temp_rightIdx, temp_cache_capacity * sizeof(int)));

        std::vector<uint64_t> posNegData = rei::posNegCSData(languageSystem, inputParams);
        checkCuda(cudaMalloc(&d_posNeg_data, 2 * sizeof(uint64_t) * chunksPerCS));
        checkCuda(cudaMemcpy(d_posNeg_data, posNegData.data(), 2 * sizeof(uint64_t) * chunksPerCS, cudaMemcpyHostToDevice));

        posBits = CS(d_posNeg_data, chunksPerCS);
        negBits = CS(d_posNeg_data + chunksPerCS, chunksPerCS);

        d_langCache = CSBuffer(d_langCache_data, cache_capacity, chunksPerCS);
        d_temp_langCache = CSBuffer(d_temp_langCache_data, temp_cache_capacity, chunksPerCS);

        init_context<<<1,1>>>(d_visited, d_langCache, languageSystem.getAlphabetSize());

        lastIdx = languageSystem.getAlphabetSize();
        allREs = 0;
        onTheFly = false;
    }

    ~Context() {

        checkCuda(cudaFree(d_langCache_data));
        checkCuda(cudaFree(d_temp_langCache_data));
        checkCuda(cudaFree(d_leftIdx));
        checkCuda(cudaFree(d_rightIdx));
        checkCuda(cudaFree(d_temp_leftIdx));
        checkCuda(cudaFree(d_temp_rightIdx));

        checkCuda(cudaFree(d_posNeg_data));
    }

    View getView() {
        return View(*this);
    }

    void syncAndCheck(int REs, std::vector<int>& foundedIds) {

        allREs += REs;

        int previousSize = this->foundedIds.previousSize();
        int size = this->foundedIds.size();

        if (size - previousSize > 0)
        {
            std::vector<int> ids(size);
            this->foundedIds.copyToHost(ids.data(), size);
            for (int i = previousSize; i < size; i++)
                foundedIds.push_back(ids[i]);
        }

        if (size == this->foundedIds.capacity())
            return;

        if (!onTheFly) storeUniqueREs(REs);
    }

    void storeUniqueREs(int N) {
        int chunksPerCS = d_langCache.getNumChuncksPerElement();

        thrust::device_ptr<uint64_t> new_end_ptr;
        thrust::device_ptr<uint64_t> d_langCache_ptr(d_langCache_data + lastIdx * chunksPerCS);
        thrust::device_ptr<uint64_t> d_temp_langCache_ptr(d_temp_langCache_data);
        thrust::device_ptr<int> d_leftIdx_ptr(d_leftIdx + lastIdx);
        thrust::device_ptr<int> d_rightIdx_ptr(d_rightIdx + lastIdx);
        thrust::device_ptr<int> d_temp_leftIdx_ptr(d_temp_leftIdx);
        thrust::device_ptr<int> d_temp_rightIdx_ptr(d_temp_rightIdx);

        new_end_ptr = thrust::device_ptr<uint64_t>(remove_stride_groups(d_temp_langCache_data, N * chunksPerCS, chunksPerCS, (uint64_t)-1));
        //new_end_ptr = // end of d_temp_langCache
        //    thrust::remove(d_temp_langCache_ptr, d_temp_langCache_ptr + N, (uint64_t)-1);
        thrust::remove(d_temp_leftIdx_ptr, d_temp_leftIdx_ptr + N, -1);
        thrust::remove(d_temp_rightIdx_ptr, d_temp_rightIdx_ptr + N, -1);

        int numberOfNewUniqueREs = static_cast<int>(new_end_ptr - d_temp_langCache_ptr) / chunksPerCS;
        if (lastIdx + numberOfNewUniqueREs > cache_capacity) {
            printf(">>> switch to \"OnTheFly\" >>>\n");
            N = cache_capacity - lastIdx;
            onTheFly = true;
        }
        else 
            N = numberOfNewUniqueREs;

        thrust::copy_n(d_temp_langCache_ptr, N * chunksPerCS, d_langCache_ptr);
        thrust::copy_n(d_temp_leftIdx_ptr, N, d_leftIdx_ptr);
        thrust::copy_n(d_temp_rightIdx_ptr, N, d_rightIdx_ptr);

        lastIdx += N;
    }

    int cache_capacity;
    int temp_cache_capacity;

    CSBuffer d_langCache;
    uint64_t* d_langCache_data;
    CSBuffer d_temp_langCache;
    uint64_t* d_temp_langCache_data;
    DeviceHashSet d_visited;

    int* d_leftIdx;
    int* d_rightIdx;
    int* d_temp_leftIdx;
    int* d_temp_rightIdx;
    DeviceAppendBuffer<int> foundedIds;

    uint64_t allREs;
    // Index of the last free position in the language cache
    uint64_t lastIdx;
    bool onTheFly;

    uint64_t* d_posNeg_data;
    CS posBits, negBits;
};

// Finding the left and right indices that makes the final RE to bring to the host later
__global__ void generateResIndices(
    const int index,
    const int alphabetSize,
    const int* d_leftIdx,
    const int* d_rightIdx,
    int* d_FinalREIdx)
{

    int resIdx = 0;
    while (d_FinalREIdx[resIdx] != -1) resIdx++;
    int queue[600];
    queue[0] = index;
    int head = 0;
    int tail = 1;
    while (head < tail) {
        int re = queue[head];
        int l = d_leftIdx[re];
        int r = d_rightIdx[re];
        d_FinalREIdx[resIdx++] = re;
        d_FinalREIdx[resIdx++] = l;
        d_FinalREIdx[resIdx++] = r;
        if (l >= alphabetSize) queue[tail++] = l;
        if (r >= alphabetSize) queue[tail++] = r;
        head++;
    }
}

// Adding parentheses if needed
std::string bracket(std::string s) {
    int p = 0;
    for (int i = 0; i < s.length(); i++) {
        if (s[i] == '(') p++;
        else if (s[i] == ')') p--;
        else if ((s[i] == '+' || s[i] == '&') && p <= 0) return "(" + s + ")";
    }
    return s;
}

// Generating the final RE string recursively
// When all the left and right indices are ready in the host
std::string constructDownward(
    int index,
    std::map<int, std::pair<int, int>>& indicesMap,
    const std::vector<char>& alphabet,
    const rei::LevelPartitioner& partitioner)
{
    if (index == -2) return "eps"; // Epsilon
    if (index < alphabet.size()) { std::string s(1, *next(alphabet.begin(), index)); return s; }

    auto [cost, op] = partitioner.indexToLevel(index);

    if (op == Operation::Question) {
        std::string res = constructDownward(indicesMap[index].first, indicesMap, alphabet, partitioner);
        if (res.length() > 1) return "(" + res + ")?";
        return res + "?";
    }

    if (op == Operation::Star) {
        std::string res = constructDownward(indicesMap[index].first, indicesMap, alphabet, partitioner);
        if (res.length() > 1) return "(" + res + ")*";
        return res + "*";
    }

    if (op == Operation::Concatenate) {
        std::string left = constructDownward(indicesMap[index].first, indicesMap, alphabet, partitioner);
        std::string right = constructDownward(indicesMap[index].second, indicesMap, alphabet, partitioner);
        return bracket(left) + bracket(right);
    }

    if (op == Operation::Or)
    {
        std::string left = constructDownward(indicesMap[index].first, indicesMap, alphabet, partitioner);
        std::string right = constructDownward(indicesMap[index].second, indicesMap, alphabet, partitioner);
        return left + "+" + right;
    }
}

#ifdef CS_DECOMPOSETION
std::vector<std::pair<rei::Operation, std::vector<uint64_t>>> constructCSDecomposetion(const Context& context, const rei::LanguageSystem& languageSystem, const rei::LevelPartitioner& partitioner, int idx, bool idxInTemp = true) {

    auto* LIdx = new int[1];
    auto* RIdx = new int[1];

    if (idxInTemp)
    {
        checkCuda(cudaMemcpy(LIdx, context.d_temp_leftIdx + idx, sizeof(int), cudaMemcpyDeviceToHost));
        checkCuda(cudaMemcpy(RIdx, context.d_temp_rightIdx + idx, sizeof(int), cudaMemcpyDeviceToHost));
        idx += context.lastIdx;
    }
    else
    {
        checkCuda(cudaMemcpy(LIdx, context.d_leftIdx + idx, sizeof(int), cudaMemcpyDeviceToHost));
        checkCuda(cudaMemcpy(RIdx, context.d_rightIdx + idx, sizeof(int), cudaMemcpyDeviceToHost));
    }

    auto alphabetSize = languageSystem.getAlphabetSize();

    int* d_resIndices;
    checkCuda(cudaMalloc(&d_resIndices, 600 * sizeof(int)));

    thrust::device_ptr<int> d_resIndices_ptr(d_resIndices);
    thrust::fill(d_resIndices_ptr, d_resIndices_ptr + 600, -1);

    if (*LIdx >= alphabetSize)
        generateResIndices<<<1, 1 >>>(*LIdx, alphabetSize, context.d_leftIdx, context.d_rightIdx, d_resIndices);
    if (*RIdx >= alphabetSize)
        generateResIndices<<<1, 1 >>>(*RIdx, alphabetSize, context.d_leftIdx, context.d_rightIdx, d_resIndices);

    int resIndices[600];
    checkCuda(cudaMemcpy(resIndices, d_resIndices, 600 * sizeof(int), cudaMemcpyDeviceToHost));

    int chunksPerCS = context.d_langCache.getNumChuncksPerElement();
    std::vector<std::pair<rei::Operation, std::vector<uint64_t>>> res;

    {
        std::vector<uint64_t> cs_data(chunksPerCS * 3);

        if (idxInTemp)
            checkCuda(cudaMemcpy(cs_data.data(), context.d_temp_langCache_data + (idx - context.lastIdx) * chunksPerCS, sizeof(uint64_t) * chunksPerCS, cudaMemcpyDeviceToHost));
        else
            checkCuda(cudaMemcpy(cs_data.data(), context.d_langCache_data + idx * chunksPerCS, sizeof(uint64_t) * chunksPerCS, cudaMemcpyDeviceToHost));
        checkCuda(cudaMemcpy(cs_data.data() + 1 * chunksPerCS, context.d_langCache_data + *LIdx * chunksPerCS, sizeof(uint64_t) * chunksPerCS, cudaMemcpyDeviceToHost));
        checkCuda(cudaMemcpy(cs_data.data() + 2 * chunksPerCS, context.d_langCache_data + *RIdx * chunksPerCS, sizeof(uint64_t) * chunksPerCS, cudaMemcpyDeviceToHost));

        res.push_back({ partitioner.indexToLevel(idx).second, cs_data });
    }

    int i = 0;
    while (resIndices[i] != -1 && i + 2 < 600) {
        std::vector<uint64_t> cs_data(chunksPerCS * 3);
        checkCuda(cudaMemcpy(cs_data.data(), context.d_langCache_data + resIndices[i] * chunksPerCS, sizeof(uint64_t) * chunksPerCS, cudaMemcpyDeviceToHost));
        checkCuda(cudaMemcpy(cs_data.data() + 1 * chunksPerCS, context.d_langCache_data + resIndices[i + 1] * chunksPerCS, sizeof(uint64_t) * chunksPerCS, cudaMemcpyDeviceToHost));
        checkCuda(cudaMemcpy(cs_data.data() + 2 * chunksPerCS, context.d_langCache_data + resIndices[i + 2] * chunksPerCS, sizeof(uint64_t) * chunksPerCS, cudaMemcpyDeviceToHost));
        res.push_back({ partitioner.indexToLevel(resIndices[i]).second, cs_data });
        i += 3;
    }

    cudaFree(d_resIndices);

    return res;
}
#endif

// Bringing the left and right indices of the RE from device to host
std::string constructDownward(const Context& context, const rei::LanguageSystem& languageSystem, const rei::LevelPartitioner& partitioner, int idx, bool idxInTemp = true)
{
    auto* LIdx = new int[1];
    auto* RIdx = new int[1];

    if (idxInTemp)
    {
        checkCuda(cudaMemcpy(LIdx, context.d_temp_leftIdx + idx, sizeof(int), cudaMemcpyDeviceToHost));
        checkCuda(cudaMemcpy(RIdx, context.d_temp_rightIdx + idx, sizeof(int), cudaMemcpyDeviceToHost));
        idx += context.lastIdx;
    }
    else
    {
        checkCuda(cudaMemcpy(LIdx, context.d_leftIdx + idx, sizeof(int), cudaMemcpyDeviceToHost));
        checkCuda(cudaMemcpy(RIdx, context.d_rightIdx + idx, sizeof(int), cudaMemcpyDeviceToHost));
    }

    auto alphabetSize = languageSystem.getAlphabetSize();

    int* d_resIndices;
    checkCuda(cudaMalloc(&d_resIndices, 600 * sizeof(int)));

    thrust::device_ptr<int> d_resIndices_ptr(d_resIndices);
    thrust::fill(d_resIndices_ptr, d_resIndices_ptr + 600, -1);

    if (*LIdx >= alphabetSize)
        generateResIndices<<<1, 1 >>>(*LIdx, alphabetSize, context.d_leftIdx, context.d_rightIdx, d_resIndices);
    if (*RIdx >= alphabetSize)
        generateResIndices<<<1, 1 >>>(*RIdx, alphabetSize, context.d_leftIdx, context.d_rightIdx, d_resIndices);

    int resIndices[600];
    checkCuda(cudaMemcpy(resIndices, d_resIndices, 600 * sizeof(int), cudaMemcpyDeviceToHost));

    std::map<int, std::pair<int, int>> indicesMap;

    indicesMap.insert(std::make_pair(idx, std::make_pair(*LIdx, *RIdx)));

    int i = 0;
    while (resIndices[i] != -1 && i + 2 < 600) {
        int re = resIndices[i];
        int l = resIndices[i + 1];
        int r = resIndices[i + 2];
        indicesMap.insert(std::make_pair(re, std::make_pair(l, r)));
        i += 3;
    }

    if (i + 2 >= 600) return "Size of the output is too big";

    cudaFree(d_resIndices);

    std::vector<char> alphabet(alphabetSize);
    for (int i = 0; i < alphabetSize; i++)
        alphabet[i] = languageSystem.getIC()[i + 1][0];

    return constructDownward(idx, indicesMap, alphabet, partitioner);
}

static Context* context;

__global__ void QuestionMark(rei::Pair<int> interval, Context::View context)
{
    const int tid = blockDim.x * blockIdx.x + threadIdx.x;

    uint64_t data[UINT64_PER_THREAD];
    CS cs(&data[0], context.d_langCache.getNumChuncksPerElement());

    if (tid < (interval.right - interval.left)) {

        cs.copy(context.d_langCache[interval.left + tid]);

        rei::processQuestionDevice(cs);

        context.insert(cs, tid, interval.left + tid);
    }
}

__global__ void Star(rei::Pair<int> interval, rei::GuideTableDevice::View guideTable, int alphabetSize, Context::View context)
{
    const int tid = blockDim.x * blockIdx.x + threadIdx.x;

    uint64_t data[UINT64_PER_THREAD];
    CS cs(&data[0], context.d_langCache.getNumChuncksPerElement());

    if (tid < (interval.right - interval.left)) {

        cs.copy(context.d_langCache[(interval.left + tid)]);

        processStarDevice(guideTable, alphabetSize, cs);

        context.insert(cs, tid, interval.left + tid);
    }
}

__global__ void Concat(rei::Pair<int> lInterval, rei::Pair<int> rInterval, rei::GuideTableDevice::View guideTable, int alphabetSize, Context::View context)
{
    const int tid = blockDim.x * blockIdx.x + threadIdx.x;

    uint64_t data[UINT64_PER_THREAD];
    CS cs(&data[0], context.d_langCache.getNumChuncksPerElement());

    if (tid < (lInterval.right - lInterval.left) * (rInterval.right - rInterval.left)) {

        const auto group = warpcore::cg::tiled_partition<1>(warpcore::cg::this_thread_block());

        int ldx = lInterval.left + tid / (rInterval.right - rInterval.left);
        CS lCS = context.d_langCache[ldx];

        int rdx = rInterval.left + tid % (rInterval.right - rInterval.left);
        CS rCS = context.d_langCache[rdx];

        rei::processConcatenateDevice(guideTable, alphabetSize, lCS, rCS, cs.clear());
        context.insert(cs, tid * 2, ldx, rdx, group);

        rei::processConcatenateDevice(guideTable, alphabetSize, rCS, lCS, cs.clear());
        context.insert(cs, tid * 2 + 1, rdx, ldx, group);
    }
}

__global__ void Or(rei::Pair<int> lInterval, rei::Pair<int> rInterval, Context::View context)
{
    const int tid = blockDim.x * blockIdx.x + threadIdx.x;

    uint64_t data[UINT64_PER_THREAD];
    CS cs(&data[0], context.d_langCache.getNumChuncksPerElement());

    if (tid < (lInterval.right - lInterval.left) * (rInterval.right - rInterval.left)) {

        int ldx = lInterval.left + tid / (rInterval.right - rInterval.left);
        CS lCS = context.d_langCache[ldx];

        int rdx = rInterval.left + tid % (rInterval.right - rInterval.left);
        CS rCS = context.d_langCache[rdx];

        processOrDevice(lCS, rCS, cs.clear());

        context.insert(cs, tid, ldx, rdx);
    }
}

rei::BottomUpSearchDevice::BottomUpSearchDevice(const LanguageSystem& languageSystem, const InputParams& inputParams) :
	inputParams(inputParams), languageSystem(languageSystem), partitioner(inputParams.maxCost + 1), guideTable(languageSystem.getGuideTable())
{
	int alphaCost = inputParams.costFunc.alphaCost();

	costLevel = alphaCost + 1;
	shortageCost = -1;
	lastRound = false;

	partitioner.fillTo(alphaCost, 0);

    uint64_t available_memory = getFreeMemory() * 0.8; // 80% of the free memory
    auto [ langCacheCapacity, temp_langCacheCapacity] = Context::getCacheCapacity(available_memory, languageSystem.getIC().size());

    printf("Allocating device memory: %lf mb.\n", available_memory / ((double)1024 * 1024));
    printf("The max amount of CS that will be stored: %llu. Given the size of IC: %u\n", langCacheCapacity, languageSystem.getIC().size());

    context = new Context(languageSystem, inputParams, langCacheCapacity, temp_langCacheCapacity);

    partitioner.end(alphaCost, Operation::Concatenate) = context->lastIdx;
    partitioner.end(alphaCost, Operation::Or) = context->lastIdx;
}

EnumerationState rei::BottomUpSearchDevice::enumerateCostLevel(Result& res)
{
    if (costLevel > inputParams.maxCost) {
        res.message = "Max Cost has been reached!";
        return EnumerationState::End;
    }

    EnumerationState enumState = enumerateLevel(res);

    if (res.entries.size() == inputParams.n)
        return EnumerationState::Found;

    if (enumState == EnumerationState::End)
        res.message = "the search run's out of memory!";

    costLevel++;
    return enumState;
}

std::string rei::BottomUpSearchDevice::constructRE(int idx) const
{
    return constructDownward(*(context), languageSystem, partitioner, idx);
}

void add_results(const rei::LanguageSystem& languageSystem, const rei::LevelPartitioner& partitioner, 
    std::vector<int> foundedIds, Result& result) {
    for (auto id : foundedIds)
    {
        Solution entry;
        entry.RE = constructDownward(*(context), languageSystem, partitioner, id);
#ifdef CS_DECOMPOSETION
        entry.decomposetion = constructCSDecomposetion(*(context), languageSystem, partitioner, id);
#endif
        entry.allCSs = context->allREs;
        entry.uniqueCSs = context->lastIdx;
        result.push_back(entry);
    }
}

EnumerationState rei::BottomUpSearchDevice::enumerateLevel(Result& result)
{
    auto costs = inputParams.costFunc;
    bool useQuestionOverOr = costs.alphaCost() + costs.operationCost(Operation::Or) >= costs.operationCost(Operation::Question);

    // Once it uses a previous cost that is not fully stored, it should continue as the last round
    if (context->onTheFly) {
        int dif = costLevel - shortageCost;
        if (dif == costs.operationCost(Operation::Question) || dif == costs.operationCost(Operation::Star) ||
            dif == costs.alphaCost() + costs.operationCost(Operation::Concatenate) || dif == costs.alphaCost() + costs.operationCost(Operation::Or))
            lastRound = true;
    }

    std::vector<int> foundedIds;

    //Question Mark
    if (costLevel >= costs.alphaCost() + costs.operationCost(Operation::Question) && useQuestionOverOr) {

        // ignore results from (*) and (?)
        auto [start, end] = partitioner.Interval(costLevel - costs.operationCost(Operation::Question), static_cast<Operation>(2));

        for (int sub_start = start; sub_start <= end; sub_start += context->temp_cache_capacity)
        {
            auto sub_end = min(sub_start + context->temp_cache_capacity - 1, end);
            int batch_size = sub_end - sub_start;

            if (batch_size == 0) break;

            LOG_OP((*context), costLevel, to_string(Operation::Question), batch_size);

            int threadBlocks = (batch_size + THREAD_COUNT - 1) / THREAD_COUNT;
            QuestionMark<<<threadBlocks, THREAD_COUNT>>>({ sub_start, sub_end }, context->getView());

            checkCuda(cudaGetLastError());

            context->syncAndCheck(batch_size, foundedIds);
            add_results(languageSystem, partitioner, foundedIds, result);
            foundedIds.clear();
            if(result.entries.size() == inputParams.n)
                return EnumerationState::Found;
        }
    }
    partitioner.end(costLevel, Operation::Question) = context->lastIdx;

    // Star
    if (costLevel >= costs.alphaCost() + costs.operationCost(Operation::Star)) {
        // ignore results from (*) and (?)
        auto [start, end] = partitioner.Interval(costLevel - costs.operationCost(Operation::Star), static_cast<Operation>(2));
        for (int sub_start = start; sub_start <= end; sub_start += context->temp_cache_capacity)
        {
            auto sub_end = min(sub_start + context->temp_cache_capacity - 1, end);
            int batch_size = sub_end - sub_start;

            if (batch_size == 0) break;

            LOG_OP((*context), costLevel, to_string(Operation::Star), batch_size);

            int threadBlocks = (batch_size + THREAD_COUNT - 1) / THREAD_COUNT;
            Star<<<threadBlocks, THREAD_COUNT>>>({ sub_start, sub_end }, guideTable.getView(), languageSystem.getAlphabetSize(),  context->getView());

            checkCuda(cudaGetLastError());

            context->syncAndCheck(batch_size, foundedIds);
            add_results(languageSystem, partitioner, foundedIds, result);
            foundedIds.clear();
            if (result.entries.size() == inputParams.n)
                return EnumerationState::Found;
        }
    }
    partitioner.end(costLevel, Operation::Star) = context->lastIdx;

    //Concatenate
    for (int i = costs.alphaCost(); 2 * i <= costLevel - costs.operationCost(Operation::Concatenate); ++i) {

        auto [lstart, lend] = partitioner.Interval(i);
        auto [rstart, rend] = partitioner.Interval(costLevel - i - costs.operationCost(Operation::Concatenate));

        auto seg_length = context->temp_cache_capacity / (2 * (lend - lstart));

        for (int sub_start = rstart; sub_start <= rend; sub_start += seg_length)
        {
            auto sub_end = min(sub_start + seg_length, rend);
            auto batch_size = (sub_end - sub_start) * (lend - lstart);

            if (batch_size == 0) break;

            LOG_OP((*context), costLevel, to_string(Operation::Concatenate), batch_size * 2);

            int threadBlocks = (batch_size + THREAD_COUNT - 1) / THREAD_COUNT;

            Concat<<<threadBlocks, THREAD_COUNT>>>({ sub_start, sub_end }, {lstart, lend}, guideTable.getView(), languageSystem.getAlphabetSize(), context->getView());

            checkCuda(cudaGetLastError());

            context->syncAndCheck(batch_size * 2, foundedIds);
            add_results(languageSystem, partitioner, foundedIds, result);
            foundedIds.clear();
            if (result.entries.size() == inputParams.n)
                return EnumerationState::Found;
        }
    }
    partitioner.end(costLevel, Operation::Concatenate) = context->lastIdx;

    //Union
    if (!useQuestionOverOr && costLevel >= 2 * costs.alphaCost() + costs.operationCost(Operation::Or)) {

        auto [start, end] = partitioner.Interval(costLevel - costs.alphaCost() - costs.operationCost(Operation::Or));

        for (int sub_start = start; sub_start <= end; sub_start += context->temp_cache_capacity)
        {
            auto sub_end = min(sub_start + context->temp_cache_capacity - 1, end);
            int batch_size = sub_end - sub_start;

            if (batch_size == 0) break;

            LOG_OP((*context), costLevel, to_string(Operation::Or), batch_size);

            int threadBlocks = (batch_size + THREAD_COUNT - 1) / THREAD_COUNT;
            QuestionMark<<<threadBlocks, THREAD_COUNT >>>({ sub_start, sub_end }, context->getView());

            checkCuda(cudaGetLastError());

            context->syncAndCheck(batch_size, foundedIds);
            add_results(languageSystem, partitioner, foundedIds, result);
            foundedIds.clear();
            if (result.entries.size() == inputParams.n)
                return EnumerationState::Found;
        }
    }
    for (int i = costs.alphaCost(); 2 * i <= costLevel - costs.operationCost(Operation::Or); ++i) {

        auto [lstart, lend] = partitioner.Interval(i);
        auto [rstart, rend] = partitioner.Interval(costLevel - i - costs.operationCost(Operation::Or));

        auto seg_length = context->temp_cache_capacity / (lend - lstart);

        for (int sub_start = rstart; sub_start <= rend; sub_start += seg_length)
        {
            auto sub_end = min(sub_start + seg_length, rend);
            auto batch_size = (sub_end - sub_start) * (lend - lstart);

            if (batch_size == 0) break;

            LOG_OP((*context), costLevel, to_string(Operation::Or), batch_size);

            int threadBlocks = (batch_size + THREAD_COUNT - 1) / THREAD_COUNT;
            Or<<<threadBlocks, THREAD_COUNT>>>({ sub_start, sub_end }, { lstart, lend }, context->getView());

            checkCuda(cudaGetLastError());

            context->syncAndCheck(batch_size, foundedIds);
            add_results(languageSystem, partitioner, foundedIds, result);
            foundedIds.clear();
            if (result.entries.size() == inputParams.n)
                return EnumerationState::Found;
        }
    }
    partitioner.end(costLevel, Operation::Or) = context->lastIdx;

    if (lastRound) return EnumerationState::End;
    if (context->onTheFly && shortageCost == (unsigned short)-1)  shortageCost = costLevel + EXCEED_SHORTAGE_COST;

    return EnumerationState::NotFound;
}