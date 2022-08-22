#include <iostream>
#include "design_pattern.h"

struct MemBlock
{
    void *data;
    size_t size;

    static int i;
    MemBlock(){
        i++;
        std::cout << "MemBlock: " << i << std::endl;
    }
    ~MemBlock(){std::cout << "~MemBlock" << std::endl;}
    void reset(){std::cout << "MemBlock reset" << std::endl;};
};

int MemBlock::i = 0;

class Buffer : public MemBlock
{
  public:
    Buffer(){std::cout << "Buffer" << std::endl;}
    ~Buffer(){std::cout << "~Buffer" << std::endl;}
    void reset(){std::cout << "MemBUffer reset" << std::endl;};
};


int main_1(int argc, char** argv)
{
    const int pool_size = 1024;

    ObjectPool<Buffer> buffer_pool(pool_size);
    ObjectPool<MemBlock> mem_block_pool(pool_size);

    auto a = buffer_pool.get();
    a->reset();
    return 0;
}