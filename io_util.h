//
// Created by Shawn Zhao on 2023/7/14.
//

#ifndef LIGHT_THREAD_POOL_LEARNING_IO_UTIL_H
#define LIGHT_THREAD_POOL_LEARNING_IO_UTIL_H

#include <iostream>
#include <optional>

#include "status.h"
#include "visibility.h"

//使用ARROW_EXPORT来指定函数的导出/可见性
ARROW_EXPORT
std::optional<std::string> GetEnvVar(const char* name);
ARROW_EXPORT
Status SetEnvVar(const char* name, const char* value);
ARROW_EXPORT
Status SetEnvVar(const std::string& name, const std::string& value);
#endif //LIGHT_THREAD_POOL_LEARNING_IO_UTIL_H
