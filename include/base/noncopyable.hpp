#pragma once

namespace CIM {
class Noncopyable {
   public:
    Noncopyable(const Noncopyable&) = delete;
    void operator=(const Noncopyable&) = delete;

   protected:
    Noncopyable() = default;
    ~Noncopyable() = default;
};
}  // namespace CIM