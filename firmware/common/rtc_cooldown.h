/**
 * rtc_cooldown.h
 * Rule-engine cooldown table backed by ESP32 RTC slow memory.
 *
 * ESP32 deep sleep powers down most peripherals and wipes regular SRAM, but
 * the ~8 KB RTC slow-memory region survives until a hard reset. Storing the
 * rule cooldown table here lets the device sleep between sensor reads
 * without re-firing a rule the instant it wakes up. A heap-backed std::map
 * would forget its keys on every wake and turn cooldown into a no-op.
 *
 * Index strategy: linear scan over a small fixed array. With MAX_COOLDOWNS
 * = 32 and ~10 rules typical, the cost is negligible compared to the cost
 * of a single MQTT publish or sensor read.
 */
#pragma once

#include "esp_attr.h"
#include <cstdint>
#include <cstddef>

namespace planthub {

struct CooldownEntry {
    int rule_id;
    uint32_t last_fired_epoch;
};

static constexpr size_t MAX_COOLDOWNS = 32;

// RTC slow memory survives deep sleep but NOT a power cycle / hard reset.
// Initial value on first boot is zero-filled, which means rule_id=0 — we
// treat slot.rule_id == 0 as "empty" so 0 must never be a valid local_id.
// The backend assigns local_ids starting at 1 (see AutomationRuleService
// .assignLocalIdIfDeviceBound), so this invariant holds.
inline RTC_DATA_ATTR CooldownEntry cooldown_table[MAX_COOLDOWNS] = {};

/**
 * Returns the epoch-seconds timestamp at which the given rule last fired,
 * or 0 if the rule is not tracked. Callers compare against the current
 * epoch to decide whether the cooldown window has elapsed.
 */
inline uint32_t getCooldownLastFired(int rule_id) {
    if (rule_id <= 0) return 0;
    for (size_t i = 0; i < MAX_COOLDOWNS; i++) {
        if (cooldown_table[i].rule_id == rule_id) {
            return cooldown_table[i].last_fired_epoch;
        }
    }
    return 0;
}

/**
 * Records that the given rule fired at `epoch`. Updates the existing slot
 * if the rule is already tracked; otherwise occupies the first empty slot.
 * If all slots are full, overwrites the slot with the oldest timestamp —
 * losing cooldown state on a rarely-firing rule is preferable to letting a
 * frequently-firing rule double-fire.
 */
inline void setCooldownLastFired(int rule_id, uint32_t epoch) {
    if (rule_id <= 0) return;
    int empty_slot = -1;
    int oldest_slot = 0;
    uint32_t oldest_epoch = cooldown_table[0].last_fired_epoch;
    for (size_t i = 0; i < MAX_COOLDOWNS; i++) {
        if (cooldown_table[i].rule_id == rule_id) {
            cooldown_table[i].last_fired_epoch = epoch;
            return;
        }
        if (cooldown_table[i].rule_id == 0 && empty_slot < 0) {
            empty_slot = (int) i;
        }
        if (cooldown_table[i].last_fired_epoch < oldest_epoch) {
            oldest_epoch = cooldown_table[i].last_fired_epoch;
            oldest_slot = (int) i;
        }
    }
    int target = empty_slot >= 0 ? empty_slot : oldest_slot;
    cooldown_table[target].rule_id = rule_id;
    cooldown_table[target].last_fired_epoch = epoch;
}

} // namespace planthub
