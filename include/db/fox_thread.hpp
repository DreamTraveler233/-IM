#ifndef __CIM_DB_FOX_THREAD_HPP__
#define __CIM_DB_FOX_THREAD_HPP__

#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>

#include <list>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "io/lock.hpp"
#include "base/singleton.hpp"

namespace CIM {
class FoxThread;
class IFoxThread {
   public:
    using ptr = std::shared_ptr<IFoxThread>;
    using callback = std::function<void()>;

    virtual ~IFoxThread() {};
    virtual bool dispatch(callback cb) = 0;
    virtual bool dispatch(uint32_t id, callback cb) = 0;
    virtual bool batchDispatch(const std::vector<callback>& cbs) = 0;
    virtual void broadcast(callback cb) = 0;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void join() = 0;
    virtual void dump(std::ostream& os) = 0;
    virtual uint64_t getTotal() = 0;
};

class FoxThread : public IFoxThread {
   public:
    using ptr = std::shared_ptr<FoxThread>;
    typedef IFoxThread::callback callback;
    typedef std::function<void(FoxThread*)> init_cb;
    FoxThread(const std::string& name = "", struct event_base* base = NULL);
    ~FoxThread();

    static FoxThread* GetThis();
    static const std::string& GetFoxThreadName();
    static void GetAllFoxThreadName(std::map<uint64_t, std::string>& names);

    void setThis();
    void unsetThis();

    void start();

    virtual bool dispatch(callback cb);
    virtual bool dispatch(uint32_t id, callback cb);
    virtual bool batchDispatch(const std::vector<callback>& cbs);
    virtual void broadcast(callback cb);

    void join();
    void stop();
    bool isStart() const { return m_start; }

    struct event_base* getBase() { return m_base; }
    std::thread::id getId() const;

    void* getData(const std::string& name);
    template <class T>
    T* getData(const std::string& name) {
        return (T*)getData(name);
    }
    void setData(const std::string& name, void* v);

    void setInitCb(init_cb v) { m_initCb = v; }

    void dump(std::ostream& os);
    virtual uint64_t getTotal() { return m_total; }

   private:
    void thread_cb();
    static void read_cb(evutil_socket_t sock, short which, void* args);

   private:
    evutil_socket_t m_read;
    evutil_socket_t m_write;
    struct event_base* m_base;
    struct event* m_event;
    std::thread* m_thread;
    RWMutex m_mutex;
    std::list<callback> m_callbacks;

    std::string m_name;
    init_cb m_initCb;

    std::map<std::string, void*> m_datas;

    bool m_working;
    bool m_start;
    uint64_t m_total;
};

class FoxThreadPool : public IFoxThread {
   public:
    using ptr = std::shared_ptr<FoxThreadPool>;
    using callback = IFoxThread::callback;

    FoxThreadPool(uint32_t size, const std::string& name = "", bool advance = false);
    ~FoxThreadPool();

    void start();
    void stop();
    void join();

    // 随机线程执行
    bool dispatch(callback cb);
    bool batchDispatch(const std::vector<callback>& cb);
    // 指定线程执行
    bool dispatch(uint32_t id, callback cb);

    FoxThread* getRandFoxThread();
    void setInitCb(FoxThread::init_cb v) { m_initCb = v; }

    void dump(std::ostream& os);

    void broadcast(callback cb);
    virtual uint64_t getTotal() { return m_total; }

   private:
    void releaseFoxThread(FoxThread* t);
    void check();

    void wrapcb(std::shared_ptr<FoxThread>, callback cb);

   private:
    uint32_t m_size;
    uint32_t m_cur;
    std::string m_name;
    bool m_advance;
    bool m_start;
    RWMutex m_mutex;
    std::list<callback> m_callbacks;
    std::vector<FoxThread*> m_threads;
    std::list<FoxThread*> m_freeFoxThreads;
    FoxThread::init_cb m_initCb;
    uint64_t m_total;
};

class FoxThreadManager {
   public:
    typedef IFoxThread::callback callback;
    void dispatch(const std::string& name, callback cb);
    void dispatch(const std::string& name, uint32_t id, callback cb);
    void batchDispatch(const std::string& name, const std::vector<callback>& cbs);
    void broadcast(const std::string& name, callback cb);

    void dumpFoxThreadStatus(std::ostream& os);

    void init();
    void start();
    void stop();

    IFoxThread::ptr get(const std::string& name);
    void add(const std::string& name, IFoxThread::ptr thr);

   private:
    std::map<std::string, IFoxThread::ptr> m_threads;
};

using FoxThreadMgr = Singleton<FoxThreadManager>;

}  // namespace CIM
#endif