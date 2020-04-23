#pragma once
/****
*** @file  eddid_functional.h
*** @brief 简化标准库functional对象的调用
*** @author xiay
*** @version v0.1.1
*** @date   2019-11-22
*****/

#include <type_traits>
#include <memory>

// 为简化标准库函数对象
namespace eddid {
// 绑定成员函数
template <typename T, typename F, F f>
class eddid_bind_mem_function;
template <typename T, typename R, typename... Args, R(std::decay_t<T>::*func)(Args...)>
class eddid_bind_mem_function <T, R(std::decay_t<T>::*)(Args...), func>
{
public:
    eddid_bind_mem_function(T& t) : t_(t) {}
public:
    inline R operator()(Args&&... args) { return (t_.*func)(std::forward<Args>(args)...); }
private:
    T& t_;
};

template <typename T, typename R, typename... Args, R(std::decay_t<T>::*func)(Args...) const>
class eddid_bind_mem_function <T, R(std::decay_t<T>::*)(Args...) const, func>
{
public:
    eddid_bind_mem_function(T& t) : t_(t) {}
public:
    inline R operator()(Args&&... args) const { return (t_.*func)(std::forward<Args>(args)...); }
private:
    T& t_;
};

template <typename Func>
class eddid_function_wrap;
template <typename Func>
class eddid_const_function_wrap;
template <typename R, typename... Args>
class eddid_function_wrap<R(Args...)>
{
public:
     template <typename T> eddid_function_wrap(const eddid_function_wrap<T>&) = delete;
     template <typename T> eddid_function_wrap& operator=(const eddid_function_wrap<T>&) = delete;
     template <typename T> eddid_function_wrap(const eddid_const_function_wrap<T>&) = delete;
     template <typename T> eddid_function_wrap& operator=(const eddid_const_function_wrap<T>&) = delete;
     eddid_function_wrap() = default;
    ~eddid_function_wrap()
    {
        impl_.reset();
        impl_ptr_ = nullptr;
    }

    eddid_function_wrap(eddid_function_wrap&& rhs)
    {
        impl_.swap(rhs.impl_);
        impl_ptr_ = impl_.get();
    }

    eddid_function_wrap& operator=(eddid_function_wrap&& rhs)
    {
        impl_.swap(rhs.impl_);
        impl_ptr_ = impl_.get();
        return *this;
    }

    eddid_function_wrap(const eddid_function_wrap& rhs)
    {
        impl_ = rhs.impl_;
        impl_ptr_ = impl_.get();
    }

    eddid_function_wrap& operator=(const eddid_function_wrap& rhs)
    {
        impl_ = rhs.impl_;
        impl_ptr_ = impl_.get();
        return *this;
    }

public:
    template <typename Func>
    inline eddid_function_wrap(Func&& func) :impl_ptr_(nullptr) , impl_(new FunctionImpl<Func>(std::forward<Func>(func))) {impl_ptr_ = impl_.get();}
public:
    inline R operator()(Args... args) { return (*impl_ptr_)(std::forward<Args>(args)...);}

    explicit operator bool() const noexcept { return impl_ != nullptr;}

    eddid_function_wrap reference() { return *impl_ptr_; }

private:
    class FunctionInterface
    {
    public:
        FunctionInterface() = default;
        virtual ~FunctionInterface() = default;
    public:
        virtual R operator()(Args... params) = 0;
    };

    template <typename Func>
    class FunctionImpl final : public FunctionInterface
    {
    public:
        FunctionImpl() = default;
        ~FunctionImpl() = default;
    public:
        explicit FunctionImpl(Func&& f) : func_(std::forward<Func>(f)) {}
    public:
        inline R operator()(Args... params) override final { return func_(std::forward<Args>(params)...); }
    private:
        Func func_;
    };
private:
    FunctionInterface* impl_ptr_;
    std::shared_ptr<FunctionInterface> impl_;
};

template <typename R, typename... Args>
class eddid_const_function_wrap<R(Args...)>
{
public:
    template <typename T> eddid_const_function_wrap(const eddid_const_function_wrap<T>&) = delete;
    template <typename T> eddid_const_function_wrap& operator=(const eddid_const_function_wrap<T>&) = delete;
    template <typename T> eddid_const_function_wrap(const eddid_function_wrap<T>&) = delete;
    template <typename T> eddid_const_function_wrap& operator=(const eddid_function_wrap<T>&) = delete;
    eddid_const_function_wrap() = default;
    ~eddid_const_function_wrap()
    {
        impl_.reset();
        impl_ptr_ = nullptr;
    }

    eddid_const_function_wrap(eddid_const_function_wrap&& rhs)
    {
        impl_.swap(rhs.impl_);
        impl_ptr_ = impl_.get();
    }

    eddid_const_function_wrap& operator=(eddid_const_function_wrap&& rhs)
    {
        impl_.swap(rhs.impl_);
        impl_ptr_ = impl_.get();
        return *this;
    }

    eddid_const_function_wrap(const eddid_const_function_wrap& rhs) { impl_ = rhs.impl_; impl_ptr_ = impl_.get();}

    eddid_const_function_wrap& operator=(const eddid_const_function_wrap& rhs)
    {
        impl_ = rhs.impl_;
        impl_ptr_ = impl_.get();
        return *this;
    }
public:
    template <typename Func>
    inline eddid_const_function_wrap(Func&& func)
    : impl_ptr_(nullptr), impl_(new FunctionImpl<Func>(std::forward<Func>(func))) {impl_ptr_ = impl_.get();}
public:
    inline R operator()(Args... args) const { return (*impl_ptr_)(std::forward<Args>(args)...);}

    explicit operator bool() const noexcept { return impl_ != nullptr;}

    eddid_const_function_wrap reference() { return *impl_;}
private:
    class FunctionInterface
    {
    public:
        FunctionInterface() = default;
        virtual ~FunctionInterface() = default;
    public:
        virtual R operator()(Args... params) = 0;
    };
    template <typename Func>
    class FunctionImpl final : public FunctionInterface
    {
    public:
        FunctionImpl() = default;
        ~FunctionImpl() = default;
    public:
        explicit FunctionImpl(Func&& f) : func_(std::forward<Func>(f)) {}
    public:
        inline R operator()(Args... params) const override { return func_(std::forward<Args>(params)...); }
    private:
        Func func_;
    };
private:
    FunctionInterface* impl_ptr_;
    std::shared_ptr<FunctionInterface> impl_;
};

#define EDDID_BIND_MEM_FUNC(obj, func)  \
eddid::eddid_bind_mem_function<decltype(obj), decltype(&std::decay_t<decltype(obj)>::func), &std::decay_t<decltype(obj)>::func >(obj)

}















