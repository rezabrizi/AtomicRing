/**
 * Author: Reza A Tabrizi
 * Email: Rtabrizi03@gmail.com
 */


#pragma once

#include <cstdint>
#include <functional>
#include <atomic>
#include <memory>
#include <new>


using BlockVersion = uint32_t;
using MessageSize = uint32_t;
using WriteCallback = std::function<void(uint8_t* data)>;

struct Block
{
    // Local block versions reduce contention for the queue
    std::atomic<BlockVersion> mVersion{0};
    // Size of the data
    std::atomic<MessageSize> mSize{0};
    // 64 byte buffer
    alignas(std::hardware_destructive_interference_size) uint8_t mData[64]{};
};

struct Header
{
    // Block count
    alignas(std::hardware_destructive_interference_size) std::atomic<uint64_t> mBlockCounter {0};
};


class SPMC_Q {
public:
    SPMC_Q(size_t sz): mSz(sz), mBlocks( std::make_unique<Block[]>(sz)){}
    ~SPMC_Q()=default;

    void Write(MessageSize size, WriteCallback c){
        // the next block index to write to
        uint64_t blockIndex = mHeader.mBlockCounter.fetch_add(1, std::memory_order_acquire) % mSz;
        Block &block = mBlocks[blockIndex];

        BlockVersion currentVersion = block.mVersion.load(std::memory_order_acquire) + 1;
        // If the block has been written to before, it has an odd version
        // we need to make its version even before writing begins to indicate that writing is in progress
        if (block.mVersion.load(std::memory_order_acquire) % 2 == 1){
            // make the version even
            block.mVersion.store(currentVersion, std::memory_order_release);
            // store the newVersion used for after the writing is done
            currentVersion++;
        }
        // store the size
        block.mSize.store(size, std::memory_order_release);
        // perform write using the callback function
        c(block.mData);
        // store the new odd version
        block.mVersion.store(currentVersion, std::memory_order_release);
    }

    bool Read (uint64_t blockIndex, uint8_t* data, MessageSize& size) const {
        // Block
        Block &block = mBlocks[blockIndex];
        // Block version
        BlockVersion version = block.mVersion.load(std::memory_order_acquire);
        // Read when version is odd
        if(version % 2 == 1){
            // Size of the data
            size = block.mSize.load(std::memory_order_acquire);
            // Perform the read
            std::memcpy(data, block.mData, size);
            // Indicate that a read has occurred by adding a 2 to the version
            // However do not block subsequent reads
            block.mVersion.store(version + 2, std::memory_order_release);
            return true;
        }
        return false;
    }

    size_t size () const {
        return mSz;
    }

private:
    Header mHeader;
    size_t mSz;
    std::unique_ptr<Block[]> mBlocks;
};