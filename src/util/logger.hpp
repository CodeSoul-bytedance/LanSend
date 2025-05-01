#pragma once
#include <string>

class Logger {
public:
    static Logger& get_instance();

    // ������·����Խ�� "info" ��Աȱʧ������
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);

private:
    Logger() = default;
};
