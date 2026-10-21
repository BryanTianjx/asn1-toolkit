#pragma once

#include <future>
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

}  // namespace detail
}  // namespace asn1
