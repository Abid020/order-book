#pragma once

#include <atomic>
#include <array>
#include <optional>
#include <cstddef>

namespace queue {

// ─────────────────────────────────────────────
//  Single Producer Single Consumer lock-free
//  ring buffer queue.
//
//  One thread calls push() — the parser.
//  One thread calls pop()  — the order book.
//  Never call push/pop from the same thread.
//
//  Capacity must be a power of two — allows
//  cheap modulo via bitmask instead of division.
// ─────────────────────────────────────────────

template<typename T, std::size_t Capacity>
class SpscQueue {
    static_assert((Capacity & (Capacity - 1)) == 0,
        "Capacity must be a power of two");

public:
    SpscQueue() : head_(0), tail_(0) {}

    // Called by producer thread only
    bool push(const T& item) {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        const std::size_t next = (tail + 1) & mask_;

        // Queue is full if next write position == head
        if (next == head_.load(std::memory_order_acquire))
            return false;

        buffer_[tail] = item;

        // Release — ensures buffer_[tail] write is visible
        // to consumer before tail_ update is visible
        tail_.store(next, std::memory_order_release);
        return true;
    }

    // Called by consumer thread only
    std::optional<T> pop() {
        const std::size_t head = head_.load(std::memory_order_relaxed);

        // Queue is empty if head == tail
        if (head == tail_.load(std::memory_order_acquire))
            return std::nullopt;

        T item = buffer_[head];

        // Release — ensures buffer_[head] read is complete
        // before head_ update is visible to producer
        head_.store((head + 1) & mask_, std::memory_order_release);
        return item;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    std::size_t size() const {
        const std::size_t tail = tail_.load(std::memory_order_acquire);
        const std::size_t head = head_.load(std::memory_order_acquire);
        return (tail - head) & mask_;
    }

    static constexpr std::size_t capacity() { return Capacity; }

private:
    // ── Cache line padding ────────────────────
    // head_ is written by the consumer and read
    // by the producer. tail_ is written by the
    // producer and read by the consumer. Padding
    // them to separate cache lines prevents false
    // sharing — where two cores invalidate each
    // other's cache line even though they're
    // accessing different variables.

    static constexpr std::size_t cache_line = 64;
    static constexpr std::size_t mask_      = Capacity - 1;

    alignas(cache_line) std::atomic<std::size_t> head_;
    alignas(cache_line) std::atomic<std::size_t> tail_;

    std::array<T, Capacity> buffer_;
};

} // namespace queue