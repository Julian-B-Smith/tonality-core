// Set-class identity over masks: Rahn normal order / prime form, Z-relations.
// Ports mts/core/setclass.py. For zero-rooted sorted pc tuples, Rahn's
// comparison is exactly integer comparison of the masks, so the prime form is
// the minimum mask over the 24 zero-rooted images (12 rotations of the set,
// 12 of its inversion) — same derivation as the engine.
#pragma once

#include <array>
#include <optional>

#include "bitmask.hpp"

namespace tonality {

// Pitch classes in Rahn normal order (the set's actual pcs, most compact
// rotation, ties to the lowest starting pc).
constexpr PcList normal_order(Mask mask) {
    if (mask == 0) return PcList{};
    Mask best_relative = kFullMask + 1;  // above any 12-bit value
    int best_start = 0;
    const PcList pcs = pcs_from_mask(mask);
    for (int i = 0; i < pcs.count; ++i) {
        const int pc = pcs.pcs[static_cast<std::size_t>(i)];
        const Mask relative = rotate_mask(mask, -pc);
        if (relative < best_relative) {  // strict: first (lowest) pc wins ties
            best_relative = relative;
            best_start = pc;
        }
    }
    PcList out = pcs_from_mask(best_relative);
    for (int i = 0; i < out.count; ++i) {
        out.pcs[static_cast<std::size_t>(i)] =
            static_cast<std::uint8_t>((best_start + out.pcs[static_cast<std::size_t>(i)]) % 12);
    }
    return out;
}

// Rahn prime form as a mask — the canonical set-class identifier (same integer
// convention as Ian Ring's scale numbers).
constexpr Mask prime_form_mask(Mask mask) {
    if (mask == 0) return 0;
    Mask best = kFullMask;
    const Mask images[2] = {mask, invert_mask(mask)};
    for (const Mask image : images) {
        const PcList pcs = pcs_from_mask(image);
        for (int i = 0; i < pcs.count; ++i) {
            const Mask candidate = rotate_mask(image, -pcs.pcs[static_cast<std::size_t>(i)]);
            if (candidate < best) best = candidate;
        }
    }
    return best;
}

constexpr PcList prime_form(Mask mask) { return pcs_from_mask(prime_form_mask(mask)); }

// Z-partner map over the whole 4096-mask universe: prime forms sharing an
// interval vector (keyed with cardinality, matching the engine — the empty set
// and a singleton share the all-zero vector but are not Z-related). In 12-TET
// the relation is strictly pairwise.
class ZTable {
public:
    ZTable() {
        struct Group {
            int cardinality = 0;
            std::array<int, 6> vector{};
            Mask members[3] = {0, 0, 0};
            int size = 0;
        };
        std::array<Group, 352> groups{};  // 352 set classes in 12-TET
        int group_count = 0;
        std::array<bool, 4096> seen{};
        for (int m = 0; m <= kFullMask; ++m) {
            const Mask prime = prime_form_mask(static_cast<Mask>(m));
            if (seen[prime]) continue;
            seen[prime] = true;
            const int card = cardinality(prime);
            const std::array<int, 6> vec = interval_vector(prime);
            Group* group = nullptr;
            for (int g = 0; g < group_count; ++g) {
                if (groups[static_cast<std::size_t>(g)].cardinality == card &&
                    groups[static_cast<std::size_t>(g)].vector == vec) {
                    group = &groups[static_cast<std::size_t>(g)];
                    break;
                }
            }
            if (group == nullptr) {
                group = &groups[static_cast<std::size_t>(group_count++)];
                group->cardinality = card;
                group->vector = vec;
            }
            if (group->size < 3) group->members[group->size] = prime;
            ++group->size;
        }
        partner_.fill(0);
        has_partner_.fill(false);
        for (int g = 0; g < group_count; ++g) {
            const Group& group = groups[static_cast<std::size_t>(g)];
            if (group.size == 2) {
                partner_[group.members[0]] = group.members[1];
                partner_[group.members[1]] = group.members[0];
                has_partner_[group.members[0]] = true;
                has_partner_[group.members[1]] = true;
            }
        }
    }

    // Prime-form mask of the Z-partner, or nullopt if not Z-related.
    std::optional<Mask> partner(Mask mask) const {
        const Mask prime = prime_form_mask(mask);
        if (!has_partner_[prime]) return std::nullopt;
        return partner_[prime];
    }

private:
    std::array<Mask, 4096> partner_{};
    std::array<bool, 4096> has_partner_{};
};

inline const ZTable& z_table() {
    static const ZTable table;
    return table;
}

inline std::optional<Mask> z_partner_mask(Mask mask) { return z_table().partner(mask); }

}  // namespace tonality
