//
// Created by Shawn Zhao on 2023/7/14.
//

#ifndef LIGHT_THREAD_POOL_LEARNING_MACROS_H
#define LIGHT_THREAD_POOL_LEARNING_MACROS_H

// 断言语句可以使用Google test
#define ARROW_UNUSED(x) (void) (x)

#define ARROW_PREDICT_FALSE(x) (__builtin_expect(!!(x), 0))

// 如果使用google test EXPECT_EQ 断言失败时会继续进行代码，不会停止测试
// 但是可以尝试使用Assert系列的断言
#define DCHECK_GE(val1, val2) \
  do { \
    if (!((val1) >= (val2))) { \
      std::abort(); \
    } \
  } while (false)

#define DCHECK_EQ(val1, val2) \
  do { \
    if (!((val1) == (val2))) { \
      std::abort(); \
    } \
  } while (false)

#define DCHECK_OK(val) \
  do { \
    if (!((val).ok())) { \
      std::abort(); \
    } \
  } while (false)

#define DCHECK_NOT_OK(val) \
  do { \
    if (((val).ok())) { \
      std::abort(); \
    } \
  } while (false)


#endif //LIGHT_THREAD_POOL_LEARNING_MACROS_H
