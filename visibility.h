//
// Created by Shawn Zhao on 2023/7/14.
//

#ifndef LIGHT_THREAD_POOL_LEARNING_VISIBILITY_H
#define LIGHT_THREAD_POOL_LEARNING_VISIBILITY_H
#endif //LIGHT_THREAD_POOL_LEARNING_VISIBILITY_H

// 定义了 ARROW_EXPORT 宏，同时指定宏的可见性，将这个宏定义写在里面也是合法的
#ifndef ARROW_EXPORT
#define ARROW_EXPORT __attribute__((visibility("default")))
#endif
