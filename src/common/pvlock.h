#pragma once

#include "assertion.h"
#include <unistd.h>
#include <sys/eventfd.h>

class pvlock
{
public:
    pvlock(unsigned int init_value = 0)
    {
        _eventfd = CCALL(eventfd(init_value, EFD_CLOEXEC | EFD_SEMAPHORE));
    }

    void V(uint64_t add = 1)
    {
        CCALL(write(_eventfd, &add, sizeof(add)));
    }

    void P()
    {
        uint64_t dummy;
        CCALL(read(_eventfd, &dummy, sizeof(dummy)));
        ASSERT(dummy == 1);
    }

    ~pvlock()
    {
        CCALL(close(_eventfd));
        _eventfd = -1;
    }
    
private:
    int _eventfd;
};

