#include <array>
#include <utility>

template <typename HashFunc, typename T>
constexpr std::size_t bucket_index(std::size_t num_buckets, const T &val) {
    return HashFunc()(val) % num_buckets;
}

namespace detail {

    template <typename HashFunc, std::size_t NumBuckets, typename T, std::size_t... I>
    constexpr auto make_bucket_index_table_(const std::array<T, sizeof...(I)> &values, std::index_sequence<I...>) {
        return std::array<std::size_t, sizeof...(I)>{ bucket_index<HashFunc>(NumBuckets, values[I])... };
    }    

} // namespace detail

template <typename HashFunc, std::size_t NumBuckets, typename T, std::size_t N>
constexpr auto make_bucket_index_table(const std::array<T, N> &values) {
    return detail::make_bucket_index_table_<HashFunc, NumBuckets>(values, std::make_index_sequence<N>());
}

template <std::size_t N>
constexpr auto bucket_offset(const std::array<std::size_t, N> &bucket_index_table, std::size_t index) {
    std::size_t actual_index = 0;
    for (std::size_t i = 0; i < N; ++i) {
        if (bucket_index_table[i] < index) ++actual_index;
    }
    return actual_index;
}

namespace detail {

    template <std::size_t N, std::size_t... I>
    constexpr auto make_bucket_offset_table_(const std::array<std::size_t, N> &bucket_index_table, std::index_sequence<I...>) {
        return std::array<std::size_t, sizeof...(I)>{ bucket_offset(bucket_index_table, I)... };
    }

} // namespace detail

template <std::size_t NumBuckets, std::size_t N>
constexpr auto make_bucket_offset_table(const std::array<std::size_t, N> &bucket_index_table) {
    return detail::make_bucket_offset_table_(bucket_index_table, std::make_index_sequence<NumBuckets>());
}

template <std::size_t NumBuckets, std::size_t N>
constexpr auto actual_index(const std::array<std::size_t, N> &bucket_index_table, const std::array<std::size_t, NumBuckets> &bucket_offset_table, std::size_t index) {
    const std::size_t bucket_index = bucket_index_table[index];
    std::size_t offset = 0;
    for (std::size_t i = 0; i < index; ++i) {
        if (bucket_index_table[i] == bucket_index) ++offset;
    }
    return bucket_offset_table[bucket_index] + offset;
}

namespace detail {

    template <std::size_t NumBuckets, std::size_t... I>
    constexpr auto make_actual_index_table_(const std::array<std::size_t, sizeof...(I)> &bucket_index_table, 
                                            const std::array<std::size_t, NumBuckets> &bucket_offset_table,
                                            std::index_sequence<I...>) 
    {
        return std::array<std::size_t, sizeof...(I)>{ actual_index(bucket_index_table, bucket_offset_table, I)... };
    }

} // namespace detail

template <std::size_t NumBuckets, std::size_t N>
constexpr auto make_actual_index_table(const std::array<std::size_t, N> &bucket_index_table, const std::array<std::size_t, NumBuckets> &bucket_offset_table) {
    return detail::make_actual_index_table_(bucket_index_table, bucket_offset_table, std::make_index_sequence<N>());
}

template <std::size_t N>
constexpr std::size_t reverse_index(const std::array<std::size_t, N> &actual_index_table, std::size_t index) {
    std::size_t idx = 0;
    for (std::size_t i = 0; i < N; ++i) {
        if (actual_index_table[i] == index) return i;
    }
    return idx;
}

namespace detail {

    template <std::size_t... I>
    constexpr auto make_reverse_index_table_(const std::array<std::size_t, sizeof...(I)> &actual_index_table, 
                                             std::index_sequence<I...>) 
    {
        return std::array<std::size_t, sizeof...(I)>{ reverse_index(actual_index_table, I)... };
    }

} // namespace detail

template <std::size_t N>
constexpr auto make_reverse_index_table(const std::array<std::size_t, N> &actual_index_table) {
    return detail::make_reverse_index_table_(actual_index_table, std::make_index_sequence<N>());
}

namespace detail {

    template <typename T, std::size_t... I>
    constexpr auto make_hash_set_(const std::array<T, sizeof...(I)> &values, 
                                  const std::array<std::size_t, sizeof...(I)> &reverse_index_table,
                                  std::index_sequence<I...>) 
    {
        return std::array<T, sizeof...(I)>{ values[reverse_index_table[I]]... };
    }

} // namespace detail

template <typename T, std::size_t N>
constexpr auto make_hash_set(const std::array<T, N> &values, const std::array<std::size_t, N> &reverse_index_table) {
    return detail::make_hash_set_(values, reverse_index_table, std::make_index_sequence<N>());
}

template <std::size_t NumBuckets, typename HashFunc, typename T, std::size_t N>
class constexpr_hash_set {
    static_assert(NumBuckets > 2, "NumBuckets must be > 2");

    std::array<std::size_t, NumBuckets> m_bucket_offset_table;
    std::array<T, N> m_hash_map;

public:

    constexpr constexpr_hash_set(const std::array<T, N> &values)
    :m_bucket_offset_table{ make_bucket_offset_table<NumBuckets>(make_bucket_index_table<HashFunc, NumBuckets>(values)) },
     m_hash_map{ make_hash_set(values, make_reverse_index_table(make_actual_index_table(make_bucket_index_table<HashFunc, NumBuckets>(values), m_bucket_offset_table))) }
    {}

    constexpr bool contains(const T &value) const {
        std::size_t bucket = bucket_index<HashFunc>(NumBuckets, value);
        std::size_t bucket_i = m_bucket_offset_table[bucket];
        std::size_t bucket_end = (bucket == N-1) ? N : m_bucket_offset_table[bucket+1];
        for (; bucket_i < bucket_end; ++bucket_i) {
            if (value == m_hash_map[bucket_i]) return true;
        }

        return false;
    }

};

template <std::size_t NumBuckets, typename HashFunc, typename T, std::size_t N>
constexpr auto make_constexpr_hash_set(const std::array<T, N> &values) {
    return constexpr_hash_set<NumBuckets, HashFunc, T, N>{ values };
}

struct int_hash {

    constexpr std::size_t operator()(int val) const {
        return static_cast<std::size_t>(val);
    }

};

int main(int argc, char** argv) {
    constexpr std::array<int, 5> values = { 33, 23, 532, 32, 10 };
    constexpr std::size_t num_buckets = 3;

    constexpr auto set = make_constexpr_hash_set<num_buckets, int_hash>(values);
    return set.contains(argc);
} 
