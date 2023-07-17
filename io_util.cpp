//
// Created by Shawn Zhao on 2023/7/14.
//

#include "io_util.h"

std::optional<std::string> GetEnvVar(const char* name) {
    char* c_str = getenv(name); // 获取环境变量
    if (c_str == nullptr) {
        return {};
    }
    return std::string(c_str);
}

Status SetEnvVar(const char* name, const char* value) {
    if (setenv(name, value, 1) == 0) { // overwrite = 0 不重写，为1重写
        return Status::OK();
    }
    else {
        return Status::Invalid("failed setting environment variable");
    }
}

Status SetEnvVar(const std::string& name, const std::string& value) {
    return SetEnvVar(name.c_str(), value.c_str());
}