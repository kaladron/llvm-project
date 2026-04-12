// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP_ARRAY
#define _LIBCPP_ARRAY

/*
    array synopsis

namespace std
{
template <class T, size_t N >
struct array
{
    // types:
    using value_type             = T;
    using pointer                = T*;
    using const_pointer          = const T*;
    using reference              = T&;
    using const_reference        = const T&;
    using size_type              = size_t;
    using difference_type        = ptrdiff_t;
    using iterator               = implementation-defined;
    using const_iterator         = implementation-defined;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    // No explicit construct/copy/destroy for aggregate type
    void fill(const T& u);                                      // constexpr in C++20
    void swap(array& a) noexcept(is_nothrow_swappable_v<T>);    // constexpr in C++20

    // iterators:
    iterator begin() noexcept;                                  // constexpr in C++17
    const_iterator begin() const noexcept;                      // constexpr in C++17
    iterator end() noexcept;                                    // constexpr in C++17
    const_iterator end() const noexcept;                        // constexpr in C++17

    reverse_iterator rbegin() noexcept;                         // constexpr in C++17
    const_reverse_iterator rbegin() const noexcept;             // constexpr in C++17
    reverse_iterator rend() noexcept;                           // constexpr in C++17
    const_reverse_iterator rend() const noexcept;               // constexpr in C++17

    const_iterator cbegin() const noexcept;                     // constexpr in C++17
    const_iterator cend() const noexcept;                       // constexpr in C++17
    const_reverse_iterator crbegin() const noexcept;            // constexpr in C++17
    const_reverse_iterator crend() const noexcept;              // constexpr in C++17

    // capacity:
    constexpr size_type size() const noexcept;
    constexpr size_type max_size() const noexcept;
    constexpr bool empty() const noexcept;

    // element access:
    reference operator[](size_type n);                          // constexpr in C++17
    const_reference operator[](size_type n) const;              // constexpr in C++14
    reference at(size_type n);                                  // constexpr in C++17
    const_reference at(size_type n) const;                      // constexpr in C++14

    reference front();                                          // constexpr in C++17
    const_reference front() const;                              // constexpr in C++14
    reference back();                                           // constexpr in C++17
    const_reference back() const;                               // constexpr in C++14

    T* data() noexcept;                                         // constexpr in C++17
    const T* data() const noexcept;                             // constexpr in C++17
};

template <class T, class... U>
  array(T, U...) -> array<T, 1 + sizeof...(U)>;                 // C++17

template <class T, size_t N>
  bool operator==(const array<T,N>& x, const array<T,N>& y);    // constexpr in C++20
template <class T, size_t N>
  bool operator!=(const array<T,N>& x, const array<T,N>& y);    // removed in C++20
template <class T, size_t N>
  bool operator<(const array<T,N>& x, const array<T,N>& y);     // removed in C++20
template <class T, size_t N>
  bool operator>(const array<T,N>& x, const array<T,N>& y);     // removed in C++20
template <class T, size_t N>
  bool operator<=(const array<T,N>& x, const array<T,N>& y);    // removed in C++20
template <class T, size_t N>
  bool operator>=(const array<T,N>& x, const array<T,N>& y);    // removed in C++20
template<class T, size_t N>
  constexpr synth-three-way-result<T>
    operator<=>(const array<T, N>& x, const array<T, N>& y);    // since C++20

template <class T, size_t N >
  void swap(array<T,N>& x, array<T,N>& y) noexcept(noexcept(x.swap(y))); // constexpr in C++20

template <class T, size_t N>
  constexpr array<remove_cv_t<T>, N> to_array(T (&a)[N]);  // C++20
template <class T, size_t N>
  constexpr array<remove_cv_t<T>, N> to_array(T (&&a)[N]); // C++20

template <class T> struct tuple_size;
template <size_t I, class T> struct tuple_element;
template <class T, size_t N> struct tuple_size<array<T, N>>;
template <size_t I, class T, size_t N> struct tuple_element<I, array<T, N>>;
template <size_t I, class T, size_t N> T& get(array<T, N>&) noexcept;               // constexpr in C++14
template <size_t I, class T, size_t N> const T& get(const array<T, N>&) noexcept;   // constexpr in C++14
template <size_t I, class T, size_t N> T&& get(array<T, N>&&) noexcept;             // constexpr in C++14
template <size_t I, class T, size_t N> const T&& get(const array<T, N>&&) noexcept; // constexpr in C++14

}  // std

*/

#include "src/__support/CPP/type_traits.h"
#include "src/__support/CPP/utility.h"
#include "src/__support/CPP/iterator.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include <stddef.h>


namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// Minimal implementations of needed algorithms
template <class InputIt1, class InputIt2>
constexpr bool equal(InputIt1 first1, InputIt1 last1, InputIt2 first2) {
  for (; first1 != last1; ++first1, ++first2) {
    if (!(*first1 == *first2)) {
      return false;
    }
  }
  return true;
}

template <class OutputIt, class Size, class T>
constexpr OutputIt fill_n(OutputIt first, Size n, const T& value) {
  for (Size i = 0; i < n; ++first, ++i) {
    *first = value;
  }
  return first;
}

template <class InputIt1, class InputIt2>
constexpr bool lexicographical_compare(InputIt1 first1, InputIt1 last1,
                                       InputIt2 first2, InputIt2 last2) {
  for (; (first1 != last1) && (first2 != last2); ++first1, ++first2) {
    if (*first1 < *first2)
      return true;
    if (*first2 < *first1)
      return false;
  }
  return (first1 == last1) && (first2 != last2);
}

template <class ForwardIt1, class ForwardIt2>
constexpr ForwardIt2 swap_ranges(ForwardIt1 first1, ForwardIt1 last1, ForwardIt2 first2) {
  for (; first1 != last1; ++first1, ++first2) {
    auto tmp = *first1;
    *first1 = *first2;
    *first2 = tmp;
  }
  return first2;
}


template <class _Tp, size_t _Size>
struct array {
  // types:
  using value_type             = _Tp;
  using reference              = value_type&;
  using const_reference        = const value_type&;
  using pointer                = value_type*;
  using const_pointer          = const value_type*;
  using iterator               = pointer;
  using const_iterator         = const_pointer;
  using size_type              = size_t;
  using difference_type        = ptrdiff_t;
  using reverse_iterator       = cpp::reverse_iterator<iterator>;
  using const_reverse_iterator = cpp::reverse_iterator<const_iterator>;



  _Tp __elems_[_Size];

  // No explicit construct/copy/destroy for aggregate type
  constexpr void fill(const value_type& __u) {
    fill_n(data(), _Size, __u);
  }

  constexpr void swap(array& __a) {
    swap_ranges(data(), data() + _Size, __a.data());
  }

  // iterators:
  constexpr iterator begin() noexcept {
    return data();
  }
  constexpr const_iterator begin() const noexcept {
    return data();
  }

  constexpr iterator end() noexcept {
    return data() + _Size;
  }
  constexpr const_iterator end() const noexcept {
    return data() + _Size;
  }


  constexpr reverse_iterator rbegin() noexcept {
    return reverse_iterator(end());
  }
  constexpr const_reverse_iterator rbegin() const noexcept {
    return const_reverse_iterator(end());
  }
  constexpr reverse_iterator rend() noexcept {
    return reverse_iterator(begin());
  }
  constexpr const_reverse_iterator rend() const noexcept {
    return const_reverse_iterator(begin());
  }

  constexpr const_iterator cbegin() const noexcept {
    return begin();
  }
  constexpr const_iterator cend() const noexcept {
    return end();
  }
  constexpr const_reverse_iterator crbegin() const noexcept {
    return rbegin();
  }
  constexpr const_reverse_iterator crend() const noexcept {
    return rend();
  }

  // capacity:
  constexpr size_type size() const noexcept { return _Size; }
  constexpr size_type max_size() const noexcept { return _Size; }
  constexpr bool empty() const noexcept { return _Size == 0; }

  // element access:
  constexpr reference operator[](size_type __n) noexcept {
    return __elems_[__n];
  }
  constexpr const_reference operator[](size_type __n) const noexcept {
    return __elems_[__n];
  }

  constexpr reference front() noexcept {
    return __elems_[0];
  }
  constexpr const_reference front() const noexcept {
    return __elems_[0];
  }
  constexpr reference back() noexcept {
    return __elems_[_Size - 1];
  }
  constexpr const_reference back() const noexcept {
    return __elems_[_Size - 1];
  }

  constexpr value_type* data() noexcept {
    return __elems_;
  }
  constexpr const value_type* data() const noexcept {
    return __elems_;
  }

};



template <class _Tp, size_t _Size>
constexpr bool operator==(const array<_Tp, _Size>& __x, const array<_Tp, _Size>& __y) {
  return equal(__x.begin(), __x.end(), __y.begin());
}

template <class _Tp, size_t _Size>
constexpr bool operator!=(const array<_Tp, _Size>& __x, const array<_Tp, _Size>& __y) {
  return !(__x == __y);
}

template <class _Tp, size_t _Size>
constexpr bool operator<(const array<_Tp, _Size>& __x, const array<_Tp, _Size>& __y) {
  return lexicographical_compare(__x.begin(), __x.end(), __y.begin(), __y.end());
}

template <class _Tp, size_t _Size>
constexpr bool operator>(const array<_Tp, _Size>& __x, const array<_Tp, _Size>& __y) {
  return __y < __x;
}

template <class _Tp, size_t _Size>
constexpr bool operator<=(const array<_Tp, _Size>& __x, const array<_Tp, _Size>& __y) {
  return !(__y < __x);
}

template <class _Tp, size_t _Size>
constexpr bool operator>=(const array<_Tp, _Size>& __x, const array<_Tp, _Size>& __y) {
  return !(__x < __y);
}

template <class _Tp, size_t _Size>
constexpr void swap(array<_Tp, _Size>& __x, array<_Tp, _Size>& __y) {
  __x.swap(__y);
}




} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // _LIBCPP_ARRAY
