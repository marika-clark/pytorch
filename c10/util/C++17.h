#pragma once
#ifndef C10_UTIL_CPP17_H_
#define C10_UTIL_CPP17_H_

#include <c10/macros/Macros.h>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#if !defined(__clang__) && !defined(_MSC_VER) && defined(__GNUC__) && \
    __GNUC__ < 5
#error \
    "You're trying to build PyTorch with a too old version of GCC. We need GCC 5 or later."
#endif

#if defined(__clang__) && __clang_major__ < 4
#error \
    "You're trying to build PyTorch with a too old version of Clang. We need Clang 4 or later."
#endif

#if (defined(_MSC_VER) && (!defined(_MSVC_LANG) || _MSVC_LANG < 201703L)) || \
    (!defined(_MSC_VER) && __cplusplus < 201703L)
#error You need C++17 to compile PyTorch
#endif

#if defined(_WIN32) && (defined(min) || defined(max))
#error Macro clash with min and max -- define NOMINMAX when compiling your program on Windows
#endif

/*
 * This header adds some polyfills with C++17 functionality
 */

namespace c10 {

// std::is_pod is deprecated in C++20, std::is_standard_layout and
// std::is_trivial are introduced in C++11, std::conjunction has been introduced
// in C++17.
template <typename T>
#if defined(__cpp_lib_logical_traits) && __cpp_lib_logical_traits >= 201510L
using is_pod = std::conjunction<std::is_standard_layout<T>, std::is_trivial<T>>;
#else
using is_pod = std::is_pod<T>;
#endif

template <typename T>
constexpr bool is_pod_v = is_pod<T>::value;

namespace guts {

template <typename Base, typename Child, typename... Args>
typename std::enable_if<
    !std::is_array<Base>::value && !std::is_array<Child>::value &&
        std::is_base_of<Base, Child>::value,
    std::unique_ptr<Base>>::type
make_unique_base(Args&&... args) {
  return std::unique_ptr<Base>(new Child(std::forward<Args>(args)...));
}

#if defined(USE_ROCM)
// rocm doesn't like the C10_HOST_DEVICE
#define CUDA_HOST_DEVICE
#else
#define CUDA_HOST_DEVICE C10_HOST_DEVICE
#endif

#if defined(__cpp_lib_apply) && !defined(__CUDA_ARCH__)

template <class F, class Tuple>
CUDA_HOST_DEVICE inline constexpr decltype(auto) apply(F&& f, Tuple&& t) {
  return std::apply(std::forward<F>(f), std::forward<Tuple>(t));
}

#else

// Implementation from http://en.cppreference.com/w/cpp/utility/apply (but
// modified)
// TODO This is an incomplete implementation of std::apply, not working for
// member functions.
namespace detail {
template <class F, class Tuple, std::size_t... INDEX>
#if defined(_MSC_VER)
// MSVC has a problem with the decltype() return type, but it also doesn't need
// it
C10_HOST_DEVICE constexpr auto apply_impl(
    F&& f,
    Tuple&& t,
    std::index_sequence<INDEX...>)
#else
// GCC/Clang need the decltype() return type
CUDA_HOST_DEVICE constexpr decltype(auto) apply_impl(
    F&& f,
    Tuple&& t,
    std::index_sequence<INDEX...>)
#endif
{
  return std::forward<F>(f)(std::get<INDEX>(std::forward<Tuple>(t))...);
}
} // namespace detail

template <class F, class Tuple>
CUDA_HOST_DEVICE constexpr decltype(auto) apply(F&& f, Tuple&& t) {
  return detail::apply_impl(
      std::forward<F>(f),
      std::forward<Tuple>(t),
      std::make_index_sequence<
          std::tuple_size<std::remove_reference_t<Tuple>>::value>{});
}

#endif

#undef CUDA_HOST_DEVICE

namespace detail {
struct _identity final {
  template <class T>
  using type_identity = T;

  template <class T>
  decltype(auto) operator()(T&& arg) {
    return std::forward<T>(arg);
  }
};

template <class Func, class Enable = void>
struct function_takes_identity_argument : std::false_type {};
template <class Func>
struct function_takes_identity_argument<
    Func,
    std::void_t<decltype(std::declval<Func>()(_identity()))>> : std::true_type {
};
} // namespace detail

// GCC 4.8 doesn't define std::to_string, even though that's in C++11. Let's
// define it.
namespace detail {
class DummyClassForToString final {};
} // namespace detail
} // namespace guts
} // namespace c10
namespace std {
// We use SFINAE to detect if std::to_string exists for a type, but that only
// works if the function name is defined. So let's define a std::to_string for a
// dummy type. If you're getting an error here saying that this overload doesn't
// match your std::to_string() call, then you're calling std::to_string() but
// should be calling c10::guts::to_string().
inline std::string to_string(c10::guts::detail::DummyClassForToString) {
  return "";
}

} // namespace std
namespace c10 {
namespace guts {
namespace detail {

template <class T, class Enable = void>
struct to_string_ final {
  static std::string call(T value) {
    std::ostringstream str;
    str << value;
    return str.str();
  }
};
// If a std::to_string exists, use that instead
template <class T>
struct to_string_<T, void_t<decltype(std::to_string(std::declval<T>()))>>
    final {
  static std::string call(T value) {
    return std::to_string(value);
  }
};
} // namespace detail
template <class T>
inline std::string to_string(T value) {
  return detail::to_string_<T>::call(value);
}

} // namespace guts
} // namespace c10

#endif // C10_UTIL_CPP17_H_
