#include "io/thread.hpp"

#include "base/macro.hpp"

namespace CIM {
// 当前线程的线程对象
static thread_local Thread* t_thread = nullptr;
// 当前线程的线程名称
static thread_local std::string t_thread_name = "unknown";

static auto g_logger = CIM_LOG_NAME("system");

Thread::Thread(std::function<void()> cb, const std::string& name)
    : m_id(-1), m_thread(-1), m_cb(cb), m_name(name) {
    /**
         * 这里将 this 传入线程函数的原因：
         *      1、线程函数Thread::run是静态函数，无法直接访问类的成员变量
         *      2、通过将this指针作为参数传入，线程函数可以获得指向创建它的Thread对象的引用
         *      3、在run函数内部，通过(Thread *)arg将参数转换回Thread*指针，从而可以访问和操作该线程对象的成员变量
         */
    /**
         * 为什么线程函数需要设置为静态函数：
         *      1、pthread_create需要一个符合void* (*)(void*)签名的函数指针，而非静态成员函数隐含了一个this指针参数，
         *      实际签名是void* (Class::*)(void*)，与要求不符
         *      2、void* (*)(void*)表示一个指向“返回值为void*”“函数参数为void*”的函数的指针，即函数指针，void*表示可以指向任意类型的数据
         */
    int rt = pthread_create(&m_thread, nullptr, &Thread::run, this);
    if (rt) {
        CIM_LOG_ERROR(g_logger) << "pthread_create fail, rt = " << rt << " name = " << name;
        throw std::logic_error("pthread_create error");
    }

    m_semaphore.wait();  // 等待线程创建成功
}

Thread::~Thread() {
    pthread_detach(m_thread);
}

void Thread::join() {
    int rt = pthread_join(m_thread, nullptr);
    if (rt) {
        CIM_LOG_ERROR(g_logger) << "pthread_join thread fail, rt = " << rt << " name = " << m_name;
        throw std::logic_error("pthread_join error");
    }
    m_thread = 0;
}

void Thread::SetName(const std::string& name) {
    if (t_thread) {
        t_thread->m_name = name;
    }
    t_thread_name = name;
}

void* Thread::run(void* arg) {
    Thread* thread = (Thread*)arg;

    // 设置局部线程变量
    t_thread = thread;
    t_thread_name = thread->m_name;

    // 获取并设置当前线程的真实系统线程ID
    thread->m_id = CIM::GetThreadId();
    // 设置线程的名称，限制在15个字符以内
    pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());

    std::function<void()> cb;
    // 使用swap而不是直接赋值是为了避免增加智能指针的引用计数，swap的作用类似移交所属权
    // 同时swap操作后thread->m_cb会被清空，避免回调函数被重复执行
    cb.swap(thread->m_cb);
    thread->m_semaphore.notify();  // 通知主线程线程创建成功
    cb();
    return 0;
}

Thread* Thread::GetThis() {
    return t_thread;
}

const std::string& Thread::GetName() {
    return t_thread_name;
}

pid_t Thread::getId() const {
    return m_id;
}

const std::string& Thread::getName() const {
    return m_name;
}
}  // namespace CIM