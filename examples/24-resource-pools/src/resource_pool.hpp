#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <vector>

#if __cplusplus >= 202302L
#include <expected>
#include <string_view>
#endif

namespace pool24 {

// Why a non-blocking attempt can fail. Kept small and explicit so a C++23
// caller can tell "every resource is currently leased" apart from "the
// pool was shut down," a distinction a null optional cannot express.
enum class PoolAcquireError {
    Exhausted,
    Closed,
};

#if __cplusplus >= 202302L
constexpr std::string_view describe(PoolAcquireError error) {
    switch (error) {
        case PoolAcquireError::Exhausted:
            return "pool exhausted: every resource is currently leased";
        case PoolAcquireError::Closed:
            return "pool closed: shutdown() has already been called";
    }
    return "unknown pool acquire error";
}
#endif

// A generic, bounded pool of Resource instances, built once (or lazily,
// up to capacity) and reused rather than constructed fresh on every
// request. Thread-safe via a mutex and a condition_variable: acquire()
// blocks a caller until a resource frees up, a new one can be built under
// the capacity bound, or a timeout elapses.
//
// The returned Lease is the other half of the idiom: an RAII handle that
// puts its resource back into the pool's free list the moment it goes out
// of scope, whether the caller returns normally or an exception unwinds
// past it. A caller never calls a "give it back" method by hand.
template <typename Resource>
class ResourcePool {
public:
    using Factory = std::function<std::unique_ptr<Resource>()>;

    ResourcePool(std::size_t capacity, Factory factory)
        : capacity_(capacity), factory_(std::move(factory)) {
        if (capacity_ == 0) {
            throw std::invalid_argument("ResourcePool capacity must be at least 1");
        }
    }

    ResourcePool(const ResourcePool&) = delete;
    ResourcePool& operator=(const ResourcePool&) = delete;

    class Lease {
    public:
        Lease() = default;
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;

        Lease(Lease&& other) noexcept { *this = std::move(other); }

        Lease& operator=(Lease&& other) noexcept {
            if (this != &other) {
                release();
                pool_ = other.pool_;
                resource_ = std::move(other.resource_);
                other.pool_ = nullptr;
            }
            return *this;
        }

        ~Lease() { release(); }

        Resource& operator*() const { return *resource_; }
        Resource* operator->() const { return resource_.get(); }
        explicit operator bool() const { return resource_ != nullptr; }

    private:
        friend class ResourcePool;

        Lease(ResourcePool* pool, std::unique_ptr<Resource> resource)
            : pool_(pool), resource_(std::move(resource)) {}

        void release() {
            if (pool_ != nullptr && resource_ != nullptr) {
                pool_->give_back(std::move(resource_));
            }
            pool_ = nullptr;
        }

        ResourcePool* pool_ = nullptr;
        std::unique_ptr<Resource> resource_;
    };

    // Blocking acquire with a timeout. Waits until a resource frees up or
    // one more can be built under the capacity bound; throws
    // std::runtime_error if the timeout elapses or the pool has been shut
    // down while this call was waiting.
    Lease acquire(std::chrono::milliseconds timeout) {
        std::unique_lock lock(mutex_);
        const bool ready = cv_.wait_for(lock, timeout, [this] {
            return closed_ || !free_.empty() || created_ < capacity_;
        });
        if (!ready) {
            throw std::runtime_error("ResourcePool::acquire timed out waiting for a free resource");
        }
        if (closed_) {
            throw std::runtime_error("ResourcePool::acquire called after shutdown()");
        }
        return take_locked();
    }

#if __cplusplus >= 202302L
    // C++23: report *why* a non-blocking attempt failed.
    std::expected<Lease, PoolAcquireError> try_acquire() {
        std::unique_lock lock(mutex_);
        if (closed_) {
            return std::unexpected(PoolAcquireError::Closed);
        }
        if (free_.empty() && created_ >= capacity_) {
            return std::unexpected(PoolAcquireError::Exhausted);
        }
        return take_locked();
    }
#else
    // C++20: a null optional is the only signal available. The caller
    // learns *that* the attempt failed, never *why*.
    std::optional<Lease> try_acquire() {
        std::unique_lock lock(mutex_);
        if (closed_ || (free_.empty() && created_ >= capacity_)) {
            return std::nullopt;
        }
        return take_locked();
    }
#endif

    // Marks the pool closed and wakes every blocked acquire() so it can
    // fail fast instead of waiting out its timeout. Resources already on
    // lease are unaffected; their Lease destructors still return them to
    // the (now closed) free list.
    void shutdown() {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }
        cv_.notify_all();
    }

    std::size_t capacity() const { return capacity_; }

    std::size_t created() const {
        std::lock_guard lock(mutex_);
        return created_;
    }

    std::size_t available() const {
        std::lock_guard lock(mutex_);
        return free_.size();
    }

private:
    // Precondition: mutex_ is held by the caller and the predicate that
    // let the caller proceed (a free resource exists, or capacity allows
    // building one) has already been checked.
    Lease take_locked() {
        std::unique_ptr<Resource> resource;
        if (!free_.empty()) {
            resource = std::move(free_.back());
            free_.pop_back();
        } else {
            resource = factory_();
            ++created_;
        }
        return Lease(this, std::move(resource));
    }

    void give_back(std::unique_ptr<Resource> resource) {
        {
            std::lock_guard lock(mutex_);
            free_.push_back(std::move(resource));
        }
        cv_.notify_one();
    }

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::size_t capacity_;
    std::size_t created_ = 0;
    bool closed_ = false;
    Factory factory_;
    std::vector<std::unique_ptr<Resource>> free_;
};

}  // namespace pool24
