#include <iostream>
#include <vector>
#include <array>

template<size_t MAX_EVENTS = 1000>
class SparseEventSystem {
private:
    // 1. Fast O(1) lookup to check if an event is already queued (prevents duplicates)
    std::array<bool, MAX_EVENTS> is_queued{};
    
    // 2. Dense, contiguous array storing ONLY the active event IDs
    std::array<int, MAX_EVENTS> active_ids;
    size_t active_count = 0;

public:
    // Fast Notification: O(1)
    inline void notify(int event_id) {
        if (!is_queued[event_id]) {
            is_queued[event_id] = true;
            active_ids[active_count++] = event_id; // Contiguous memory push
        }
    }

    // Fast Consumption: O(K) where K is strictly the number of active events (20-50)
    inline void consume_all() {
        // Snapshot the count to handle any incoming notifications safely
        size_t count = active_count;
        active_count = 0; 

        // Loop runs EXACTLY 20 to 50 times. Perfect CPU branch prediction.
        for (size_t i = 0; i < count; ++i) {
            int event_id = active_ids[i];
            is_queued[event_id] = false; // Reset state

            // Execute logic
            process_event(event_id);
        }
    }

private:
    void process_event(int id) {
        // Your event logic here
    }
}
}
#include <iostream>
#include <cstdint>

class EventNotifier64 {
private:
    // Every bit position (0 to 63) represents an event ID
    uint64_t active_events = 0; 

public:
    // Fast Notification: O(1)
    inline void notify(int event_id) {
        active_events |= (1ULL << event_id);
    }

    // Fast Consumption: O(K) where K is the number of active events (20 to 50)
    inline void consume_all() {
        uint64_t mask = active_events;
        active_events = 0; // Clear immediately to allow new notifications

        while (mask > 0) {
            // Instantly find the trailing zero count (index of the lowest set bit)
            int event_id = __builtin_ctzll(mask);
            
            // Execute event logic
            process_event(event_id);
            
            // Clear the lowest set bit we just processed
            mask &= (mask - 1); 
        }
    }

private:
    void process_event(int id) {
        // Your event logic here
    }
};



#include <iostream>
#include <cstdint>
#include <array>
#include <type_traits>

// ==========================================
// 1. OPTION 1: Ultra-fast Bitmask Strategy
// ==========================================
template <size_t MaxEvents>
class BitmaskEventStrategy {
private:
    uint64_t active_events = 0;

public:
    inline void notify(size_t event_id) {
        active_events |= (1ULL << event_id);
    }

    template <typename Func>
    inline void consume_all(Func&& process_event) {
        uint64_t mask = active_events;
        active_events = 0; // Clear immediately for new notifications

        while (mask > 0) {
            // Instantly find the next set bit using CPU hardware intrinsic
            int event_id = __builtin_ctzll(mask);
            
            process_event(event_id);
            
            // Clear the lowest set bit
            mask &= (mask - 1); 
        }
    }
};

// ==========================================
// 2. OPTION 2: Cache-Friendly Sparse-Dense Strategy
// ==========================================
template <size_t MaxEvents>
class SparseDenseEventStrategy {
private:
    std::array<bool, MaxEvents> is_queued{};
    std::array<size_t, MaxEvents> active_ids;
    size_t active_count = 0;

public:
    inline void notify(size_t event_id) {
        if (!is_queued[event_id]) {
            is_queued[event_id] = true;
            active_ids[active_count++] = event_id;
        }
    }

    template <typename Func>
    inline void consume_all(Func&& process_event) {
        size_t count = active_count;
        active_count = 0;

        for (size_t i = 0; i < count; ++i) {
            size_t event_id = active_ids[i];
            is_queued[event_id] = false; // Reset sparse lookup
            
            process_event(event_id);
        }
    }
};

// ==========================================
// 3. THE UNIFIED GENERIC SYSTEM
// ==========================================
template <size_t MaxEvents>
class GenericEventSystem {
private:
    // Automatically select the fastest data structure at compile-time
    using Strategy = std::conditional_t<
        (MaxEvents <= 64),
        BitmaskEventStrategy<MaxEvents>,
        SparseDenseEventStrategy<MaxEvents>
    >;

    Strategy strategy;

public:
    // O(1) Fast Notification
    inline void notify(size_t event_id) {
        // Enforce safety boundary checks at runtime/debug mode
        if (event_id >= MaxEvents) return; 
        strategy.notify(event_id);
    }

    // High-performance consumption via a zero-cost abstraction lambda
    template <typename Func>
    inline void consume_all(Func&& process_event) {
        strategy.consume_all(std::forward<Func>(process_event));
    }
};

// Dummy processing logic
void my_event_handler(size_t id) {
    std::cout << "Processing event: " << id << "\n";
}

int main() {
    // ----------------------------------------------------
    // Scenario A: Max 32 events. Compiles to Bitmask.
    // ----------------------------------------------------
    GenericEventSystem<32> micro_system;
    
    micro_system.notify(5);
    micro_system.notify(21);
    
    // Consumes using hardware __builtin_ctzll
    micro_system.consume_all([](size_t id) {
        my_event_handler(id);
    });

    // ----------------------------------------------------
    // Scenario B: Max 1000 events. Compiles to Sparse-Dense array.
    // ----------------------------------------------------
    GenericEventSystem<1000> large_system;
    
    large_system.notify(12);
    large_system.notify(850);
    
    // Loops strictly 2 times, skipping empty spaces instantly
    large_system.consume_all([](size_t id) {
        my_event_handler(id);
    });

    return 0;
}




Multithread event


#include <iostream>
#include <cstdint>
#include <array>
#include <atomic>
#include <type_traits>
#include <utility>

// ============================================================================
// 1. OPTION 1: Lock-Free Atomic Bitmask Strategy (MaxEvents <= 64)
// ============================================================================
template <size_t MaxEvents>
class AtomicBitmaskStrategy {
private:
    // Atomic bitmask to support concurrent thread notifications
    std::atomic<uint64_t> active_events{0};

public:
    // Thread-safe Notification: O(1) Lock-free bitwise OR
    inline void notify(size_t event_id) {
        // Release order ensures any data writes prior to notify() are visible to the consumer
        active_events.fetch_or(1ULL << event_id, std::memory_order_release);
    }

    template <typename Func>
    inline void consume_all(Func&& process_event) {
        // Atomic exchange grabs ALL active events and zeroes the bitmask in 1 instruction
        // Acquire order pairs with fetch_or's release to guarantee memory visibility
        uint64_t mask = active_events.exchange(0, std::memory_order_acquire);

        while (mask > 0) {
            int event_id = __builtin_ctzll(mask);
            process_event(event_id);
            mask &= (mask - 1); 
        }
    }
};

// ============================================================================
// 2. OPTION 2: Lock-Free Sparse-Dense Strategy (MaxEvents > 64)
// ============================================================================
template <size_t MaxEvents>
class AtomicSparseDenseStrategy {
private:
    // Atomic flags to prevent duplicate entries in the sequential queue
    std::array<std::atomic<bool>, MaxEvents> is_queued{};
    
    // Contiguous queue array (only written to by producers, read by consumer)
    std::array<size_t, MaxEvents> active_ids;
    
    // Atomic tracking index for where to append next
    std::atomic<size_t> active_count{0};

public:
    inline void notify(size_t event_id) {
        // 1. Atomically check-and-set to ensure only 1 thread queues this exact event ID
        bool expected = false;
        if (is_queued[event_id].compare_exchange_strong(expected, true, 
                                                        std::memory_order_relaxed)) {
            // 2. Fetch-and-add safely reserves a unique sequential slot in our dense array
            size_t slot = active_count.fetch_add(1, std::memory_order_release);
            active_ids[slot] = event_id;
        }
    }

    template <typename Func>
    inline void consume_all(Func&& process_event) {
        // Grabs the current snapshot size and resets the counter to 0 in one atomic step
        size_t count = active_count.exchange(0, std::memory_order_acquire);

        for (size_t i = 0; i < count; ++i) {
            size_t event_id = active_ids[i];
            
            // Allow this event ID to be queued again by producers in the next cycle
            is_queued[event_id].store(false, std::memory_order_relaxed);
            
            process_event(event_id);
        }
    }
};

// ============================================================================
// 3. THE UNIFIED GENERIC SYSTEM
// ============================================================================
template <size_t MaxEvents>
class GenericMultiThreadedEventSystem {
private:
    using Strategy = std::conditional_t<
        (MaxEvents <= 64),
        AtomicBitmaskStrategy<MaxEvents>,
        AtomicSparseDenseStrategy<MaxEvents>
    >;

    Strategy strategy;

public:
    // Thread-safe, non-blocking notification
    inline void notify(size_t event_id) {
        if (event_id >= MaxEvents) [[unlikely]] return; 
        strategy.notify(event_id);
    }

    // Thread-safe consumption (Meant to be invoked by 1 designated consumer thread)
    template <typename Func>
    inline void consume_all(Func&& process_event) {
        strategy.consume_all(std::forward<Func>(process_event));
    }
};













