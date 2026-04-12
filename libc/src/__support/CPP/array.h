//===-- A self contained equivalent of std::array ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_ARRAY_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_ARRAY_H

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

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_ARRAY_H
