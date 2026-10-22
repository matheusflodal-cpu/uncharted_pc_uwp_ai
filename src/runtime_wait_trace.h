/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef UNCHARTED_RUNTIME_WAIT_TRACE_H
#define UNCHARTED_RUNTIME_WAIT_TRACE_H
#include <stdint.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { UW_GUEST=0,UW_MUTEX,UW_CONDITION,UW_SEMAPHORE,UW_JOIN,UW_SLEEP,UW_EQUEUE,UW_RWLOCK,UW_FILE,UW_EXITED };
void uncharted_wait_enable(void);
void uncharted_wait_name(const char *name);
void uncharted_wait_begin(unsigned kind,uintptr_t target,uintptr_t caller);
void uncharted_wait_end(int64_t result);
void __attribute__((ms_abi)) uncharted_wait_poll(uint64_t object,uint64_t current,
    uint64_t target,uint64_t mode,uint64_t caller);
void uncharted_wait_snapshot(FILE *out);
#ifdef __cplusplus
}
#endif
/* Only the Xbox runtime opts in. The host call and its result are unchanged. */
#ifdef UNCHARTED_WAIT_TRACE
#define UW_ENTRY __attribute__((noinline))
#define UW_CALLER ((uintptr_t)__builtin_return_address(0))
#define UW_NAME(name) uncharted_wait_name(name)
#define UW_BEGIN_AT(kind,target,caller) uncharted_wait_begin(kind,(uintptr_t)(target),(uintptr_t)(caller))
#define UW_BEGIN(kind,target) UW_BEGIN_AT(kind,target,UW_CALLER)
#define UW_END(result) uncharted_wait_end((int64_t)(result))
#define UW_CALL(kind,target,expr) __extension__ ({ UW_BEGIN(kind,target); __auto_type uw_result=(expr); UW_END(uw_result); uw_result; })
#else
#define UW_ENTRY
#define UW_CALLER ((uintptr_t)0)
#define UW_NAME(name) ((void)0)
#define UW_BEGIN_AT(kind,target,caller) ((void)0)
#define UW_BEGIN(kind,target) ((void)0)
#define UW_END(result) ((void)0)
#define UW_CALL(kind,target,expr) (expr)
#endif
#endif
