//
// Created by Shawn Zhao on 2023/7/14.
//

#ifndef LIGHT_THREAD_POOL_LEARNING_FUNCTIONAL_H
#define LIGHT_THREAD_POOL_LEARNING_FUNCTIONAL_H

#include <type_traits>
#include <memory>

namespace arrow {
namespace internal {

/// A type erased callable object which may only be invoked once.
/// It can be constructed from any lambda which matches the provided call signature.
/// Invoking it results in destruction of the lambda, freeing any state/references
/// immediately. Invoking a default constructed FnOnce or one which has already been
/// invoked will segfault.

template<typename Signature>
class FnOnce;
// ...A是可变的模版参数包 ...表示参数包展开
template<typename R, typename... A>
class FnOnce<R(A...)> {
    //并且使用了部分特化，其中特化的模板参数为 R(A...)，表示匹配一个返回类型为 R，参数列表为 A... 的函数类型。
public:
    FnOnce() = default;

    template <typename Fn,
            typename = typename std::enable_if<std::is_convertible<
                    decltype(std::declval<Fn&&>()(std::declval<A>()...)), R>::value>::type>
    FnOnce(Fn fn) : impl_(new FnImpl<Fn>(std::move(fn))) { // NOLINT runtime/explicit
    }
    explicit operator bool() const { return impl_ != nullptr; }

    R operator()(A... a) && { // 执行一次实际操作
        auto bye = std::move(impl_); // 直接改变对于所属内存的控制权限
        return bye->invoke(std::forward<A&&>(a)...);
        //// ... 表示参数包展开
    }

private:
    struct Impl {
        virtual ~Impl() = default;
        virtual R invoke(A&&... a) = 0;
        };

    template<typename Fn>
    struct FnImpl : Impl {
        explicit FnImpl(Fn fn) : fn_(std::move(fn)) {}
        R invoke(A&&... a) override { return std::move(fn_)(std::forward<A&&>(a)...); }
        // std::move(fn_) 传递右值
        //// override 覆盖函数，覆盖虚基类的定义
        Fn fn_; // 左值fn_
    };
    std::unique_ptr<Impl> impl_; // 指针
};

} // namespace internal
} // namespace arrow

#endif //LIGHT_THREAD_POOL_LEARNING_FUNCTIONAL_H
