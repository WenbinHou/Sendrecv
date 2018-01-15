#pragma once

#ifdef UNITTEST
#define UNITTEST_NO_DOTEST
#include <tests/test.h>
#endif

#include <type_traits>
#include <functional>
#include <atomic>
#include <climits>
//_active_count 可以用掩码组合来表示当前资源的状态
//SHOTDOWN_MASK 和 CALLBACK_MASK同时出现在_active_count就表示已经shutdown并调用了回调函数
class rundown_protection
{
public:
    typedef std::function<void()> callback_t;
    typedef uint_fast64_t counter_t;
    static_assert(std::is_unsigned<counter_t>::value, "counter_t must be unsigned type.");

private:
    static const counter_t MAX_MASK = (counter_t)1 << (CHAR_BIT * sizeof(counter_t) - 1);
    static const counter_t SHUTDOWN_MASK     = MAX_MASK >> 0;
    static const counter_t CALLBACK_MASK     = MAX_MASK >> 1;
    static const counter_t ACTIVE_COUNT_MASK = ((counter_t)-1) >> 2;

    std::atomic<counter_t> _active_count;
    callback_t _callback;

public:
    counter_t get_max_mask(){
        return MAX_MASK;
    }
    counter_t get_shutdown_mask(){
        return SHUTDOWN_MASK;
    }
    counter_t get_callback_mask(){
        return CALLBACK_MASK;
    }
    counter_t get_active_count_mask(){
        return ACTIVE_COUNT_MASK;
    }
    rundown_protection()
    {
        _callback = nullptr;
        _active_count.store(0);
    }

    void register_callback(const callback_t& callback)
    {
        _callback = callback;
    }

    bool shutdown_required() const //这个是用来做什么的
    {
        return (bool)(_active_count.load() & SHUTDOWN_MASK);
    }

    bool shutdown()
    {
        const counter_t original = _active_count.fetch_or(SHUTDOWN_MASK);

        if (original == 0) {//表示当前资源没有被使用，可以立刻调用回调释放
            // !(original & SHUTDOWN_MASK) && !(original & CALLBACK_MASK) && ((original & ACTIVE_COUNT_MASK) == 0)
            invoke_callback_if_first_time();
        }
        //当original == 0, success返回true,且_active_count已经变为SHUTDOWN_MASK
        //当original != 0, 也返回true,       _active_count大于SHUTDOWN_MASK
        const bool success = !(original & SHUTDOWN_MASK);
        return success;
    }
    //need_release只是表情当前这次调用是否需要执行release操作
    //可以这么说在没有真正执行回调函数之前，有几次try_acquire就需要执行几次release操作
    bool try_acquire(bool* need_release)
    {
        // At most times, we expect try_acquire() will succeed.
        // Yet, we may use a unstrict shutdown check (do not involve `lock xchg`)
        // but this check is just a waste of time!

        //const counter_t tmp = _active_count.load(std::memory_order_relaxed);
        //if (tmp & SHUTDOWN_MASK) {
        //    return false;
        //}
        
        //当shutdown调用后，_active_count >= SHUTDOWN_MASK 
        //所以result & (CALLBACK_MASK|SHUTDOWN_MASK)==true,
        //need_release为true(表示仍然还有资源在使用),同时函数返回false(表示已经无法获取资源使用权) 
        //当资源已经释放后，且shutdown后既不需要释放且又不不能获取
        const counter_t result = ++_active_count;

        if (result & (CALLBACK_MASK | SHUTDOWN_MASK)) {

            // If we have not called the callback, we must call release() to decrease _active_count
            // However, if we have ever called the callback, we do not care about _active_count's value any more!
            // This most likely saves an atomic exhange after calling shutdown()
            if (!(result & CALLBACK_MASK)) {
                *need_release = true;
            }
            else {
                *need_release = false;
            }
            return false;
        }

        *need_release = true;
        return true;
    }

    void release()
    {
        const counter_t remain = --_active_count;

#ifdef UNITTEST
        const std::make_signed<counter_t>::type sign = (std::make_signed<counter_t>::type)remain << 2 >> 2;
        //printf("remain count: %lld\n", (long long)sign);
        TEST_ASSERT(sign >= 0);
#endif
        //当_active_count==SHUTDOWN_MASK时候，表示需要调用callback来释放资源
        if (remain == SHUTDOWN_MASK) {
            // (remain & SHUTDOWN_MASK) && !(remain & CALLBACK_MASK) && ((remain & ACTIVE_COUNT_MASK) == 0)
            invoke_callback_if_first_time();
        }
    }

private:
    void invoke_callback_if_first_time()
    {
        //如果已经没有线程使用资源，则_active_count变为SHOTDOWN_MASK
        //调用invoke_callback后，_active_count变为11000...0(即_active_count是一个大于等于2^63+2^62的数)
        //如果这时候再有try_acquire,need_release一定返回的是false
        const counter_t original = _active_count.fetch_or(CALLBACK_MASK);
            
#ifdef UNITTEST
        TEST_ASSERT(original & SHUTDOWN_MASK);
#endif

        if (!(original & CALLBACK_MASK)) {
            const callback_t cb = _callback;
            if (cb) {
                cb();
            }
        }
    }
};
