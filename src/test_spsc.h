
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <cstring>
#include <atomic>
#include <new>


struct alignas(64) SPSCBlock
{
    std::atomic<size_t> m_version{0};
    size_t m_size;
    alignas(64) std::byte m_data[64]{};
};

class SPSCQueue
{
    std::unique_ptr<SPSCBlock[]> m_pBlocks;
    alignas(64) size_t m_readerIdx {0};
    alignas(64) size_t m_writerIdx {0};
    size_t m_size;
public:
    explicit SPSCQueue(size_t size = 1024)
    {
        m_pBlocks = std::make_unique<SPSCBlock[]>(size);
        m_size = size;
    }

    bool Write(size_t size, std::byte * pData){

		SPSCBlock & currBlock = m_pBlocks[m_writerIdx];
		size_t version = currBlock.m_version.load(std::memory_order_acquire);

		//check if curr block version is 1 that means reader has not finished reading this block so return.
		if(version == 1)
		{
            //cant perform write on this block
			return false;
		}
		//increment the writ index
		m_writerIdx = (m_writerIdx+1) & (m_size -1 );

		//start writting data
		memcpy(currBlock.m_data, pData, size);
		currBlock.m_size = size;

		//set this block version to 1 so that reader can start reading it
		currBlock.m_version.store(1, std::memory_order_release);
		return true;
    }

    bool read(std::byte* data, size_t & size){

        SPSCBlock &block = m_pBlocks[m_readerIdx];

        size_t version = block.m_version.load(std::memory_order_acquire);
        // Can only read if the version is 1
        if (version == 1){
            //inc the reader index.
			m_readerIdx = (m_readerIdx + 1) & (m_size-1);

			std::memcpy(data, block.m_data, block.m_size);
            size = block.m_size;
			block.m_version.store(0, std::memory_order_release);
			return true;
        }
        return false;
    }

};
