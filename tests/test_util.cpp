#include "base/macro.hpp"
#include "util.hpp"
#include "base/macro.hpp"
#include <assert.h>

auto g_logger = CIM_LOG_ROOT();

void test_assert()
{
    CIM_LOG_ERROR(g_logger) << CIM::BacktraceToString(100);
    CIM_ASSERT(false);
}

int main(int argc, char **argv)
{
    test_assert();
    return 0;
}