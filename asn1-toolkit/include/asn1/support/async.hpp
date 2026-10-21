#pragma once

#include <future>
#include <algorithm>
#include <cstddef>
#include <vector>
#include <system_error>
#include <utility>

namespace asn1 {
namespace detail {

// Keep a copy of work until launch succeeds. A failed thread launch must not
// consume the only callable; deferred execution preserves module merge order.
template <typename F, typename Launcher>
auto async_or_deferred(F work, Launcher launch) -> std::future<decltype(work())> {
  try {
    return launch(work);
  } catch (const std::system_error&) {
    return std::async(std::launch::deferred, std::move(work));
  }
}

template <typename F>
auto async_or_deferred(F work) -> std::future<decltype(work())> {
  return async_or_deferred(std::move(work), [](auto task) {
    return std::async(std::launch::async, std::move(task));
  });
}

// Bounded batches retain input order, including deferred launch fallback.
// Merge only after every task in the batch has completed.
template <typename Work, typename Merge>
void bounded_ordered(std::size_t count, std::size_t limit, Work work, Merge merge) {
  limit = std::max<std::size_t>(1, limit);
  using R = decltype(work(std::size_t{}));
  for (std::size_t first = 0; first < count;) {
    const auto size = std::min(limit, count - first);
    std::vector<std::future<R>> pending;
    pending.reserve(size);
    for (std::size_t i = 0; i < size; ++i) {
      pending.emplace_back(async_or_deferred([&, index = first + i] { return work(index); }));
    }
    std::vector<R> results;
    results.reserve(size);
    for (auto& future : pending) results.emplace_back(future.get());
    for (auto& result : results) merge(std::move(result));
    first += size;
  }
}

}  // namespace detail
}  // namespace asn1
