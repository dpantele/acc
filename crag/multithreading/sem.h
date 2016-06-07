// This file is based on https://github.com/preshing/cpp11-on-multicore/blob/master/common/sema.h
// Added comments, namespaces and signalAll() method
// It was shared by https://github.com/preshing
// under https://github.com/preshing/cpp11-on-multicore/blob/master/LICENSE

#ifndef __CPP11OM_SEMAPHORE_H__
#define __CPP11OM_SEMAPHORE_H__

#include <atomic>
#include <cassert>

#if defined(_WIN32)
#include <windows.h>
#undef min
#undef max
#elif defined(__MACH__)
#include <mach/mach.h>
#elif defined(__unix__)
#include <semaphore.h>
#else

#error Unsupported platform!

#endif

namespace crag {
namespace multithreading {

#if defined(_WIN32)
//---------------------------------------------------------
// Semaphore (Windows)
//---------------------------------------------------------
class Semaphore
{
private:
    HANDLE m_hSema;

    Semaphore(const Semaphore& other) = delete;
    Semaphore& operator=(const Semaphore& other) = delete;

public:
    Semaphore(int initialCount = 0)
    {
        assert(initialCount >= 0);
        m_hSema = CreateSemaphore(NULL, initialCount, MAXLONG, NULL);
    }

    ~Semaphore()
    {
        CloseHandle(m_hSema);
    }

    void wait()
    {
        WaitForSingleObject(m_hSema, INFINITE);
    }

    void signal(int count = 1)
    {
        ReleaseSemaphore(m_hSema, count, NULL);
    }
};


#elif defined(__MACH__)
//---------------------------------------------------------
// Semaphore (Apple iOS and OSX)
// Can't use POSIX semaphores due to http://lists.apple.com/archives/darwin-kernel/2009/Apr/msg00010.html
//---------------------------------------------------------

class Semaphore
{
private:
    semaphore_t m_sema;

    Semaphore(const Semaphore& other) = delete;
    Semaphore& operator=(const Semaphore& other) = delete;

public:
    Semaphore(int initialCount = 0)
    {
        assert(initialCount >= 0);
        semaphore_create(mach_task_self(), &m_sema, SYNC_POLICY_FIFO, initialCount);
    }

    ~Semaphore()
    {
        semaphore_destroy(mach_task_self(), m_sema);
    }

    void wait()
    {
        semaphore_wait(m_sema);
    }

    void signal()
    {
        semaphore_signal(m_sema);
    }

    void signal(int count)
    {
        while (count-- > 0)
        {
            semaphore_signal(m_sema);
        }
    }
};


#elif defined(__unix__)
//---------------------------------------------------------
// Semaphore (POSIX, Linux)
//---------------------------------------------------------

class Semaphore
{
private:
    sem_t m_sema;

    Semaphore(const Semaphore& other) = delete;
    Semaphore& operator=(const Semaphore& other) = delete;

public:
    Semaphore(int initialCount = 0)
    {
        assert(initialCount >= 0);
        sem_init(&m_sema, 0, initialCount);
    }

    ~Semaphore()
    {
        sem_destroy(&m_sema);
    }

    void wait()
    {
        // http://stackoverflow.com/questions/2013181/gdb-causes-sem-wait-to-fail-with-eintr-error
        int rc;
        do
        {
            rc = sem_wait(&m_sema);
        }
        while (rc == -1 && errno == EINTR);
    }

    void signal()
    {
        sem_post(&m_sema);
    }

    void signal(int count)
    {
        while (count-- > 0)
        {
            sem_post(&m_sema);
        }
    }
};


#else

#error Unsupported platform!

#endif


//---------------------------------------------------------
// LightweightSemaphore
//---------------------------------------------------------
class LightweightSemaphore
{
private:
    std::atomic<int> m_count;
    Semaphore m_sema; //this is used only when m_count becomes negative, and only until m_sema is negative

    void waitWithPartialSpinning()
    {
        int oldCount;
        // Is there a better way to set the initial spin count?
        // If we lower it to 1000, testBenaphore becomes 15x slower on my Core i7-5930K Windows PC,
        // as threads start hitting the kernel semaphore.
        int spin = 10000;
        while (spin--)
        {
            oldCount = m_count.load(std::memory_order_relaxed);
            if ((oldCount > 0) && m_count.compare_exchange_strong(oldCount, oldCount - 1, std::memory_order_acquire)) {
                return;
            }
            std::atomic_signal_fence(std::memory_order_acquire);     // Prevent the compiler from collapsing the loop.
        }
        oldCount = m_count.fetch_sub(1, std::memory_order_acquire);
        if (oldCount <= 0)
        {
            m_sema.wait();
        }
    }

public:
    LightweightSemaphore(int initialCount = 0) : m_count(initialCount)
    {
        assert(initialCount >= 0);
    }

    bool tryWait()
    {
        int oldCount = m_count.load(std::memory_order_relaxed);
        return (oldCount > 0 && m_count.compare_exchange_strong(oldCount, oldCount - 1, std::memory_order_acquire));
    }

    void wait()
    {
        if (!tryWait())
            waitWithPartialSpinning();
    }

    void signal(int count = 1)
    {
        int oldCount = m_count.fetch_add(count, std::memory_order_release);
        int toRelease = -oldCount < count ? -oldCount : count;
        if (toRelease > 0)
        {
            // here up to count signals are unblocked, and count - oldCount are left for a fast-spin
            m_sema.signal(toRelease);
        }
    }

    int approximateCount() const {
        return m_count.load(std::memory_order_relaxed);
    }

    // makes sure that all sleeping threads are waked up after this call, and semaphore had value at least n
    void signalAndWakeAll(int n = 0) {
        int oldCount = m_count.load(std::memory_order_relaxed);
        for (;;) {
            int desired = n;
            if (oldCount > n) {
                desired = oldCount;
            }
            if (m_count.compare_exchange_weak(oldCount, desired, std::memory_order_release, std::memory_order_relaxed)) {
                break;
            }
        }
        if (oldCount < 0) {
            m_sema.signal(-oldCount);
        }
    }

    bool hasWaiters() const {
        return m_count.load(std::memory_order_relaxed) < 0;
    }

};

typedef LightweightSemaphore DefaultSemaphoreType;

}} //crag::threading




#endif // __CPP11OM_SEMAPHORE_H__
