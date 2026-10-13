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













