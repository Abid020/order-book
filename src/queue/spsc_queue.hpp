#pragma once

#include <atomic>
#include <array>
#include <optional>
#include <cstddef>

namespace queue {

template<typename T, std::size_t Capacity>
class SpscQueue {
    static_assert((Capacity & (Capacity - 1)) == 0,
        "Capacity must be a power of two");

public:
    SpscQueue() : m_head(0), m_tail(0) {}

    // Called by producer (parser) thread only
    bool push(const T& item) {
        const std::size_t tail = m_tail.load(std::memory_order_relaxed);
        const std::size_t next = (tail + 1) & m_mask;

        // Queue is full if next write position == head
        if (next == m_head.load(std::memory_order_acquire))
            return false;

        m_buffer[tail] = item;

        // Release — ensures m_buffer[tail] write is visible
        // to consumer before m_tail update is visible
        m_tail.store(next, std::memory_order_release);
        return true;
    }

    // Called by consumer (order-book) thread only
    std::optional<T> pop() {
        const std::size_t head = m_head.load(std::memory_order_relaxed);

        // Queue is empty if head == tail
        if (head == m_tail.load(std::memory_order_acquire))
            return std::nullopt;

        T item = m_buffer[head];

        // Release — ensures m_buffer[head] read is complete
        // before m_head update is visible to producer
        m_head.store((head + 1) & m_mask, std::memory_order_release);
        return item;
    }

    bool empty() const {
        return m_head.load(std::memory_order_acquire) ==
               m_tail.load(std::memory_order_acquire);
    }

    std::size_t size() const {
        const std::size_t tail = m_tail.load(std::memory_order_acquire);
        const std::size_t head = m_head.load(std::memory_order_acquire);
        return (tail - head) & m_mask;
    }

    static constexpr std::size_t capacity() { return Capacity; }

private:
    // ── Cache line padding ────────────────────
    // m_head is written by the consumer and read
    // by the producer. m_tail is written by the
    // producer and read by the consumer. Padding
    // them to separate cache lines prevents false
    // sharing — where two cores invalidate each
    // other's cache line even though they're
    // accessing different variables.

    static constexpr std::size_t cache_line = 64;
    static constexpr std::size_t m_mask      = Capacity - 1;

    alignas(cache_line) std::atomic<std::size_t> m_head;
    alignas(cache_line) std::atomic<std::size_t> m_tail;

    std::array<T, Capacity> m_buffer;
};

} // namespace queue