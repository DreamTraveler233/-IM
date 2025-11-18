#ifndef __CIM_UTIL_ID_WORKER_HPP__
#define __CIM_UTIL_ID_WORKER_HPP__

#include <atomic>
#include <cstdint>
#include <mutex>

namespace CIM::util {

// 轻量雪花ID生成器 (Twitter Snowflake 风格)
// 注意：当前项目已改为使用数据库自增 ID，雪花ID生成器保留为可选/兼容实现。
// 64-bit 格式（从高位到低位）：
// 1 bit sign(0) | 41 bit timestamp(ms since epoch) | 10 bit worker id | 12 bit sequence
// 可通过 IdWorker::Init(worker_id) 初始化 worker id，随后调用 NextId() 获取唯一 ID（如需恢复分布式ID，请在启动时初始化）。

class IdWorker {
   public:
    // 获取单例实例
    static IdWorker& GetInstance();

    // 初始化 worker id（0 ~ max_worker_id），可以多次调用但应在生成ID之前完成
    void Init(uint16_t worker_id);

    // 生成下一个 ID（线程安全）
    uint64_t NextId();

    // 当前 worker id
    uint16_t GetWorkerId() const { return worker_id_; }

   private:
    IdWorker();
    ~IdWorker() = default;

    // 禁止拷贝
    IdWorker(const IdWorker&) = delete;
    IdWorker& operator=(const IdWorker&) = delete;

    uint16_t worker_id_;
    std::mutex mutex_;
    uint64_t last_ts_;   // 上次时间戳（ms）
    uint64_t sequence_;  // 序列号

    static const uint64_t kEpoch = 1577836800000ULL;  // 2020-01-01 00:00:00 UTC in ms
    static const int kWorkerIdBits = 10;
    static const int kSequenceBits = 12;
    static const uint64_t kMaxWorkerId = (1ULL << kWorkerIdBits) - 1;
    static const uint64_t kMaxSequence = (1ULL << kSequenceBits) - 1;
};

}  // namespace CIM::util

#endif  // __CIM_UTIL_ID_WORKER_HPP__
