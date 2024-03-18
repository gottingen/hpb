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

/*
 * This is where we define internal portability macros used across hpb.
 *
 * All of these macros are undef'd in undef.inc to avoid leaking them to users.
 *
 * The correct usage is:
 *
 *   #include "hpb/foobar.h"
 *   #include "hpb/baz.h"
 *
 *   // MUST be last included header.
 *   #include "hpb/port/def.inc"
 *
 *   // Code for this file.
 *   // <...>
 *
 *   // Can be omitted for .c files, required for .h.
 *   #include "hpb/port/undef.inc"
 *
 * This file is private and must not be included by users!
 */

#if !((defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L) || \
      (defined(__cplusplus) && __cplusplus >= 201402L) ||           \
      (defined(_MSC_VER) && _MSC_VER >= 1900))
#error hpb requires C99 or C++14 or MSVC >= 2015.
#endif

// Portable check for GCC minimum version:
// https://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html
#if defined(__GNUC__) && defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__)
#define HPB_GNUC_MIN(x, y) \
  (__GNUC__ > (x) || __GNUC__ == (x) && __GNUC_MINOR__ >= (y))
#else
#define HPB_GNUC_MIN(x, y) 0
#endif

#include <assert.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifndef UINTPTR_MAX
Error, UINTPTR_MAX is undefined
#endif

#if UINTPTR_MAX == 0xffffffff
#define HPB_SIZE(size32, size64) size32
#else
#define HPB_SIZE(size32, size64) size64
#endif

/* If we always read/write as a consistent type to each address, this shouldn't
 * violate aliasing.
 */
#define HPB_PTR_AT(msg, ofs, type) ((type*)((char*)(msg) + (ofs)))

#define HPB_MAPTYPE_STRING 0

// HPB_EXPORT: always generate a public symbol.
#if defined(__GNUC__) || defined(__clang__)
#define HPB_EXPORT __attribute__((visibility("default"))) __attribute__((used))
#else
#define HPB_EXPORT
#endif

// HPB_INLINE: inline if possible, emit standalone code if required.
#ifdef __cplusplus
#define HPB_INLINE inline
#elif defined (__GNUC__) || defined(__clang__)
#define HPB_INLINE static __inline__
#else
#define HPB_INLINE static
#endif

#ifdef HPB_BUILD_API
#define HPB_API HPB_EXPORT
#define HPB_API_INLINE HPB_EXPORT
#else
#define HPB_API
#define HPB_API_INLINE HPB_INLINE
#endif

#define HPB_MALLOC_ALIGN 8
#define HPB_ALIGN_UP(size, align) (((size) + (align) - 1) / (align) * (align))
#define HPB_ALIGN_DOWN(size, align) ((size) / (align) * (align))
#define HPB_ALIGN_MALLOC(size) HPB_ALIGN_UP(size, HPB_MALLOC_ALIGN)
#ifdef __clang__
#define HPB_ALIGN_OF(type) _Alignof(type)
#else
#define HPB_ALIGN_OF(type) offsetof (struct { char c; type member; }, member)
#endif

// Hints to the compiler about likely/unlikely branches.
#if defined (__GNUC__) || defined(__clang__)
#define HPB_LIKELY(x) __builtin_expect((bool)(x), 1)
#define HPB_UNLIKELY(x) __builtin_expect((bool)(x), 0)
#else
#define HPB_LIKELY(x) (x)
#define HPB_UNLIKELY(x) (x)
#endif

// Macros for function attributes on compilers that support them.
#ifdef __GNUC__
#define HPB_FORCEINLINE __inline__ __attribute__((always_inline))
#define HPB_NOINLINE __attribute__((noinline))
#define HPB_NORETURN __attribute__((__noreturn__))
#define HPB_PRINTF(str, first_vararg) __attribute__((format (printf, str, first_vararg)))
#elif defined(_MSC_VER)
#define HPB_NOINLINE
#define HPB_FORCEINLINE
#define HPB_NORETURN __declspec(noreturn)
#define HPB_PRINTF(str, first_vararg)
#else  /* !defined(__GNUC__) */
#define HPB_FORCEINLINE
#define HPB_NOINLINE
#define HPB_NORETURN
#define HPB_PRINTF(str, first_vararg)
#endif

#define HPB_MAX(x, y) ((x) > (y) ? (x) : (y))
#define HPB_MIN(x, y) ((x) < (y) ? (x) : (y))

#define HPB_UNUSED(var) (void)var

// HPB_ASSUME(): in release mode, we tell the compiler to assume this is true.
#ifdef NDEBUG
#ifdef __GNUC__
#define HPB_ASSUME(expr) if (!(expr)) __builtin_unreachable()
#elif defined _MSC_VER
#define HPB_ASSUME(expr) if (!(expr)) __assume(0)
#else
#define HPB_ASSUME(expr) do {} while (false && (expr))
#endif
#else
#define HPB_ASSUME(expr) assert(expr)
#endif

/* HPB_ASSERT(): in release mode, we use the expression without letting it be
 * evaluated.  This prevents "unused variable" warnings. */
#ifdef NDEBUG
#define HPB_ASSERT(expr) do {} while (false && (expr))
#else
#define HPB_ASSERT(expr) assert(expr)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define HPB_UNREACHABLE() do { assert(0); __builtin_unreachable(); } while(0)
#elif defined(_MSC_VER)
#define HPB_UNREACHABLE() \
  do {                    \
    assert(0);            \
    __assume(0);          \
  } while (0)
#else
#define HPB_UNREACHABLE() do { assert(0); } while(0)
#endif

/* HPB_SETJMP() / HPB_LONGJMP(): avoid setting/restoring signal mask. */
#ifdef __APPLE__
#define HPB_SETJMP(buf) _setjmp(buf)
#define HPB_LONGJMP(buf, val) _longjmp(buf, val)
#else
#define HPB_SETJMP(buf) setjmp(buf)
#define HPB_LONGJMP(buf, val) longjmp(buf, val)
#endif

#ifdef __GNUC__
#define HPB_USE_C11_ATOMICS
#define HPB_ATOMIC(T) _Atomic(T)
#else
#define HPB_ATOMIC(T) T
#endif

/* HPB_PTRADD(ptr, ofs): add pointer while avoiding "NULL + 0" UB */
#define HPB_PTRADD(ptr, ofs) ((ofs) ? (ptr) + (ofs) : (ptr))

#define HPB_PRIVATE(x) x##_dont_copy_me__hpb_internal_use_only

/* Configure whether fasttable is switched on or not. *************************/

#ifdef __has_attribute
#define HPB_HAS_ATTRIBUTE(x) __has_attribute(x)
#else
#define HPB_HAS_ATTRIBUTE(x) 0
#endif

#if HPB_HAS_ATTRIBUTE(musttail)
#define HPB_MUSTTAIL __attribute__((musttail))
#else
#define HPB_MUSTTAIL
#endif

#undef HPB_HAS_ATTRIBUTE

/* This check is not fully robust: it does not require that we have "musttail"
 * support available. We need tail calls to avoid consuming arbitrary amounts
 * of stack space.
 *
 * GCC/Clang can mostly be trusted to generate tail calls as long as
 * optimization is enabled, but, debug builds will not generate tail calls
 * unless "musttail" is available.
 *
 * We should probably either:
 *   1. require that the compiler supports musttail.
 *   2. add some fallback code for when musttail isn't available (ie. return
 *      instead of tail calling). This is safe and portable, but this comes at
 *      a CPU cost.
 */
#if (defined(__x86_64__) || defined(__aarch64__)) && defined(__GNUC__)
#define HPB_FASTTABLE_SUPPORTED 1
#else
#define HPB_FASTTABLE_SUPPORTED 0
#endif

/* define HPB_ENABLE_FASTTABLE to force fast table support.
 * This is useful when we want to ensure we are really getting fasttable,
 * for example for testing or benchmarking. */
#if defined(HPB_ENABLE_FASTTABLE)
#if !HPB_FASTTABLE_SUPPORTED
#error fasttable is x86-64/ARM64 only and requires GCC or Clang.
#endif
#define HPB_FASTTABLE 1
/* Define HPB_TRY_ENABLE_FASTTABLE to use fasttable if possible.
 * This is useful for releasing code that might be used on multiple platforms,
 * for example the PHP or Ruby C extensions. */
#elif defined(HPB_TRY_ENABLE_FASTTABLE)
#define HPB_FASTTABLE HPB_FASTTABLE_SUPPORTED
#else
#define HPB_FASTTABLE 0
#endif

/* HPB_FASTTABLE_INIT() allows protos compiled for fasttable to gracefully
 * degrade to non-fasttable if the runtime or platform do not support it. */
#if !HPB_FASTTABLE
#define HPB_FASTTABLE_INIT(...)
#define HPB_FASTTABLE_MASK(mask) -1
#else
#define HPB_FASTTABLE_INIT(...) __VA_ARGS__
#define HPB_FASTTABLE_MASK(mask) mask
#endif

#undef HPB_FASTTABLE_SUPPORTED

/* ASAN poisoning (for arena).
 * If using HPB from an interpreted language like Ruby, a build of the
 * interpreter compiled with ASAN enabled must be used in order to get sane and
 * expected behavior.
 */

/* Due to preprocessor limitations, the conditional logic for setting
 * UPN_CLANG_ASAN below cannot be consolidated into a portable one-liner.
 * See https://gcc.gnu.org/onlinedocs/cpp/_005f_005fhas_005fattribute.html.
 */
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define HPB_CLANG_ASAN 1
#else
#define HPB_CLANG_ASAN 0
#endif
#else
#define HPB_CLANG_ASAN 0
#endif

#if defined(__SANITIZE_ADDRESS__) || HPB_CLANG_ASAN
#define HPB_ASAN 1
#define HPB_ASAN_GUARD_SIZE 32
#ifdef __cplusplus
    extern "C" {
#endif
void __asan_poison_memory_region(void const volatile *addr, size_t size);
void __asan_unpoison_memory_region(void const volatile *addr, size_t size);
#ifdef __cplusplus
}  /* extern "C" */
#endif
#define HPB_POISON_MEMORY_REGION(addr, size) \
  __asan_poison_memory_region((addr), (size))
#define HPB_UNPOISON_MEMORY_REGION(addr, size) \
  __asan_unpoison_memory_region((addr), (size))
#else
#define HPB_ASAN 0
#define HPB_ASAN_GUARD_SIZE 0
#define HPB_POISON_MEMORY_REGION(addr, size) \
  ((void)(addr), (void)(size))
#define HPB_UNPOISON_MEMORY_REGION(addr, size) \
  ((void)(addr), (void)(size))
#endif

/* Disable proto2 arena behavior (TEMPORARY) **********************************/

#ifdef HPB_DISABLE_PROTO2_ENUM_CHECKING
#define HPB_TREAT_PROTO2_ENUMS_LIKE_PROTO3 1
#else
#define HPB_TREAT_PROTO2_ENUMS_LIKE_PROTO3 0
#endif

#if defined(__cplusplus)
#if defined(__clang__) || HPB_GNUC_MIN(6, 0)
// https://gcc.gnu.org/gcc-6/changes.html
#if __cplusplus >= 201402L
#define HPB_DEPRECATED [[deprecated]]
#else
#define HPB_DEPRECATED __attribute__((deprecated))
#endif
#else
#define HPB_DEPRECATED
#endif
#else
#define HPB_DEPRECATED
#endif

// begin:google_only
// #define HPB_IS_GOOGLE3
// end:google_only

#if defined(HPB_IS_GOOGLE3) && !defined(HPB_BOOTSTRAP_STAGE0)
#define HPB_DESC(sym) proto2_##sym
#else
#define HPB_DESC(sym) google_protobuf_##sym
#endif
