#include "io/coroutine.hpp"

#include <atomic>

#include "config/config.hpp"
#include "io/scheduler.hpp"
#include "base/macro.hpp"
#include "util/util.hpp"

namespace CIM {
static auto g_logger = CIM_LOG_NAME("system");

static std::atomic<uint64_t> s_coroutine_id = {0};     // 协程id
static std::atomic<uint64_t> s_coroutine_count = {0};  // 全局协程计数器

// 当前协程的协程对象
static thread_local Coroutine* t_coroutine = nullptr;
// 当前线程的主协程
static thread_local Coroutine::ptr t_thread_coroutine = nullptr;

// 定义配置项--协程默认栈大小--1MB
static auto g_coroutine_stack_size =
    Config::Lookup<uint32_t>("coroutine.stack_size", 1024 * 1024, "coroutine stack size");

static uint32_t s_coroutine_stack_size = 0;

struct CoroutineInit {
    CoroutineInit() {
        s_coroutine_stack_size = g_coroutine_stack_size->getValue();
        g_coroutine_stack_size->addListener([](const uint32_t& ole_val, const uint32_t& new_val) {
            s_coroutine_stack_size = new_val;
        });
    }
};
static CoroutineInit __coroutine_init;

using StackAllocator = MallocStackAllocator;

Coroutine::Coroutine() : m_state(State::EXEC) {
    // 获取上下文，接管当前线程
    if (getcontext(&m_ctx)) {
        CIM_ASSERT2(false, "getcontext");
        return;  // 如果有适当的错误处理机制，应该在这里处理
    }

    // 设置线程局部变量
    SetThis(this);
    ++s_coroutine_count;
    CIM_LOG_DEBUG(g_logger) << "Coroutine::Coroutine() id=" << m_id;
}

Coroutine::Coroutine(std::function<void()> cb, size_t stack_size, bool use_caller)
    : m_id(++s_coroutine_id), m_cb(cb) {
    CIM_ASSERT(cb);
    ++s_coroutine_count;

    // 使用局部变量管理栈空间，确保异常安全性
    void* stack = nullptr;
    size_t stack_size_temp = stack_size ? stack_size : s_coroutine_stack_size;

    try {
        // 设置协程的栈空间
        stack = StackAllocator::Alloc(stack_size_temp);
        m_stack_size = stack_size_temp;
        m_stack = stack;

        // 初始化上下文
        m_ctx.uc_link = nullptr;                // 协程执行完成后返回的上下文
        m_ctx.uc_stack.ss_sp = m_stack;         // 栈空间地址
        m_ctx.uc_stack.ss_size = m_stack_size;  // 栈空间大小

        // 获取当前线程上下文环境
        if (getcontext(&m_ctx)) {
            CIM_ASSERT2(false, "getcontext");
        }

        // 设置协程的入口函数
        if (use_caller) {
            makecontext(&m_ctx, &CallerMainFunc, 0);
        } else {
            makecontext(&m_ctx, &MainFunc, 0);
        }
    } catch (...) {
        // 如果发生异常，释放已分配的栈空间
        if (stack) {
            StackAllocator::Dealloc(stack, stack_size_temp);
        }
        --s_coroutine_count;
        throw;  // 重新抛出异常
    }
    CIM_LOG_DEBUG(g_logger) << "Coroutine::Coroutine()" << " id=" << m_id;
}

Coroutine::~Coroutine() {
    CIM_LOG_DEBUG(g_logger) << "Coroutine::~Coroutine" << " id=" << m_id;
    if (m_stack)  // 说明为子协程
    {
        CIM_ASSERT(m_state == State::TERM || m_state == State::INIT || m_state == State::EXCEPT);
        StackAllocator::Dealloc(m_stack, m_stack_size);
    } else  // 说明为主协程
    {
        CIM_ASSERT(!m_cb);
        CIM_ASSERT(m_state == State::EXEC);

        // 将主协程指针置空
        Coroutine* cur = t_coroutine;
        if (cur == this) {
            SetThis(nullptr);
        }
    }
    --s_coroutine_count;
}

void Coroutine::reset(std::function<void()> cb) {
    CIM_ASSERT(m_stack);
    CIM_ASSERT(m_stack_size > 0);
    CIM_ASSERT(m_state == State::TERM || m_state == State::INIT || m_state == State::EXCEPT);
    m_cb = cb;
    if (getcontext(&m_ctx))  // 获取当前上下文环境
    {
        CIM_ASSERT2(false, "getcontext");
    }
    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stack_size;

    makecontext(&m_ctx, &MainFunc, 0);
    m_state = State::INIT;
}

void Coroutine::swapIn() {
    // 把当前运行协程设置为该子协程
    SetThis(this);
    CIM_ASSERT(m_state != State::EXEC && m_state != State::TERM && m_state != State::EXCEPT);
    m_state = State::EXEC;

    // 从主协程切换到当前线程（子协程）
    if (swapcontext(&Scheduler::GetMainCoroutine()->m_ctx, &m_ctx)) {
        CIM_ASSERT2(false, "swapcontext");
    }
}

void Coroutine::swapOut() {
    // 从当前线程（子协程）切换回主协程
    SetThis(Scheduler::GetMainCoroutine());
    if (swapcontext(&m_ctx, &Scheduler::GetMainCoroutine()->m_ctx)) {
        CIM_ASSERT2(false, "swapcontext");
    }
}

void Coroutine::call() {
    SetThis(this);
    CIM_ASSERT(m_state != State::EXEC && m_state != State::TERM && m_state != State::EXCEPT);
    m_state = State::EXEC;
    if (swapcontext(&t_thread_coroutine->m_ctx, &m_ctx)) {
        CIM_ASSERT2(false, "swapcontext");
    }
}

void Coroutine::back() {
    SetThis(t_thread_coroutine.get());
    if (swapcontext(&m_ctx, &t_thread_coroutine->m_ctx)) {
        CIM_ASSERT2(false, "swapcontext");
    }
}

uint64_t Coroutine::getId() const {
    return m_id;
}

Coroutine::State Coroutine::getState() const {
    return m_state;
}

void Coroutine::setState(State state) {
    m_state = state;
}

void Coroutine::SetThis(Coroutine* val) {
    t_coroutine = val;
}

Coroutine::ptr Coroutine::GetThis() {
    if (t_coroutine) {
        // 当前协程存在，直接返回
        return t_coroutine->shared_from_this();
    }

    // 当前协程为空，说明在当前线程还没有协程，则创建一个主协程
    Coroutine::ptr main_coroutine(new Coroutine);
    CIM_ASSERT(t_coroutine == main_coroutine.get());
    // 设置主协程
    t_thread_coroutine = main_coroutine;
    return t_coroutine->shared_from_this();
}

void Coroutine::YieldToReady() {
    Coroutine::ptr cur = GetThis();
    CIM_ASSERT(cur->m_state == EXEC);
    cur->m_state = State::READY;
    cur->swapOut();
}

void Coroutine::YieldToHold() {
    Coroutine::ptr cur = GetThis();
    CIM_ASSERT(cur->m_state == EXEC);
    cur->m_state = State::HOLD;
    cur->swapOut();
}

uint64_t Coroutine::TotalCoroutines() {
    return s_coroutine_count;
}

void Coroutine::MainFunc() {
    // 获取当前正在运行的协程
    Coroutine::ptr cur = GetThis();
    CIM_ASSERT(cur);

    try {
        cur->m_cb();  // 执行协程的回调函数
        cur->m_cb = nullptr;
        cur->m_state = State::TERM;
    } catch (std::exception& ex) {
        cur->m_state = State::EXCEPT;
        CIM_LOG_ERROR(g_logger) << "coroutine exception: " << ex.what()
                                << " coroutine id: " << cur->getId() << std::endl
                                << BacktraceToString();
    } catch (...) {
        cur->m_state = State::EXCEPT;
        CIM_LOG_ERROR(g_logger) << "Coroutine exception";
    }

    // 协程执行完毕后，需要将控制权交还给主协程
    auto p = cur.get();
    cur.reset();
    p->swapOut();

    CIM_ASSERT2(false, "never reach coroutine id=" + std::to_string(p->getId()));
}

void Coroutine::CallerMainFunc() {
    // 获取当前正在运行的协程
    Coroutine::ptr cur = GetThis();
    CIM_ASSERT(cur);

    try {
        cur->m_cb();  // 执行协程的回调函数
        cur->m_cb = nullptr;
        cur->m_state = State::TERM;
    } catch (std::exception& ex) {
        cur->m_state = State::EXCEPT;
        CIM_LOG_ERROR(g_logger) << "coroutine exception: " << ex.what()
                                << " coroutine id: " << cur->getId() << std::endl
                                << BacktraceToString();
    } catch (...) {
        cur->m_state = State::EXCEPT;
        CIM_LOG_ERROR(g_logger) << "Coroutine exception";
    }

    // 协程执行完毕后，需要将控制权交还给主协程
    auto p = cur.get();
    cur.reset();
    p->back();

    CIM_ASSERT2(false, "never reach coroutine id=" + std::to_string(p->getId()));
}

uint64_t Coroutine::GetCoroutineId() {
    return t_coroutine ? t_coroutine->m_id : 0;
}

void* MallocStackAllocator::Alloc(size_t size) {
    return malloc(size);
}

void MallocStackAllocator::Dealloc(void* ptr, size_t size) {
    free(ptr);
}
}  // namespace CIM