#include "base/macro.hpp"
#include "iomanager.hpp"
#include "config.hpp"

void test()
{
    static auto g_logger = CIM_LOG_NAME("system");

    YAML::Node test = YAML::LoadFile("/home/szy/code/CIM/bin/config/log.yaml");
    CIM::Config::LoadFromYaml(test);

    // CIM::FileLogAppender::ptr file_ap(new CIM::FileLogAppender("/home/szy/code/CIM/bin/log/test.log"));
    // file_ap->getLogFile()->setRotateType(CIM::RotateType::Minute);
    // g_logger->addAppender(file_ap);

    for (int i = 0; i < 1000000; ++i)
    {
        CIM_LOG_INFO(g_logger) << "第 " << i << " 条日志";
        //sleep(0.1);
    }

    // // 加载日志配置文件
    // YAML::Node root = YAML::LoadFile("/home/szy/code/CIM/bin/log/log.yaml");
    // CIM::Config::LoadFromYaml(root);
}

int main(int argc, char **argv)
{
    CIM::IOManager iom(2);
    iom.schedule(test);
    return 0;
}