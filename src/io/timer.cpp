#include "io/timer.hpp"

#include "base/macro.hpp"
#include "util/time_util.hpp"

namespace CIM {
Timer::Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager* manager)
    : m_recurring(recurring),
      m_ms(ms),
      m_next(TimeUtil::NowToMS() + m_ms),
      m_cb(cb),
      m_manager(manager) {}

Timer::Timer(uint64_t next) : m_next(next) {}

bool Timer::cancel() {
    TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
    if (m_cb) {
        // 取消回调函数
        m_cb = nullptr;
        // 将定时器从定时器队列中删除
        auto it = m_manager->m_timers.find(shared_from_this());
        if (it != m_manager->m_timers.end()) {
            m_manager->m_timers.erase(it);
            return true;
        }
    }
    return false;
}

bool Timer::refresh() {
    TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
    if (m_cb) {
        auto it = m_manager->m_timers.find(shared_from_this());
        if (it != m_manager->m_timers.end()) {
            m_manager->m_timers.erase(it);
            m_next = TimeUtil::NowToMS() + m_ms;
            m_manager->m_timers.insert(shared_from_this());
            return true;
        }
    }
    return false;
}

bool Timer::reset(uint64_t ms, bool from_now) {
    // 如果新的间隔时间与当前间隔时间相同，并且不需要从现在开始计算，则无需重置
    if (ms == m_ms && !from_now) {
        return true;
    }
    TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
    // 检查定时器回调函数是否存在
    if (!m_cb) {
        return false;
    }
    auto it = m_manager->m_timers.find(shared_from_this());
    // 查找定时器是否在管理器中存在
    if (it == m_manager->m_timers.end()) {
        return false;
    }
    // 从定时器管理器中移除该定时器
    m_manager->m_timers.erase(it);
    uint64_t start = 0;
    if (from_now) {
        // 从当前时间开始计算
        start = TimeUtil::NowToMS();
    } else {
        // 从原定时间点开始计算
        start = m_next - m_ms;
    }
    m_ms = ms;
    m_next = start + ms;
    // 将更新后的定时器重新添加到管理器中
    m_manager->addTimer(shared_from_this(), lock);
    return true;
}

bool Timer::Comparator::operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const {
    // 处理空指针情况
    if (!lhs && !rhs) {
        return false;
    }
    if (!lhs) {
        return true;
    }
    if (!rhs) {
        return false;
    }

    // 按照下次执行时间进行比较
    if (lhs->m_next < rhs->m_next) {
        return true;
    }
    if (lhs->m_next > rhs->m_next) {
        return false;
    }

    // 如果下次执行时间相同，则按照对象地址比较
    return lhs.get() < rhs.get();
}

TimerManager::TimerManager() {
    m_previouseTime = TimeUtil::NowToMS();
}

Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> cb, bool recurring) {
    //CIM_ASSERT(ms && cb);
    Timer::ptr timer(new Timer(ms, cb, recurring, this));
    RWMutex::WriteLock lock(m_mutex);
    addTimer(timer, lock);
    return timer;
}

static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb) {
    // 如果对象还存在
    std::shared_ptr<void> tmp = weak_cond.lock();
    if (tmp) {
        cb();
    }
}

Timer::ptr TimerManager::addConditionTimer(uint64_t ms, std::function<void()> cb,
                                           std::weak_ptr<void> weak_cond, bool recurring) {
    return addTimer(ms, std::bind(&OnTimer, weak_cond, cb), recurring);
}

uint64_t TimerManager::getNextTimer() {
    RWMutex::ReadLock lock(m_mutex);
    m_tickled = false;
    if (m_timers.empty()) {
        return ~0ull;
    }
    // 获取最早执行的定时器
    const Timer::ptr& next = *m_timers.begin();
    uint64_t now = TimeUtil::NowToMS();

    if (now >= next->m_next) {
        // 立即处理
        return 0;
    } else {
        return next->m_next - now;
    }
}

void TimerManager::listExpiredCb(std::vector<std::function<void()>>& cbs) {
    uint64_t now_ms = TimeUtil::NowToMS();
    std::vector<Timer::ptr> expired;

    // 双重检查，减少获取写锁的次数，提高并发性能
    {
        RWMutex::ReadLock lock(m_mutex);
        if (m_timers.empty()) {
            return;
        }
    }

    // 获取写锁以修改定时器集合
    RWMutex::WriteLock lock(m_mutex);
    if (m_timers.empty()) {
        return;
    }

    // 检查是否有系统时钟回退或是否有到期的定时器
    bool rollover = detectClockRollover(now_ms);
    if (!rollover && ((*m_timers.begin())->m_next > now_ms)) {
        return;
    }

    // 查找所有已到期的定时器
    Timer::ptr now_timer(new Timer(now_ms));
    // 查找第一个未到期（或刚好到期）的定时器
    auto it = rollover ? m_timers.end() : m_timers.lower_bound(now_timer);
    // 处理多个执行时间相同的定时器
    while (it != m_timers.end() && (*it)->m_next == now_ms) {
        ++it;
    }

    // 将已到期的定时器移到expired向量中
    expired.insert(expired.begin(), m_timers.begin(), it);
    m_timers.erase(m_timers.begin(), it);

    cbs.reserve(expired.size());

    // 处理所有已到期的定时器
    for (auto& timer : expired) {
        cbs.push_back(timer->m_cb);
        if (timer->m_recurring) {
            // 对于重复执行的定时器，设置下次执行时间并重新插入队列
            timer->m_next = now_ms + timer->m_ms;
            m_timers.insert(timer);
        } else {
            // 对于一次性定时器，清空回调函数
            timer->m_cb = nullptr;
        }
    }
}

bool TimerManager::hasTimer() {
    RWMutex::ReadLock lock(m_mutex);
    return !m_timers.empty();
}

/*
        将锁作为参数的目的：减小锁颗粒度
    */
void TimerManager::addTimer(Timer::ptr val, RWMutexType::WriteLock& lock) {
    auto it = m_timers.insert(val).first;
    bool at_front = (it == m_timers.begin() && !m_tickled);
    if (at_front) {
        // 标记已通知上层
        m_tickled = true;
    }
    lock.unlock();

    // 通知IO管理器等组件有新的最早执行定时器
    if (at_front) {
        onTimerInsertedAtFront();
    }
}

bool TimerManager::detectClockRollover(uint64_t now_ms) {
    bool rollover = false;
    if (now_ms < m_previouseTime && now_ms < (m_previouseTime - 60 * 60 * 1000)) {
        rollover = true;
    }
    m_previouseTime = now_ms;
    return rollover;
}
}  // namespace CIM