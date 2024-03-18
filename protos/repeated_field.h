// Protocol Buffers - Google's data interchange format
// Copyright 2023 Google LLC.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google LLC nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef UPB_PROTOS_REPEATED_FIELD_H_
#define UPB_PROTOS_REPEATED_FIELD_H_

#include <cstddef>
#include <iterator>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "protos/protos.h"
#include "protos/protos_traits.h"
#include "protos/repeated_field_iterator.h"
#include "hpb/base/string_view.h"
#include "hpb/collections/array.h"
#include "hpb/collections/internal/array.h"
#include "hpb/mem/arena.h"
#include "hpb/message/copy.h"

// Must be last:
#include "hpb/port/def.inc"

namespace protos {

namespace internal {

// Shared implementation of repeated fields for absl::string_view and
// message types for mutable and immutable variants.
//
// Immutable (const accessor), constructs this class with a nullptr hpb_Array*
// when the underlying array in the message is empty.
//
// Mutable accessors on the other hand, will allocate a new empty non-null
// hpb_Array* for the message when the RepeatedFieldProxy is constructed.
template <class T>
class RepeatedFieldProxyBase {
  using Array = add_const_if_T_is_const<T, hpb_Array>;

 public:
  explicit RepeatedFieldProxyBase(Array* arr, hpb_Arena* arena)
      : arr_(arr), arena_(arena) {}

  size_t size() const { return arr_ != nullptr ? hpb_Array_Size(arr_) : 0; }

  bool empty() const { return size() == 0; }

 protected:
  // Returns hpb_Array message member.
  inline hpb_Message* GetMessage(size_t n) const;

  Array* arr_;
  hpb_Arena* arena_;
};

template <class T>
hpb_Message* RepeatedFieldProxyBase<T>::GetMessage(size_t n) const {
  auto** messages =
      static_cast<hpb_Message**>(hpb_Array_MutableDataPtr(this->arr_));
  return messages[n];
}

template <class T>
class RepeatedFieldProxyMutableBase : public RepeatedFieldProxyBase<T> {
 public:
  RepeatedFieldProxyMutableBase(hpb_Array* arr, hpb_Arena* arena)
      : RepeatedFieldProxyBase<T>(arr, arena) {}

  void clear() { hpb_Array_Resize(this->arr_, 0, this->arena_); }
};

// RepeatedField proxy for repeated messages.
template <class T>
class RepeatedFieldProxy
    : public std::conditional_t<std::is_const_v<T>, RepeatedFieldProxyBase<T>,
                                RepeatedFieldProxyMutableBase<T>> {
  static_assert(!std::is_same_v<T, absl::string_view>, "");
  static_assert(!std::is_same_v<T, const absl::string_view>, "");
  static_assert(!std::is_arithmetic_v<T>, "");
  static constexpr bool kIsConst = std::is_const_v<T>;

 public:
  explicit RepeatedFieldProxy(const hpb_Array* arr, hpb_Arena* arena)
      : RepeatedFieldProxyBase<T>(arr, arena) {}
  RepeatedFieldProxy(hpb_Array* arr, hpb_Arena* arena)
      : RepeatedFieldProxyMutableBase<T>(arr, arena) {}
  // Constructor used by ::protos::Ptr.
  RepeatedFieldProxy(const RepeatedFieldProxy&) = default;

  // T::CProxy [] operator specialization.
  typename T::CProxy operator[](size_t n) const {
    hpb_MessageValue message_value = hpb_Array_Get(this->arr_, n);
    return ::protos::internal::CreateMessage<typename std::remove_const_t<T>>(
        (hpb_Message*)message_value.msg_val, this->arena_);
  }

  // TODO(b:/280069986) : Audit/Finalize based on Iterator Design.
  // T::Proxy [] operator specialization.
  template <int&... DeductionBlocker, bool b = !kIsConst,
            typename = std::enable_if_t<b>>
  typename T::Proxy operator[](size_t n) {
    return ::protos::internal::CreateMessageProxy<T>(this->GetMessage(n),
                                                     this->arena_);
  }

  // Mutable message reference specialization.
  template <int&... DeductionBlocker, bool b = !kIsConst,
            typename = std::enable_if_t<b>>
  void push_back(const T& t) {
    hpb_MessageValue message_value;
    message_value.msg_val = hpb_Message_DeepClone(
        PrivateAccess::GetInternalMsg(&t), ::protos::internal::GetMiniTable(&t),
        this->arena_);
    hpb_Array_Append(this->arr_, message_value, this->arena_);
  }

  // Mutable message add using move.
  template <int&... DeductionBlocker, bool b = !kIsConst,
            typename = std::enable_if_t<b>>
  void push_back(T&& msg) {
    hpb_MessageValue message_value;
    message_value.msg_val = PrivateAccess::GetInternalMsg(&msg);
    hpb_Arena_Fuse(GetArena(&msg), this->arena_);
    hpb_Array_Append(this->arr_, message_value, this->arena_);
    T moved_msg = std::move(msg);
  }

 private:
  friend class ::protos::Ptr<T>;
};

// RepeatedField proxy for repeated strings.
template <class T>
class RepeatedFieldStringProxy
    : public std::conditional_t<std::is_const_v<T>, RepeatedFieldProxyBase<T>,
                                RepeatedFieldProxyMutableBase<T>> {
  static_assert(std::is_same_v<T, absl::string_view> ||
                    std::is_same_v<T, const absl::string_view>,
                "");
  static constexpr bool kIsConst = std::is_const_v<T>;

 public:
  using value_type = std::remove_const_t<T>;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using iterator = internal::Iterator<StringIteratorPolicy<T>>;
  using reference = typename iterator::reference;
  using pointer = typename iterator::pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;

  // Immutable constructor.
  explicit RepeatedFieldStringProxy(const hpb_Array* arr, hpb_Arena* arena)
      : RepeatedFieldProxyBase<T>(arr, arena) {}
  // Mutable constructor.
  RepeatedFieldStringProxy(hpb_Array* arr, hpb_Arena* arena)
      : RepeatedFieldProxyMutableBase<T>(arr, arena) {}
  // Constructor used by ::protos::Ptr.
  RepeatedFieldStringProxy(const RepeatedFieldStringProxy&) = default;

  reference operator[](size_t n) const { return begin()[n]; }

  template <int&... DeductionBlocker, bool b = !kIsConst,
            typename = std::enable_if_t<b>>
  void push_back(T t) {
    hpb_MessageValue message_value;
    // Copy string to arena.
    HPB_ASSERT(this->arena_);
    char* data = (char*)hpb_Arena_Malloc(this->arena_, t.size());
    HPB_ASSERT(data);
    memcpy(data, t.data(), t.size());
    message_value.str_val = hpb_StringView_FromDataAndSize(data, t.size());
    hpb_Array_Append(this->arr_, message_value, this->arena_);
  }

  iterator begin() const { return iterator({this->arr_, this->arena_, 0}); }
  iterator end() const {
    return iterator({this->arr_, this->arena_, this->size()});
  }
  reverse_iterator rbegin() const { return reverse_iterator(end()); }
  reverse_iterator rend() const { return reverse_iterator(begin()); }
};

// RepeatedField proxy for repeated scalar types.
template <typename T>
class RepeatedFieldScalarProxy
    : public std::conditional_t<std::is_const_v<T>, RepeatedFieldProxyBase<T>,
                                RepeatedFieldProxyMutableBase<T>> {
  static_assert(std::is_arithmetic_v<T>, "");
  static constexpr bool kIsConst = std::is_const_v<T>;

 public:
  using value_type = std::remove_const_t<T>;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using iterator = internal::Iterator<ScalarIteratorPolicy<T>>;
  using reference = typename iterator::reference;
  using pointer = typename iterator::pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;

  explicit RepeatedFieldScalarProxy(const hpb_Array* arr, hpb_Arena* arena)
      : RepeatedFieldProxyBase<T>(arr, arena) {}
  RepeatedFieldScalarProxy(hpb_Array* arr, hpb_Arena* arena)
      : RepeatedFieldProxyMutableBase<T>(arr, arena) {}
  // Constructor used by ::protos::Ptr.
  RepeatedFieldScalarProxy(const RepeatedFieldScalarProxy&) = default;

  T operator[](size_t n) const {
    hpb_MessageValue message_value = hpb_Array_Get(this->arr_, n);
    typename std::remove_const_t<T> val;
    memcpy(&val, &message_value, sizeof(T));
    return val;
  }

  template <int&... DeductionBlocker, bool b = !kIsConst,
            typename = std::enable_if_t<b>>
  void push_back(T t) {
    hpb_MessageValue message_value;
    memcpy(&message_value, &t, sizeof(T));
    hpb_Array_Append(this->arr_, message_value, this->arena_);
  }

  iterator begin() const { return iterator({unsafe_array()}); }
  iterator cbegin() const { return begin(); }
  iterator end() const { return iterator({unsafe_array() + this->size()}); }
  iterator cend() const { return end(); }

  // Reverse iterator support.
  reverse_iterator rbegin() const { return reverse_iterator(end()); }
  reverse_iterator rend() const { return reverse_iterator(begin()); }
  reverse_iterator crbegin() const { return reverse_iterator(end()); }
  reverse_iterator crend() const { return reverse_iterator(begin()); }

 private:
  T* unsafe_array() const {
    if (kIsConst) {
      const void* unsafe_ptr = ::hpb_Array_DataPtr(this->arr_);
      return static_cast<T*>(const_cast<void*>(unsafe_ptr));
    }
    if (!kIsConst) {
      void* unsafe_ptr =
          ::hpb_Array_MutableDataPtr(const_cast<hpb_Array*>(this->arr_));
      return static_cast<T*>(unsafe_ptr);
    }
  }
};

}  // namespace internal

template <typename T>
class RepeatedField {
  static constexpr bool kIsString = std::is_same_v<T, absl::string_view>;
  static constexpr bool kIsScalar = std::is_arithmetic_v<T>;

 public:
  using Proxy = std::conditional_t<
      kIsScalar, internal::RepeatedFieldScalarProxy<T>,
      std::conditional_t<kIsString, internal::RepeatedFieldStringProxy<T>,
                         internal::RepeatedFieldProxy<T>>>;
  using CProxy = std::conditional_t<
      kIsScalar, internal::RepeatedFieldScalarProxy<const T>,
      std::conditional_t<kIsString, internal::RepeatedFieldStringProxy<const T>,
                         internal::RepeatedFieldProxy<const T>>>;
  // TODO(b/286451125): T supports incomplete type from fwd.h forwarding headers
  // We would like to reference T::CProxy. Validate forwarding header design.
  using ValueProxy = std::conditional_t<
      kIsScalar, T,
      std::conditional_t<kIsString, absl::string_view, ::protos::Ptr<T>>>;
  using ValueCProxy = std::conditional_t<
      kIsScalar, const T,
      std::conditional_t<kIsString, absl::string_view, ::protos::Ptr<const T>>>;
  using Access = std::conditional_t<
      kIsScalar, internal::RepeatedFieldScalarProxy<T>,
      std::conditional_t<kIsString, internal::RepeatedFieldStringProxy<T>,
                         internal::RepeatedFieldProxy<T>>>;
};

}  // namespace protos

#include "hpb/port/undef.inc"

#endif  // UPB_PROTOS_REPEATED_FIELD_H_
