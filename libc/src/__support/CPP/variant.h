//===-- Standalone implementation of std::variant ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_VARIANT_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_VARIANT_H

#include "src/__support/CPP/type_traits.h"
#include "src/__support/CPP/utility.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// Simple variant implementation for POD types.
// This implementation assumes all types are trivially copyable and destructible.
// Supports common cases of 2 and 3 types.

// Forward declaration
template <typename... Types>
class variant;

// Specialization for 2 types
template <typename T1, typename T2>
class variant<T1, T2> {
private:
  static constexpr int storage_size = (sizeof(T1) > sizeof(T2)) ? sizeof(T1) : sizeof(T2);
  static constexpr int storage_align = (alignof(T1) > alignof(T2)) ? alignof(T1) : alignof(T2);
  
  alignas(storage_align) char storage_[storage_size];
  int index_;

public:
  // Default constructor - initializes with first type default-constructed
  LIBC_INLINE constexpr variant() : index_(0) {
    *reinterpret_cast<T1*>(storage_) = T1{};
  }

  // Constructor from T1
  LIBC_INLINE constexpr variant(const T1& value) : index_(0) {
    *reinterpret_cast<T1*>(storage_) = value;
  }

  // Constructor from T2
  LIBC_INLINE constexpr variant(const T2& value) : index_(1) {
    *reinterpret_cast<T2*>(storage_) = value;
  }

  // Copy constructor
  LIBC_INLINE constexpr variant(const variant& other) : index_(other.index_) {
    if (index_ == 0) {
      *reinterpret_cast<T1*>(storage_) = *reinterpret_cast<const T1*>(other.storage_);
    } else {
      *reinterpret_cast<T2*>(storage_) = *reinterpret_cast<const T2*>(other.storage_);
    }
  }

  // Move constructor
  LIBC_INLINE constexpr variant(variant&& other) : index_(other.index_) {
    if (index_ == 0) {
      *reinterpret_cast<T1*>(storage_) = move(*reinterpret_cast<T1*>(other.storage_));
    } else {
      *reinterpret_cast<T2*>(storage_) = move(*reinterpret_cast<T2*>(other.storage_));
    }
  }

  // Assignment operators
  LIBC_INLINE constexpr variant& operator=(const variant& other) {
    if (this != &other) {
      index_ = other.index_;
      if (index_ == 0) {
        *reinterpret_cast<T1*>(storage_) = *reinterpret_cast<const T1*>(other.storage_);
      } else {
        *reinterpret_cast<T2*>(storage_) = *reinterpret_cast<const T2*>(other.storage_);
      }
    }
    return *this;
  }

  LIBC_INLINE constexpr variant& operator=(variant&& other) {
    if (this != &other) {
      index_ = other.index_;
      if (index_ == 0) {
        *reinterpret_cast<T1*>(storage_) = move(*reinterpret_cast<T1*>(other.storage_));
      } else {
        *reinterpret_cast<T2*>(storage_) = move(*reinterpret_cast<T2*>(other.storage_));
      }
    }
    return *this;
  }

  // Get the index of the currently active type
  LIBC_INLINE constexpr int index() const { return index_; }

  // Type-safe access to stored value
  template <typename T>
  LIBC_INLINE constexpr T& get() & {
    if constexpr (is_same_v<T, T1>) {
      return *reinterpret_cast<T1*>(storage_);
    } else if constexpr (is_same_v<T, T2>) {
      return *reinterpret_cast<T2*>(storage_);
    }
  }

  template <typename T>
  LIBC_INLINE constexpr const T& get() const & {
    if constexpr (is_same_v<T, T1>) {
      return *reinterpret_cast<const T1*>(storage_);
    } else if constexpr (is_same_v<T, T2>) {
      return *reinterpret_cast<const T2*>(storage_);
    }
  }
};

// Specialization for 3 types
template <typename T1, typename T2, typename T3>
class variant<T1, T2, T3> {
private:
  static constexpr int storage_size = (sizeof(T1) > sizeof(T2)) ? 
    ((sizeof(T1) > sizeof(T3)) ? sizeof(T1) : sizeof(T3)) :
    ((sizeof(T2) > sizeof(T3)) ? sizeof(T2) : sizeof(T3));
    
  static constexpr int storage_align = (alignof(T1) > alignof(T2)) ? 
    ((alignof(T1) > alignof(T3)) ? alignof(T1) : alignof(T3)) :
    ((alignof(T2) > alignof(T3)) ? alignof(T2) : alignof(T3));
  
  alignas(storage_align) char storage_[storage_size];
  int index_;

public:
  // Default constructor
  LIBC_INLINE constexpr variant() : index_(0) {
    *reinterpret_cast<T1*>(storage_) = T1{};
  }

  // Constructors
  LIBC_INLINE constexpr variant(const T1& value) : index_(0) {
    *reinterpret_cast<T1*>(storage_) = value;
  }

  LIBC_INLINE constexpr variant(const T2& value) : index_(1) {
    *reinterpret_cast<T2*>(storage_) = value;
  }

  LIBC_INLINE constexpr variant(const T3& value) : index_(2) {
    *reinterpret_cast<T3*>(storage_) = value;
  }

  // Copy constructor
  LIBC_INLINE constexpr variant(const variant& other) : index_(other.index_) {
    if (index_ == 0) {
      *reinterpret_cast<T1*>(storage_) = *reinterpret_cast<const T1*>(other.storage_);
    } else if (index_ == 1) {
      *reinterpret_cast<T2*>(storage_) = *reinterpret_cast<const T2*>(other.storage_);
    } else {
      *reinterpret_cast<T3*>(storage_) = *reinterpret_cast<const T3*>(other.storage_);
    }
  }

  // Move constructor
  LIBC_INLINE constexpr variant(variant&& other) : index_(other.index_) {
    if (index_ == 0) {
      *reinterpret_cast<T1*>(storage_) = move(*reinterpret_cast<T1*>(other.storage_));
    } else if (index_ == 1) {
      *reinterpret_cast<T2*>(storage_) = move(*reinterpret_cast<T2*>(other.storage_));
    } else {
      *reinterpret_cast<T3*>(storage_) = move(*reinterpret_cast<T3*>(other.storage_));
    }
  }

  // Assignment operators
  LIBC_INLINE constexpr variant& operator=(const variant& other) {
    if (this != &other) {
      index_ = other.index_;
      if (index_ == 0) {
        *reinterpret_cast<T1*>(storage_) = *reinterpret_cast<const T1*>(other.storage_);
      } else if (index_ == 1) {
        *reinterpret_cast<T2*>(storage_) = *reinterpret_cast<const T2*>(other.storage_);
      } else {
        *reinterpret_cast<T3*>(storage_) = *reinterpret_cast<const T3*>(other.storage_);
      }
    }
    return *this;
  }

  LIBC_INLINE constexpr variant& operator=(variant&& other) {
    if (this != &other) {
      index_ = other.index_;
      if (index_ == 0) {
        *reinterpret_cast<T1*>(storage_) = move(*reinterpret_cast<T1*>(other.storage_));
      } else if (index_ == 1) {
        *reinterpret_cast<T2*>(storage_) = move(*reinterpret_cast<T2*>(other.storage_));
      } else {
        *reinterpret_cast<T3*>(storage_) = move(*reinterpret_cast<T3*>(other.storage_));
      }
    }
    return *this;
  }

  // Get the index of the currently active type
  LIBC_INLINE constexpr int index() const { return index_; }

  // Type-safe access to stored value
  template <typename T>
  LIBC_INLINE constexpr T& get() & {
    if constexpr (is_same_v<T, T1>) {
      return *reinterpret_cast<T1*>(storage_);
    } else if constexpr (is_same_v<T, T2>) {
      return *reinterpret_cast<T2*>(storage_);
    } else if constexpr (is_same_v<T, T3>) {
      return *reinterpret_cast<T3*>(storage_);
    }
  }

  template <typename T>
  LIBC_INLINE constexpr const T& get() const & {
    if constexpr (is_same_v<T, T1>) {
      return *reinterpret_cast<const T1*>(storage_);
    } else if constexpr (is_same_v<T, T2>) {
      return *reinterpret_cast<const T2*>(storage_);
    } else if constexpr (is_same_v<T, T3>) {
      return *reinterpret_cast<const T3*>(storage_);
    }
  }
};

// Free functions for type-safe access
template <typename T, typename T1, typename T2>
LIBC_INLINE constexpr T& get(variant<T1, T2>& v) {
  return v.template get<T>();
}

template <typename T, typename T1, typename T2>
LIBC_INLINE constexpr const T& get(const variant<T1, T2>& v) {
  return v.template get<T>();
}

template <typename T, typename T1, typename T2, typename T3>
LIBC_INLINE constexpr T& get(variant<T1, T2, T3>& v) {
  return v.template get<T>();
}

template <typename T, typename T1, typename T2, typename T3>
LIBC_INLINE constexpr const T& get(const variant<T1, T2, T3>& v) {
  return v.template get<T>();
}

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_VARIANT_H