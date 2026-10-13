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










