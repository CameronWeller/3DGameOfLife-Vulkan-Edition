#pragma once

#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <functional>

// RAII wrapper for read-write locks
class ReadWriteLock {
public:
    void lockRead() { mutex_.lock_shared(); }
    void unlockRead() { mutex_.unlock_shared(); }
    void lockWrite() { mutex_.lock(); }
    void unlockWrite() { mutex_.unlock(); }

private:
    std::shared_mutex mutex_;
};

// RAII wrapper for read locks
class ReadLock {
public:
    explicit ReadLock(ReadWriteLock& lock) : lock_(lock) { lock_.lockRead(); }
    ~ReadLock() { lock_.unlockRead(); }
private:
    ReadWriteLock& lock_;
};

// RAII wrapper for write locks
class WriteLock {
public:
    explicit WriteLock(ReadWriteLock& lock) : lock_(lock) { lock_.lockWrite(); }
    ~WriteLock() { lock_.unlockWrite(); }
private:
    ReadWriteLock& lock_;
};

// Thread-safe value wrapper
template<typename T>
class ThreadSafeValue {
public:
    ThreadSafeValue() = default;
    explicit ThreadSafeValue(const T& value) : value_(value) {}

    T get() const {
        ReadLock lock(lock_);
        return value_;
    }

    void set(const T& value) {
        WriteLock lock(lock_);
        value_ = value;
    }

    template<typename F>
    void modify(F&& func) {
        WriteLock lock(lock_);
        func(value_);
    }

    template<typename F>
    T getAndModify(F&& func) {
        WriteLock lock(lock_);
        T result = func(value_);
        return result;
    }

private:
    mutable ReadWriteLock lock_;
    T value_;
};

// Thread-safe callback manager
template<typename... Args>
class ThreadSafeCallback {
public:
    using CallbackType = std::function<void(Args...)>;

    void setCallback(CallbackType callback) {
        WriteLock lock(lock_);
        callback_ = std::move(callback);
    }

    void clearCallback() {
        WriteLock lock(lock_);
        callback_ = nullptr;
    }

    void invoke(Args... args) const {
        ReadLock lock(lock_);
        if (callback_) {
            callback_(std::forward<Args>(args)...);
        }
    }

private:
    mutable ReadWriteLock lock_;
    CallbackType callback_;
}; 