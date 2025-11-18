#include "io/lock.hpp"

#include "base/macro.hpp"
#include "io/scheduler.hpp"

namespace CIM {
Mutex::Mutex() {
    pthread_mutex_init(&m_mutex, nullptr);
}
Mutex::~Mutex() {
    pthread_mutex_destroy(&m_mutex);
}
void Mutex::lock() {
    pthread_mutex_lock(&m_mutex);
}
void Mutex::unlock() {
    pthread_mutex_unlock(&m_mutex);
}

RWMutex::RWMutex() {
    pthread_rwlock_init(&m_mutex, nullptr);
}
RWMutex::~RWMutex() {
    pthread_rwlock_destroy(&m_mutex);
}
void RWMutex::rdlock() {
    pthread_rwlock_rdlock(&m_mutex);
}
void RWMutex::wrlock() {
    pthread_rwlock_wrlock(&m_mutex);
}
void RWMutex::unlock() {
    pthread_rwlock_unlock(&m_mutex);
}

SpinLock::SpinLock() {
    pthread_spin_init(&m_mutex, 0);
}
SpinLock::~SpinLock() {
    pthread_spin_destroy(&m_mutex);
}
void SpinLock::lock() {
    pthread_spin_lock(&m_mutex);
}
void SpinLock::unlock() {
    pthread_spin_unlock(&m_mutex);
}

CASLock::CASLock() {
    m_mutex.clear();
}
CASLock::~CASLock() {}
void CASLock::lock() {
    while (std::atomic_flag_test_and_set_explicit(&m_mutex, std::memory_order_acquire));
}
void CASLock::unlock() {
    std::atomic_flag_clear_explicit(&m_mutex, std::memory_order_release);
}

CoroutineSemaphore::CoroutineSemaphore(size_t initial_concurrency)
    : m_concurrency(initial_concurrency) {}

CoroutineSemaphore::~CoroutineSemaphore() {
    CIM_ASSERT(m_waiters.empty());
}

bool CoroutineSemaphore::tryWait() {
    CIM_ASSERT(Scheduler::GetThis());
    {
        MutexType::Lock lock(m_mutex);
        if (m_concurrency > 0u) {
            --m_concurrency;
            return true;
        }
        return false;
    }
}

void CoroutineSemaphore::wait() {
    CIM_ASSERT(Scheduler::GetThis());
    {
        MutexType::Lock lock(m_mutex);
        if (m_concurrency > 0u) {
            --m_concurrency;
            return;
        }
        m_waiters.push_back(std::make_pair(Scheduler::GetThis(), Coroutine::GetThis()));
    }
    Coroutine::YieldToHold();
}

void CoroutineSemaphore::notify() {
    MutexType::Lock lock(m_mutex);
    if (!m_waiters.empty()) {
        auto next = m_waiters.front();
        m_waiters.pop_front();
        next.first->schedule(next.second);
    } else {
        ++m_concurrency;
    }
}
}  // namespace CIM
