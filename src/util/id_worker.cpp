#include "util/id_worker.hpp"

#include <chrono>
#include <stdexcept>
#include <thread>

namespace CIM::util {

IdWorker::IdWorker() : worker_id_(0), last_ts_(0), sequence_(0) {}

IdWorker& IdWorker::GetInstance() {
    static IdWorker inst;
    return inst;
}

void IdWorker::Init(uint16_t worker_id) {
    if (worker_id > kMaxWorkerId) {
        throw std::out_of_range("worker_id out of range");
    }
    std::lock_guard<std::mutex> lk(mutex_);
    worker_id_ = worker_id;
}

static uint64_t NowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

uint64_t IdWorker::NextId() {
    std::lock_guard<std::mutex> lk(mutex_);

    uint64_t ts = NowMs();
    if (ts < last_ts_) {
        // 时钟回退：等待直到时间追上 last_ts_
        // 为了简单处理，这里做短暂自旋等待；生产环境可替换为更稳健策略
        uint64_t retry_until = last_ts_;
        while (ts < retry_until) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            ts = NowMs();
        }
    }

    uint64_t timestamp = ts - kEpoch;
    if (timestamp > ((1ULL << 41) - 1)) {
        throw std::runtime_error("timestamp overflow");
    }

    if (ts == last_ts_) {
        sequence_ = (sequence_ + 1) & kMaxSequence;
        if (sequence_ == 0) {
            // 本毫秒序列用尽，等待下一毫秒
            do {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                ts = NowMs();
            } while (ts <= last_ts_);
            timestamp = ts - kEpoch;
            last_ts_ = ts;
            sequence_ = 0;
        }
    } else {
        sequence_ = 0;
        last_ts_ = ts;
    }

    uint64_t id = (timestamp << (kWorkerIdBits + kSequenceBits)) |
                  ((static_cast<uint64_t>(worker_id_) & kMaxWorkerId) << kSequenceBits) |
                  (sequence_ & kMaxSequence);
    return id;
}

}  // namespace CIM::util
