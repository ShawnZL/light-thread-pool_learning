# 源码阅读📖

# todo

❌在`macro.h` 中定义了很多宏，是原先Apache Arrow当中留下的宏定义，其中的一些东西可以使用 `Google test` 进行替换，在`thread_pool.cpp` 中54-56行进行了测试，是可以进行替换的！

✅已经完成在cmake中加入google_test链接，直接`#include <gtest/gtest.h>`即可

# cancel

## cancel.h

```c++
class ARROW_EXPORT StopSource {
public:
    StopSource();
    ~StopSource();

    // Consumer API (the side that stops)
    void RequestStop();
    void RequestStop(Status error);
    void RequestStopFromSignal(int signum);

    StopToken token();

    // For internal use only
    void Reset();

protected:
    std::shared_ptr<StopSourceImpl> impl_; // shared_ptr 智能指针
};
```

这是一个名为 `StopSource` 的类的定义。它用于在多线程环境中请求停止操作。下面是该类的一些重要成员和功能：

- 构造函数 `StopSource()` 用于创建 `StopSource` 的实例。
- 析构函数 `~StopSource()` 用于销毁 `StopSource` 的实例。
- `RequestStop()` 方法用于请求停止操作。它没有参数，表示请求停止操作而没有指定错误信息。
- `RequestStop(Status error)` 方法用于请求停止操作，并传递一个 `Status` 对象作为错误信息。
- `RequestStopFromSignal(int signum)` 方法用于通过信号号码请求停止操作。
- `token()` 方法返回一个 `StopToken` 对象，用于检查是否请求了停止操作。
- `Reset()` 方法用于重置 `StopSource` 的状态。

`StopSource` 类内部使用了一个 `std::shared_ptr<StopSourceImpl>`，这是一个智能指针，用于管理 `StopSourceImpl` 的生命周期。这样可以确保在多个线程中共享和安全地使用 `StopSource` 对象。

`ARROW_EXPORT` 是一个宏，用于声明该类是可导出的，以便在库中使用或在其他模块中引用。这通常用于库的导出和可见性控制。

```c++
class ARROW_EXPORT StopToken {
public:
    StopToken() {}

    explicit StopToken(std::shared_ptr<StopSourceImpl> impl) : impl_(std::move(impl)) {}

    // A trivial token that never propagates any stop request
    static StopToken Unstoppable() { return StopToken(); }

    // Producer API (the side that gets asked to stopped)
    Status Poll() const;
    bool IsStopRequested() const;

protected:
    std::shared_ptr<StopSourceImpl> impl_;
};
```

这是一个名为 `StopToken` 的类的定义。`StopToken` 用于检查是否请求了停止操作，并提供了与停止操作相关的功能。下面是该类的一些重要成员和功能：

- 默认构造函数 `StopToken()` 用于创建一个空的 `StopToken` 对象。
- 显式构造函数 `StopToken(std::shared_ptr<StopSourceImpl> impl)` 接受一个指向 `StopSourceImpl` 的共享指针作为参数，并创建一个 `StopToken` 对象。
- `Unstoppable()` 是一个静态成员函数，返回一个不可停止的 `StopToken` 对象，表示没有请求停止操作。
- `Poll()` 方法用于轮询是否请求了停止操作，并返回相应的状态信息。
- `IsStopRequested()` 方法用于检查是否请求了停止操作。

`StopToken` 类内部使用了一个 `std::shared_ptr<StopSourceImpl>`，这是一个智能指针，用于管理 `StopSourceImpl` 的生命周期。这样可以确保在多个线程中共享和安全地使用 `StopToken` 对象。

## cancel.cpp

```c++
struct StopSourceImpl {
    // 用于存储取消操作的状态信息。它包含了一个原子整数 requested_
    // 和一个互斥锁 mutex_，以及一个用于存储取消错误的 Status 对象。
    std::atomic<int> requested_{0};  // will be -1 or signal number if requested
    // 如果 requested_ 的值为 0，表示取消操作没有被请求。
    // 如果 requested_ 的值为 -1，表示取消操作已经被请求。
    // 如果 requested_ 的值大于 0，表示取消操作由特定的信号（signal）引发，该值代表了信号的编号。
    std::mutex mutex_;
    Status cancel_error_;
};

StopSource::StopSource(): impl_(new StopSourceImpl) {}

StopSource::~StopSource() = default;

// 请求取消操作
void StopSource::RequestStop() {
    RequestStop(Status::Cancelled("Operation cancelled"));
}

void StopSource::RequestStop(Status st) {
    // 在作用域控制对象所有权的类型
    std::lock_guard<std::mutex> lock(impl_ -> mutex_);

    DCHECK_NOT_OK(st);
    if (!impl_ -> requested_) {
        impl_ -> requested_ = -1;
        impl_ -> cancel_error_ = std::move(st); // 转换所有权
    }
}

void StopSource::RequestStopFromSignal(int signum) {
    impl_ -> requested_.store(signum); // 存储signum
}

// 重新启动
void StopSource::Reset() {
    std::lock_guard<std::mutex> lock(impl_ -> mutex_);
    impl_ -> cancel_error_ = Status::OK(); // 返回Status成员
    impl_ -> requested_.store(0);
}

// StopToken于表示一个取消令牌，可以用于检查是否请求了取消操作，并通过 Poll() 函数来获取取消错误的状态。

StopToken StopSource::token() { return StopToken(impl_); }

bool StopToken::IsStopRequested() const {
    if (!impl_) {
        return false;
    }
    return impl_ -> requested_.load() != 0;
}

Status StopToken::Poll() const {
    if (!impl_) {
        return Status::OK(); // 返回Status成员
    }
    if (!impl_->requested_.load()) {
        return Status::OK(); // 返回Status成员
    }

    std::lock_guard<std::mutex> lock(impl_->mutex_);
    if (impl_->cancel_error_.ok()) { // 如果statuscode不等于OK
        auto signum = impl_->requested_.load();
        DCHECK_GE(signum, 0); // 断言不成立，离开终止程序
        impl_->cancel_error_ = Status::Cancelled("Operation cancelled");
    }
    return impl_->cancel_error_;
}
```

`std::lock_guard<std::mutex> lock(impl_ -> mutex_);` **这行代码创建了一个名为 `lock` 的 `std::lock_guard` 对象，并对 `impl_->mutex_` 进行了加锁操作。`std::lock_guard` 是一个便捷的 RAII（Resource Acquisition Is Initialization）类，用于自动管理锁的获取和释放。在构造函数中，它会获取指定的锁，并在析构函数中自动释放锁，**从而确保在作用域结束时锁会被正确释放，避免了因忘记释放锁而引发的死锁等问题。在这种情况下，`lock` 对象的生命周期被限制在 `std::lock_guard` 所在的作用域内，一旦超出该作用域，`lock` 对象将会被销毁，从而自动释放 `impl_->mutex_` 锁。这样可以确保在 `std::lock_guard` 所在的作用域中对 `impl_->mutex_` 的访问是线程安全的，避免了多个线程同时访问和修改共享资源的竞争条件。

`Status::OK(); // 返回Status成员`

`Status::ok()` 返回statuscode

# macros.h

```c++
#define ARROW_PREDICT_FALSE(x) (__builtin_expect(!!(x), 0))

#define DCHECK_GE(val1, val2) \
  do { \
    if (!((val1) >= (val2))) { \
      std::abort(); \
    } \
  } while (false)
```



`ARROW_PREDICT_FALSE` 是一个宏定义，它使用了内建函数 `__builtin_expect`，用于进行分支预测的优化提示。

`__builtin_expect` 是一个编译器内建函数，它的作用是向编译器提供关于分支条件的期望信息。该函数的原型为：

```cpp
long __builtin_expect(long exp, long c);
```

其中 `exp` 是一个表达式，`c` 是一个常量。函数返回 `exp`，并且向编译器传递了 `exp` 很可能等于 `c` 的提示。

在宏定义 `ARROW_PREDICT_FALSE` 中，`__builtin_expect` 函数被用于将表达式 `(x)` 的期望结果设置为 0。这样，编译器就会认为 `(x)` 很可能为假，并相应地优化代码，以减少分支预测失败的开销。

使用 `ARROW_PREDICT_FALSE` 宏可以帮助编译器更好地进行代码优化，提高程序的执行效率。但需要注意的是，`__builtin_expect` 函数的效果在不同的编译器和平台上可能会有所差异，因此在使用时需要结合具体的编译器和目标平台进行评估和测试。



预测器指令（如 `ARROW_PREDICT_FALSE`）是一种编译器指示，用于向编译器提供关于代码执行路径的提示。这些提示可以帮助编译器在生成机器码时做出更好的优化决策，从而提高代码的执行效率。

在这种情况下，`ARROW_PREDICT_FALSE` 用于向编译器提示后面的条件表达式很可能为假。这样，编译器可以将更多的优化资源用于假设条件为真的代码路径，提高这个路径上的指令预取、分支预测等优化效果。

通过使用预测器指令，可以帮助编译器更好地利用硬件资源，并生成更有效的机器码。这可能会减少分支预测失败的次数，提高指令级并行性，从而提高代码的执行速度。

需要注意的是，预测器指令只是一种提示，实际效果取决于编译器的实现和目标平台的硬件特性。在某些情况下，预测器指令可能对代码性能产生积极影响，但在其他情况下可能没有明显的改进。因此，在使用预测器指令时，需要结合具体情况进行评估和测试，以确保其对代码性能的实际影响。



在宏定义中，使用反斜杠 `\` 进行换行，使得宏定义可以跨越多行。这样可以增加代码的可读性。

宏定义的实现包含一个 `do...while(false)` 循环，这样可以在使用宏时使用分号进行结尾，类似于语句的形式。

`DCHECK_GE` 宏接受两个参数 `val1` 和 `val2`，并使用条件 `(val1) >= (val2)` 进行断言检查。如果断言条件不成立，则调用 `std::abort()` 终止程序的执行。



**本来希望在这里使用googletest中的断言语句`EXPECT_EQ`进行替代`ASSERT_EQ` 宏，但是`EXPECT_EQ`在运行代码情况中不会停止运行，会继续执行后续代码，但是`ASSERT_EQ` 宏定义中，会在断言失败时终止当前测试用例的执行。**

# status.h

```c++
//explicit只能使用显式转换 阻止行为Status x = Status(code);
```

这段代码定义了一个名为 `Status` 的C++类，用于表示一种状态。该类具有以下功能：

1. 构造函数：`Status` 类有多个构造函数，包括默认构造函数、接受状态代码和消息的构造函数。构造函数用于创建不同状态的 `Status` 对象。其中就包含两个部分：`StatusCode code_`和`std::string msg_`
2. 成员函数：
   - `code()`: 返回状态代码。
   - `message()`: 返回状态消息。
   - `ok()`: 检查状态是否为OK。
   - `ToString()`: 将状态转换为字符串表示，包括状态代码和消息。
3. 静态成员函数：
   - `OK()`: 返回一个表示OK状态的 `Status` 对象。
   - `Invalid(const std::string& msg)`: 返回一个表示无效状态的 `Status` 对象，可附带自定义消息。
   - `Cancelled(const std::string& msg)`: 返回一个表示被取消状态的 `Status` 对象，可附带自定义消息。
   - `KeyError(const std::string& msg)`: 返回一个表示键错误状态的 `Status` 对象，可附带自定义消息。
4. `Abort` 函数：当发生严重错误时，用于输出错误消息到标准错误流 (`std::cerr`) 并终止程序执行。

此类允许您创建不同状态的对象，并提供了一种便捷的方式来检查状态、获取状态信息以及输出错误消息。这在处理程序中的状态和错误时非常有用。

# executor.h

```c++
using StopCallback = internal::FnOnce<void(const Status&)>;
```

`StopCallback`是一个类型别名，它被定义为`internal::FnOnce<void(const Status&)>`。这表示`StopCallback`是一个可调用对象（函数或函数对象），该对象接受一个`const Status&`类型的参数并返回`void`。在这种情况下，`StopCallback`通常用作停止回调函数，用于在任务停止时执行特定的操作或清理工作。

```c++
Status Spawn(Function&& func) {
       return SpawnReal(TaskHints{}, std::forward<Function>(func), StopToken::Unstoppable(),
                             StopCallback{});
}
```

这段代码中的`Spawn`函数接受一个可调用对象（函数或函数对象）`func`，并使用默认的`TaskHints`、`StopToken::Unstoppable()`和空的`StopCallback`调用`SpawnReal`函数来实际执行任务。返回值是一个`Status`对象，表示任务的执行状态。

```c++
template <typename Function, typename... Args,
                typename ReturnType = typename std::invoke_result<Function, Args...>::type> // bug
        std::future<ReturnType> Submit(TaskHints hints, StopToken stop_token,
                                       StopCallback stop_callback, Function&& func,
                                       Args&&... args) {
            std::promise<ReturnType> promise;
            std::future<ReturnType> future = promise.get_future();
            auto task = [func = std::forward<Function>(func),
                    tup = std::make_tuple(std::forward<Args>(args)...),
                    promise = std::move(promise)]() mutable {
                try {
                    if constexpr (!std::is_void_v<ReturnType>) {
                        ReturnType result = std::apply(std::move(func), std::move(tup));
                        promise.set_value(result);
                    } else {
                        std::apply(std::move(func), std::move(tup));
                    }
                } catch (...) {
                    promise.set_exception(std::current_exception());
                }
            };

            Status status =
                    SpawnReal(hints, std::move(task), stop_token, std::move(stop_callback));
            if (!status.ok()) {
                throw std::runtime_error("Failed to submit task");
            }

            return future;
        }
```

原来有一个bug `typename ReturnType = std::invoke_result<Function, Args...>` 修改为 `typename std::invoke_result<Function, Args...>::type`

这是一个`Submit`函数的模板定义，用于将任务提交到任务池中执行。函数模板参数包括`Function`（函数类型）、`Args`（参数列表）和`ReturnType`（返回类型）。如果未指定`ReturnType`，则使用`std::invoke_result<Function, Args...>`推导返回类型。

该函数使用`std::promise`和`std::future`创建一个异步任务，并将其结果返回给调用者。任务使用lambda函数表示，其中捕获了传入的`func`和`args`作为闭包的一部分。在任务执行时，它会尝试调用`func`，并将参数列表`args`展开为`func`的参数。根据`ReturnType`的类型，使用`std::apply`调用`func`，并将结果设置到`promise`中。

如果任务执行过程中抛出异常，将捕获异常并使用`std::current_exception()`将异常传递给`promise`，以便将异常作为`future`的异常结果。

最后，函数调用`SpawnReal`将任务提交给任务池执行，并返回一个表示任务结果的`future`对象。如果任务提交失败（`SpawnReal`返回非`ok()`的`Status`），则抛出`std::runtime_error`异常。

总而言之，`Submit`函数用于将任务提交到任务池并返回一个`future`对象，以便在需要时获取任务的执行结果。

## part0

`internal::FnOnce<void()>` 看起来是一个函数对象或可调用对象，其类型表示它是一个接受无参数并返回 `void` 的函数或可调用对象。在C++中，函数对象通常用于代表某个可执行的操作，这些操作可以像函数一样被调用。

- `internal::FnOnce`: 这可能是一个命名空间、类或结构体，用于封装函数对象的类型或可调用对象的类型。`FnOnce` 通常用于表示函数或操作，表示它可以被执行一次。

- `<void()>`: 这是函数对象的参数列表和返回值类型。在这里，参数列表为空，表示不接受任何参数，而返回值类型为 `void`，表示函数或操作不返回任何值。

所以，`internal::FnOnce<void()> task` 声明了一个名为 `task` 的对象，它是一个函数对象或可调用对象，可以被执行一次，接受零个参数，执行后不返回任何值（`void`）。通常，这种语法用于存储一个可执行的操作，然后在适当的时候执行它，例如使用 `task()` 来执行它。

## Part1

```c++
std::promise<ReturnType> promise;
std::future<ReturnType> future = promise.get_future();
```

这部分代码使用了 C++ 的线程和并发库中的 `std::promise` 和 `std::future`。

`std::promise` 是一个用于在一个线程中设置值（或异常），然后在另一个线程中获取值的机制。它提供了一个可以被设置值的"承诺"，然后通过 `std::future` 来获取这个值。

在这段代码中，首先创建了一个 `std::promise` 对象 `promise`，它可以用于保存某个类型为 `ReturnType` 的值。然后，通过调用 `get_future()` 方法，获得与 `promise` 关联的 `std::future` 对象 `future`。`std::future` 表示一个未来可能会被设置值的结果。

`std::promise` 和 `std::future` 通常在多线程编程中用于实现线程间的通信和数据共享。**通过将值设置到 `std::promise` 中，并在另一个线程中通过 `std::future` 获取该值，可以实现线程间的同步和结果传递。** go语言！

在这段代码中，`std::promise` 用于保存异步任务的结果，并通过 `get_future()` 方法获得一个与该结果相关联的 `std::future` 对象。随后，这个 `std::future` 可以被返回给调用者，以便在需要的时候获取异步任务的结果。

需要注意的是，在使用 `std::promise` 和 `std::future` 时，需要确保在合适的时机调用 `std::promise` 的 `set_value()` 或 `set_exception()` 方法来设置值或异常，以便 `std::future` 可以获取到正确的结果。

总结起来，这段代码使用了 `std::promise` 和 `std::future` 来实现异步任务的结果传递和获取。`std::promise` 用于保存结果，而 `std::future` 则用于获取这个结果。

## Part2

```c++
auto task = [func = std::forward<Function>(func),
                    tup = std::make_tuple(std::forward<Args>(args)...),
                    promise = std::move(promise)]() mutable {
```

这部分代码定义了一个 lambda 表达式 `task`，用于表示要执行的异步任务。

lambda 表达式是一种匿名函数对象，可以捕获外部变量，并定义函数体。在这个 lambda 表达式中，使用了捕获初始化器（capture initializer）来捕获外部变量，并将其绑定到 lambda 表达式内部的局部变量。

具体来说，捕获初始化器 `[func = std::forward<Function>(func), tup = std::make_tuple(std::forward<Args>(args)...), promise = std::move(promise)]` 定义了三个捕获变量：

1. `func`：使用 `std::forward<Function>(func)` 来捕获参数 `func`，并将其绑定到名为 `func` 的局部变量。这个变量表示要执行的函数对象。

2. `tup`：使用 `std::make_tuple(std::forward<Args>(args)...)` 来捕获参数 `args`，并将其绑定到名为 `tup` 的局部变量。这个变量表示要传递给函数对象的参数元组。

3. `promise`：使用 `std::move(promise)` 来捕获参数 `promise`，并将其绑定到名为 `promise` 的局部变量。这个变量表示保存异步任务结果的 `std::promise` 对象。

接着，定义了 lambda 表达式的函数体：

```cpp
() mutable {
    try {
        if constexpr (!std::is_void_v<ReturnType>) {
            ReturnType result = std::apply(std::move(func), std::move(tup));
            promise.set_value(result);
        } else {
            std::apply(std::move(func), std::move(tup));
        }
    } catch (...) {
        promise.set_exception(std::current_exception());
    }
}
```

这个函数体中，首先使用 `std::apply()` 调用捕获的函数对象 `func`，并将参数元组 `tup` 进行展开，传递给函数对象进行执行。然后根据 `ReturnType` 是否为 `void`，决定是否将函数执行结果通过 `promise.set_value()` 设置到 `std::promise` 中。

如果发生了异常，使用 `std::current_exception()` 获取当前异常，并通过 `promise.set_exception()` 将异常传递给 `std::promise`。

最终，定义的 lambda 表达式 `task` 可以作为一个可调用对象，在适当的时机被调用，执行异步任务的逻辑。

## std::invoke

`std::invoke_result` 是一个 C++17 中的类型工具，用于获取函数调用表达式的返回类型。

它的用法是 `std::invoke_result<Function, Args...>`，其中 `Function` 是函数类型，`Args...` 是函数的参数列表。`std::invoke_result` 会推导出函数调用表达式 `Function(Args...)` 的返回类型。

例如，假设有一个函数 `int add(int a, int b)`，我们可以使用 `std::invoke_result` 来获取其返回类型：

```cpp
using ResultType = std::invoke_result<decltype(add), int, int>::type;
```

在这个例子中，`decltype(add)` 表达式获取了 `add` 函数的类型，然后通过 `std::invoke_result` 获取了 `add` 函数的返回类型 `int`。

`std::invoke_result` 的主要用途是在编译时获取函数调用的返回类型，可以用于类型推导和模板元编程中。它遵循了函数调用的规则，包括函数指针、成员函数指针、函数对象和成员函数对象等不同情况。

需要注意的是，如果函数调用表达式不是可调用的或者有多个重载函数，或者对于某些函数指针和成员函数指针可能需要使用 `typename std::invoke_result<Function, Args...>::type` 来获取返回类型。

总结起来，`std::invoke_result` 是一个有用的类型工具，可用于获取函数调用表达式的返回类型，以便在编译时进行类型检查和类型推导。

# functional.h

```c++
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
```

这段代码定义了一个通用的函数封装类 `FnOnce`，用于封装任意返回类型为 `R`，参数列表为 `A...` 的可调用对象。

首先，定义了一个模板类 `FnOnce`，并使用部分特化来匹配特定的函数类型 `R(A...)`。

类中有以下成员函数和成员变量：

- 默认构造函数 `FnOnce() = default;`，用于创建一个默认的函数封装对象。

- 模板构造函数 `FnOnce(Fn fn)`，用于根据传入的可调用对象 `fn` 来构造函数封装对象。该构造函数使用了 `std::enable_if` 来限制传入的可调用对象必须满足以下条件：
  - `decltype(std::declval<Fn&&>()(std::declval<A>()...))` 可转换为 `R`：即传入的可调用对象返回值可以转换为 `R` 类型。
  - `std::is_convertible<decltype(std::declval<Fn&&>()(std::declval<A>()...)), R>::value` 为 `true`：即传入的可调用对象返回值可以隐式转换为 `R` 类型。

- `operator bool()` 函数，用于将函数封装对象转换为布尔值。返回 `true` 表示函数封装对象非空，`false` 表示函数封装对象为空。

- `operator()(A... a) &&` 函数，用于执行一次实际的操作。函数封装对象被移动后，控制权转移到局部变量 `bye`，然后调用 `bye->invoke(std::forward<A&&>(a)...)` 来执行实际的操作。在调用 `invoke` 时，使用了参数包展开 `std::forward<A&&>(a)...` 来将参数 `a` 转发给实际的操作。

- `struct Impl`，作为基类，定义了一个纯虚函数 `invoke(A&&... a)`，用于表示实际的操作。派生类需要实现这个函数。

- `struct FnImpl`，作为派生类，继承自 `Impl`，并实现了 `invoke(A&&... a)` 函数。在 `invoke` 函数中，将传入的可调用对象 `fn` 移动到成员变量 `fn_` 中，并使用参数包展开 `std::forward<A&&>(a)...` 将参数转发给可调用对象执行。

- `std::unique_ptr<Impl> impl_`，作为成员变量，用于管理具体实现的对象指针。

这样，通过 `FnOnce` 类的对象，可以将任意的可调用对象进行封装，并在需要执行时进行调用。封装对象可以被移动，从而转移对实际操作内存的控制权。

# thread_pool.h

```c++
static std::optional<std::shared_ptr<ThreadPool>> Make(int threads);
```

这是一个静态成员函数 `Make`，用于创建一个线程池对象。它接受一个 `int` 类型的参数 `threads`，表示线程池中的线程数量。

函数返回一个 `std::optional<std::shared_ptr<ThreadPool>>` 对象，即一个可选的共享指针。这意味着函数可能返回一个空值，或者返回一个指向线程池对象的共享指针。

函数的实现细节可能在其他地方定义，但根据函数签名和命名约定来看，它可能会创建一个具有指定数量线程的线程池，并将线程池对象封装在共享指针中返回。

使用该函数时，你可以通过检查返回的 `std::optional` 对象是否包含值来确定是否成功创建了线程池对象。如果包含值，则可以通过解引用共享指针来访问线程池对象。

`std::optional` 是 C++17 中引入的一个类模板，用于表示可能包含值的对象。它的目的是提供一种安全、轻量级的方式来处理可能存在或不存在值的情况，以替代传统的用特殊值（例如空指针）表示缺失值的方式。

`std::optional` 可以包含任意类型的值，并提供一组成员函数来检查对象是否包含值、访问值、重置值等操作。它的使用方式类似于指针，但具有更好的语义和安全性。

使用 `std::optional` 可以明确地表示某个函数的返回值可能为空，而不是使用特殊值或异常来表示。这样可以更清晰地表达代码的意图，并减少潜在的错误。

在上述代码中，`std::optional<std::shared_ptr<ThreadPool>>` 表示返回的结果可能是一个包含 `std::shared_ptr<ThreadPool>` 的可选对象。这意味着调用 `Make` 函数可能会返回一个空的 `std::optional`，或者一个包含指向 `ThreadPool` 对象的共享指针的 `std::optional`。使用 `std::optional` 可以避免对空指针进行解引用等错误操作，并提供更明确的接口来处理可能为空的情况。

```c++
std::shared_ptr<State> sp_state_; // 指向state的智能指针
State* state_; // 指向state的原始指针
bool shutdown_on_destroy_; // 表示销毁对象是否执行关机操作
pid_t pid_; // 用于存储进程标识符
```

# thread_pool.cpp

```c++
struct Task {
    internal::FnOnce<void()> callable;
    StopToken stop_token;
    Executor::StopCallback stop_callback; // 见于StopCallback
};
```

在上述代码中，`Task` 结构体定义了一个任务对象，用于表示要在线程池中执行的任务。

`internal::FnOnce<void()> callable` 是一个可调用对象，它的类型是 `internal::FnOnce<void()>`。它表示要执行的具体任务，是一个不接受任何参数并且没有返回值的可调用对象。可以将函数、函数对象、Lambda 表达式等作为 `callable`。

`StopToken stop_token` 是一个 `StopToken` 对象，用于检查任务是否被请求停止。`StopToken` 是一个用于停止任务的标记，它可以检查是否有停止请求，以便任务可以在适当的时候终止执行。

`Executor::StopCallback stop_callback` 是一个 `Executor::StopCallback` 类型的对象，它表示在任务停止时要执行的回调函数。`StopCallback` 是一个函数对象，接受一个 `Status` 参数，并在任务停止时调用该函数。

这些成员变量组合在一起，构成了一个任务对象，包含了要执行的任务、停止标记以及停止时的回调函数。在线程池中执行任务时，将创建 `Task` 对象并将其提交给线程池，线程池会根据 `stop_token` 来判断是否停止任务，并在需要时调用 `stop_callback`。

```c++
struct ThreadPool::State {
    State() = default;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable cv_shutdown_;
    std::condition_variable cv_idle_;

    std::list<std::thread> workers_;
    // Transhcan for finished threads
    std::vector<std::thread> finished_workers_;
    std::deque<Task> pending_tasks_;

    int desired_capacity_ = 0;
    int tasks_queued_or_running_ = 0;
    bool please_shutdown_ = false;
    bool quick_shutdown_ = false;
};
```

在上述代码中，`ThreadPool::State` 结构体定义了线程池的状态对象，用于管理线程池的状态信息。

`std::mutex mutex_` 是一个互斥锁，用于保护对线程池状态的访问，确保在多线程环境下的线程安全性。

`std::condition_variable cv_` 是一个条件变量，用于线程间的同步和通信。它用于实现线程池中的任务调度和线程的等待与唤醒。

`std::condition_variable cv_shutdown_` 是一个条件变量，用于线程池的关闭。当线程池被请求关闭时，等待在这个条件变量上的线程会被唤醒。

`std::condition_variable cv_idle_` 是一个条件变量，用于表示线程池是否处于空闲状态。当线程池中没有任务执行时，等待在这个条件变量上的线程会被唤醒。

`std::list<std::thread> workers_` 是一个线程列表，用于保存线程池中的工作线程。

`std::vector<std::thread> finished_workers_` 是一个已完成的线程列表，用于保存已经完成执行的工作线程。

`std::deque<Task> pending_tasks_` 是一个任务队列，用于保存待执行的任务。

`int desired_capacity_` 表示线程池的期望容量，即期望的工作线程数量。

`int tasks_queued_or_running_` 表示当前排队或正在执行的任务数量。

`bool please_shutdown_` 表示是否请求关闭线程池。

`bool quick_shutdown_` 表示是否快速关闭线程池。

这些成员变量组合在一起，构成了线程池的状态信息，用于管理线程池中的线程、任务和关闭操作。线程池的操作和调度都会涉及到这些状态变量的读取和修改。

```c++
static void WorkerLoop(std::shared_ptr<ThreadPool::State> state,
                       std::list<std::thread>::iterator it) {
    std::unique_lock<std::mutex> lock(state->mutex_);
    // Since we hold the lock, `it` now points to the correct thread object
    // (LaunchWorkersUnlocked has exited)
    DCHECK_EQ(std::this_thread::get_id(), it->get_id());

    // If too many threads, we should secede from the pool
  // 这段代码定义了一个 lambda 函数 `should_secede`，该函数用于判断是否应该进行分离（secede）操作。它捕获了外部的状态 `state`，并通过访问 `state->workers_.size()` 和 `state->desired_capacity_` 来进行判断。
    const auto should_secede = [&]() -> bool {
        return state->workers_.size() > static_cast<size_t>(state->desired_capacity_);
    };

    while (true) {
        // By the time this thread is started, some tasks may have been pushed
        // or shutdown could even have been requested.  So we only wait on the
        // condition variable at the end of the loop.

        // Execute pending tasks if any
        while (!state->pending_tasks_.empty() && !state->quick_shutdown_) {
            // We check this opportunistically at each loop iteration since
            // it releases the lock below.
            if (should_secede()) {
                break;
            }

            DCHECK_GE(state->tasks_queued_or_running_, 0);
            {
                Task task = std::move(state->pending_tasks_.front());
                state->pending_tasks_.pop_front();
                StopToken *stop_token = &task.stop_token;
                lock.unlock();
                if (!stop_token->IsStopRequested()) {
                    std::move(task.callable)();
                } else {
                    if (task.stop_callback) {
                        std::move(task.stop_callback)(stop_token->Poll());
                        // std::move来将 task.stop_callback 移动（Move）到函数调用中，
                        // 同时传递了 stop_token->Poll() 作为参数。
                    }
                }
                ARROW_UNUSED(std::move(task));  // release resources before waiting for lock
                lock.lock();
            }
            if (ARROW_PREDICT_FALSE(--state->tasks_queued_or_running_ == 0)) {
                state->cv_idle_.notify_all();
            }
        }
        // Now either the queue is empty *or* a quick shutdown was requested
        if (state->please_shutdown_ || should_secede()) {
            break;
        }
        // Wait for next wakeup
        state->cv_.wait(lock);
    }
    DCHECK_GE(state->tasks_queued_or_running_, 0);

    // We're done.  Move our thread object to the trashcan of finished
    // workers.  This has two motivations:
    // 1) the thread object doesn't get destroyed before this function finishes
    //    (but we could call thread::detach() instead)
    // 2) we can explicitly join() the trashcan threads to make sure all OS threads
    //    are exited before the ThreadPool is destroyed.  Otherwise subtle
    //    timing conditions can lead to false positives with Valgrind.
    DCHECK_EQ(std::this_thread::get_id(), it->get_id());
    state->finished_workers_.push_back(std::move(*it));
    state->workers_.erase(it);
    if (state->please_shutdown_) {
        // Notify the function waiting in Shutdown().
        state->cv_shutdown_.notify_one();
      // 是用于通知等待在 state->cv_shutdown_ 条件变量上的线程，表示线程池即将关闭。
    }
}
```



```c++
void ThreadPool::WaitForIdle() {
    std::unique_lock<std::mutex> lk(state_->mutex_);
    state_->cv_idle_.wait(lk, [this] { return state_->tasks_queued_or_running_ == 0; });
}
```

`ThreadPool::WaitForIdle()` 函数用于等待线程池变为空闲状态。该函数会阻塞当前线程，直到线程池中没有正在执行或排队的任务。

具体实现如下：

1. 创建一个 `std::unique_lock<std::mutex>` 对象 `lk`，用于对线程池状态的互斥锁进行加锁。

2. 调用 `state_->cv_idle_.wait(lk, [this] { return state_->tasks_queued_or_running_ == 0; })`，等待条件变量 `state_->cv_idle_` 的条件满足。条件是：线程池中没有正在执行或排队的任务，即 `state_->tasks_queued_or_running_ == 0`。

3. 在等待期间，如果条件不满足，当前线程会被阻塞，同时互斥锁会被释放，允许其他线程对线程池状态进行修改。

4. 当条件满足时，当前线程会被唤醒，互斥锁会被重新加锁。函数执行结束，线程池已经变为空闲状态。

通过调用 `ThreadPool::WaitForIdle()` 函数，可以确保在需要等待线程池变为空闲状态时，当前线程不会继续执行，直到线程池中的任务全部完成。这对于需要在任务执行完毕后进行后续操作的场景非常有用。

```c++
void ThreadPool::ProtectAgainstFork() {
    pid_t current_pid = getpid();
    if (pid_ != current_pid) {
        int capacity = state_->desired_capacity_;

        auto new_state = std::make_shared<ThreadPool::State>();
        new_state->please_shutdown_ = state_->please_shutdown_;
        new_state->quick_shutdown_ = state_->quick_shutdown_;

        pid_ = current_pid;
        sp_state_ = new_state;
        state_ = sp_state_.get();

        if (!state_->please_shutdown_) {
            ARROW_UNUSED(SetCapacity(capacity));
        }
    }
}
```

**调用 `ProtectAgainstFork` 函数是为了确保在线程池进行容量设置之前，线程池的状态是一致的和正确的。**

`ThreadPool::ProtectAgainstFork` 函数用于在多进程环境中保护线程池的一致性。

在多进程环境中，当发生进程 fork 操作时，子进程会继承父进程的资源和状态，包括线程池对象。为了避免子进程和父进程使用同一个线程池对象导致的不一致性和竞争条件，需要在子进程中重新创建一个线程池对象。

该函数首先获取当前进程的 PID（进程 ID），并将其与线程池对象中保存的 PID 进行比较。如果当前 PID 与保存的 PID 不同，说明发生了进程 fork，需要进行线程池的重新保护。

在重新保护线程池时，函数创建一个新的状态对象 `new_state`，并将原来的状态信息复制给新的状态对象。然后更新成员变量 `pid_` 和 `sp_state_`，使其指向新的状态对象，并将 `state_` 设置为新状态对象的指针。

这样，在子进程中就拥有了一个独立的线程池对象，不会与父进程共享状态。同时，如果线程池在 fork 前没有被标记为 `please_shutdown_`，则通过调用 `SetCapacity` 方法重新设置线程池的容量，以确保在子进程中继续正常工作。

通过对线程池对象进行保护，可以确保在多进程环境中每个进程都有自己独立的线程池，避免了竞争条件和不一致性问题。

```c++
Status ThreadPool::SetCapacity(int threads) {
    ProtectAgainstFork();
    std::unique_lock<std::mutex> lock(state_->mutex_);
    if (state_->please_shutdown_) {
        return Status::Invalid("operation forbidden during or after shutdown");
    }
    if (threads <= 0) {
        return Status::Invalid("ThreadPool capacity must be > 0");
    }
    CollectFinishedWorkersUnlocked();

    state_->desired_capacity_ = threads;
    const int required = std::min(static_cast<int>(state_->pending_tasks_.size()),
                                  threads - static_cast<int>(state_->workers_.size()));

    if (required > 0) {
        LaunchWorkersUnlocked(required);
    } else if (required < 0) {
        // Excess threads are running, wake them so that they stop
        state_ -> cv_.notify_all();
    }
    return Status::OK();
}
```

`ThreadPool::SetCapacity` 函数用于设置线程池的容量（即线程数量）。

函数首先调用 `ProtectAgainstFork` 函数，以保护线程池的一致性。

然后，函数获取线程池状态对象的互斥锁，以确保线程池状态的独占访问。

接下来，函数进行一系列验证和调整操作：

- 如果线程池已经被标记为 `please_shutdown_`，则表示线程池正在关闭过程中或已经关闭，此时禁止进行容量设置操作，直接返回一个表示操作无效的错误状态 `Status::Invalid`。
- 如果要设置的线程数量小于等于0，也返回一个错误状态 `Status::Invalid`，因为线程池的容量必须大于0。
- 函数调用 `CollectFinishedWorkersUnlocked` 函数，清理已完成的线程，将它们从线程池中移除。

接下来，函数更新线程池的期望容量 `desired_capacity_`，即设置线程池应有的线程数量。

然后，根据需要启动新的工作线程或停止多余的工作线程：

- 如果有需要启动的新工作线程，调用 `LaunchWorkersUnlocked` 函数，在未加锁状态下启动指定数量的新工作线程。
- 如果有多余的工作线程在运行，调用 `state_->cv_.notify_all()` 唤醒这些多余的工作线程，使其停止运行。

最后，函数返回一个表示操作成功的状态 `Status::OK()`。

通过 `SetCapacity` 函数，可以动态地调整线程池的容量，以满足实际需求，并确保线程池在进行容量调整时的一致性和正确性。

```c++
void ThreadPool::LaunchWorkersUnlocked(int threads) {
    std::shared_ptr<State> state = sp_state_;

    for (int i = 0; i < threads; i++) {
        state_->workers_.emplace_back(); // 插入空线程对象
        auto it = --(state_->workers_.end()); // 指向末尾
        *it = std::thread([this, state, it] {
            current_thread_pool_ = this;
            WorkerLoop(state, it);
        });
    }
}
```

`LaunchWorkersUnlocked` 函数用于启动指定数量的工作线程。

在函数内部，首先获取线程池的共享状态 `state`。然后通过一个循环，依次创建并启动指定数量的线程。

对于每个线程，首先将一个空的线程对象插入到 `state_->workers_` 容器中，然后获取该线程对象在容器中的迭代器 `it`。接下来，通过调用 `std::thread` 构造函数，创建一个新的线程对象，并使用 lambda 表达式作为线程的执行函数。这个 lambda 表达式捕获了当前线程池的指针 `this`、线程池的共享状态 `state`，以及当前线程对象在容器中的迭代器 `it`。

在 lambda 表达式中，首先将当前线程池指针设置为 `current_thread_pool_`，以便工作线程能够访问到线程池的其他成员函数和数据。然后调用 `WorkerLoop` 函数，将线程池的共享状态和当前线程对象的迭代器作为参数传递进去。`WorkerLoop` 函数是工作线程的主循环，负责从任务队列中取出任务并执行。

通过这个过程，线程池可以创建并启动指定数量的工作线程，并将它们加入到线程池的 `workers_` 容器中。每个工作线程都会执行 `WorkerLoop` 函数，以处理任务队列中的任务。

```c++
Status ThreadPool::SpawnReal(TaskHints hints, internal::FnOnce<void()> task,
                             StopToken stop_token, StopCallback&& stop_callback) {
    {
        ProtectAgainstFork();
        std::lock_guard<std::mutex> lock(state_->mutex_);
        if (state_->please_shutdown_) {
            return Status::Invalid("operation forbidden during or after shutdown");
        }
        CollectFinishedWorkersUnlocked();
        state_->tasks_queued_or_running_++;
        if (static_cast<int>(state_->workers_.size()) < state_->tasks_queued_or_running_ &&
            state_->desired_capacity_ > static_cast<int>(state_->workers_.size())) {
            // We can still spin up more workers so spin up a new worker
            LaunchWorkersUnlocked(/*threads=*/1);
        }
        state_->pending_tasks_.push_back(
                {std::move(task), std::move(stop_token), std::move(stop_callback)});
    }
    state_->cv_.notify_one();
    return Status::OK();
}
```

`ThreadPool::SpawnReal` 函数用于将任务提交到线程池执行。

函数首先调用 `ProtectAgainstFork`，以保护线程池免受 fork 的影响。然后获取线程池的互斥锁 `state_->mutex_`，并进行加锁。

接下来，函数检查线程池的状态，如果线程池处于关闭或正在关闭的状态，则返回错误状态，表示提交任务操作被禁止。然后调用 `CollectFinishedWorkersUnlocked`，以清理已完成的工作线程。

接下来，将 `tasks_queued_or_running_` 值增加 1，表示有一个新的任务被加入队列或正在运行。如果当前工作线程的数量小于 `tasks_queued_or_running_` 值，并且线程池的期望容量大于当前工作线程的数量，则启动一个新的工作线程，通过调用 `LaunchWorkersUnlocked`。

然后，将任务封装成 `Task` 结构，并将其加入到线程池的 `pending_tasks_` 队列中。

在加锁的范围内，调用 `state_->cv_.notify_one()`，以通知一个等待中的工作线程有新任务可用。

最后，函数解锁互斥锁，并返回 `Status::OK()` 表示任务提交成功。

通过这个过程，任务被提交到线程池，并且可以被工作线程异步执行。

```c++
static int ParseOMPEnvVar(const char* name) {
    // OMP_NUM_THREADS is a comma-separated list of positive integers.
    // We are only interested in the first (top-level) number.
    auto result = GetEnvVar(name);
    if (!result.has_value()) {
        return 0;
    }
    auto str = *std::move(result);
    auto first_comma = str.find_first_of(',');
    if (first_comma != std::string::npos) {
        str = str.substr(0, first_comma);
    }
    try {
        return std::max(0, std::stoi(str));
    } catch (...) {
        return 0;
    }
}
```

`ParseOMPEnvVar` 函数用于解析环境变量 `OMP_NUM_THREADS`，该环境变量是一个逗号分隔的正整数列表。函数的目的是获取列表中的第一个（顶层）数字。

函数首先调用 `GetEnvVar` 函数获取环境变量的值，并检查是否存在。如果环境变量不存在，则返回 0。

如果环境变量的值存在，则将其移动到 `std::string` 对象 `str` 中。接下来，查找字符串中第一个逗号的位置，并将字符串截断到该逗号之前的部分。这是因为我们只对顶层的数字感兴趣，而忽略其他逗号后面的数字。

然后，使用 `std::stoi` 函数将截断后的字符串转换为整数。如果转换过程中发生异常，例如字符串无法解析为整数，将返回 0。否则，返回解析后的整数值，取最大值为 0 和解析后的整数值的较大者。

通过这个过程，函数可以解析并获取环境变量 `OMP_NUM_THREADS` 的值，并将其转换为整数表示。