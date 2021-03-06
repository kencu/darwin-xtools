/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
 *
 * Copyright (c) 2004-2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <TargetConditionals.h>
#include <libkern/OSAtomic.h>
#include <errno.h>
#include <pthread.h>
#if TARGET_IPHONE_SIMULATOR
	#include "dyldSyscallInterface.h"
	#include "dyld_images.h"
	#include <mach-o/loader.h>
	#include <mach-o/nlist.h>
	#if __LP64__
		#define LC_SEGMENT_COMMAND			LC_SEGMENT_64
		typedef struct segment_command_64	macho_segment_command;
		typedef struct mach_header_64		macho_header;
		typedef struct nlist_64				macho_nlist;
	#else
		#define LC_SEGMENT_COMMAND			LC_SEGMENT
		typedef struct segment_command		macho_segment_command;
		typedef struct mach_header			macho_header;
		typedef struct nlist				macho_nlist;
	#endif
#endif

// from _simple.h in libc
typedef struct _SIMPLE*	_SIMPLE_STRING;
extern void				_simple_vdprintf(int __fd, const char *__fmt, va_list __ap);
extern void				_simple_dprintf(int __fd, const char *__fmt, ...);
extern _SIMPLE_STRING	_simple_salloc(void);
extern int				_simple_vsprintf(_SIMPLE_STRING __b, const char *__fmt, va_list __ap);
extern void				_simple_sfree(_SIMPLE_STRING __b);
extern char *			_simple_string(_SIMPLE_STRING __b);

// dyld::log(const char* format, ...)
extern void _ZN4dyld3logEPKcz(const char*, ...);

// dyld::halt(const char* msg);
extern void _ZN4dyld4haltEPKc(const char* msg) __attribute__((noreturn));


// abort called by C++ unwinding code
void abort()
{	
	_ZN4dyld4haltEPKc("dyld calling abort()\n");
}

// std::terminate called by C++ unwinding code
void _ZSt9terminatev()
{
	_ZN4dyld4haltEPKc("dyld std::terminate()\n");
}

// std::unexpected called by C++ unwinding code
void _ZSt10unexpectedv()
{
	_ZN4dyld4haltEPKc("dyld std::unexpected()\n");
}

// __cxxabiv1::__terminate(void (*)()) called to terminate process
void _ZN10__cxxabiv111__terminateEPFvvE()
{
	_ZN4dyld4haltEPKc("dyld std::__terminate()\n");
}

// __cxxabiv1::__unexpected(void (*)()) called to terminate process
void _ZN10__cxxabiv112__unexpectedEPFvvE()
{
	_ZN4dyld4haltEPKc("dyld std::__unexpected()\n");
}

// __cxxabiv1::__terminate_handler
void* _ZN10__cxxabiv119__terminate_handlerE  = &_ZSt9terminatev;

// __cxxabiv1::__unexpected_handler
void* _ZN10__cxxabiv120__unexpected_handlerE = &_ZSt10unexpectedv;

// libc uses assert()
void __assert_rtn(const char* func, const char* file, int line, const char* failedexpr)
{
	if (func == NULL)
		_ZN4dyld3logEPKcz("Assertion failed: (%s), file %s, line %d.\n", failedexpr, file, line);
	else
		_ZN4dyld3logEPKcz("Assertion failed: (%s), function %s, file %s, line %d.\n", failedexpr, func, file, line);
	abort();
}


int	myfprintf(FILE* file, const char* format, ...) __asm("_fprintf");

// called by libuwind code before aborting
size_t fwrite(const void* ptr, size_t size, size_t nitme, FILE* stream)
{
	return myfprintf(stream, "%s", (char*)ptr); 
}

// called by libuwind code before aborting
int	fprintf(FILE* file, const char* format, ...)
{
	va_list	list;
	va_start(list, format);
	_simple_vdprintf(STDERR_FILENO, format, list);
	va_end(list);
	return 0;
}

// called by LIBC_ABORT
void abort_report_np(const char* format, ...)
{
	va_list list;
	const char *str;
	_SIMPLE_STRING s = _simple_salloc();
	if ( s != NULL ) {
		va_start(list, format);
		_simple_vsprintf(s, format, list);
		va_end(list);
		str = _simple_string(s);
	} 
	else {
		// _simple_salloc failed, but at least format may have useful info by itself
		str = format; 
	}
	_ZN4dyld4haltEPKc(str);
	// _ZN4dyld4haltEPKc doesn't return, so we can't call _simple_sfree
}


// real cthread_set_errno_self() has error handling that pulls in 
// pthread_exit() which pulls in fprintf()
extern int* __error(void);
void cthread_set_errno_self(int err)
{
	int* ep = __error();
     *ep = err;
}

/*
 * We have our own localtime() to avoid needing the notify API which is used
 * by the code in libc.a for localtime() which is used by arc4random().
 */
struct tm* localtime(const time_t* t)
{
	return (struct tm*)NULL;
}

// malloc calls exit(-1) in case of errors...
void exit(int x)
{
	_ZN4dyld4haltEPKc("exit()");
}

// static initializers make calls to __cxa_atexit
void __cxa_atexit()
{
	// do nothing, dyld never terminates
}

//
// The stack protector routines in lib.c bring in too much stuff, so 
// make our own custom ones.
//
long __stack_chk_guard = 0;


void __guard_setup(const char* apple[])
{
	for (const char** p = apple; *p != NULL; ++p) {
		if ( strncmp(*p, "stack_guard=", 12) == 0 ) {
			// kernel has provide a random value for us
			for (const char* s = *p + 12; *s != '\0'; ++s) {
				char c = *s;
				long value = 0;
				if ( (c >= 'a') && (c <= 'f') )
					value = c - 'a' + 10;
				else if ( (c >= 'A') && (c <= 'F') )
					value = c - 'A' + 10;
				else if ( (c >= '0') && (c <= '9') )
					value = c - '0';
				__stack_chk_guard <<= 4;
				__stack_chk_guard |= value;
			}
			if ( __stack_chk_guard != 0 )
				return;
		}
	}
#if !TARGET_IPHONE_SIMULATOR	
#if __LP64__
	__stack_chk_guard = ((long)arc4random() << 32) | arc4random();
#else
	__stack_chk_guard = arc4random();
#endif
#endif
}

extern void _ZN4dyld4haltEPKc(const char*);
void __stack_chk_fail()
{
	_ZN4dyld4haltEPKc("stack buffer overrun");
}


// std::_throw_bad_alloc()
void _ZSt17__throw_bad_allocv()
{
	_ZN4dyld4haltEPKc("__throw_bad_alloc()");
}

// std::_throw_length_error(const char* x)
void _ZSt20__throw_length_errorPKc()
{
	_ZN4dyld4haltEPKc("_throw_length_error()");
}

// the libc.a version of this drags in ASL
void __chk_fail()
{
	_ZN4dyld4haltEPKc("__chk_fail()");
}


// referenced by libc.a(pthread.o) but unneeded in dyld
void _init_cpu_capabilities() { }
void _cpu_capabilities() {}
void set_malloc_singlethreaded() {}
int PR_5243343_flag = 0;


// used by some pthread routines
char* mach_error_string(mach_error_t err)
{
	return (char *)"unknown error code";
}
char* mach_error_type(mach_error_t err)
{
	return (char *)"(unknown/unknown)";
}

// _pthread_reap_thread calls fprintf(stderr). 
// We map fprint to _simple_vdprintf and ignore FILE* stream, so ok for it to be NULL
FILE* __stderrp = NULL;
FILE* __stdoutp = NULL;

// work with c++abi.a
void (*__cxa_terminate_handler)() = _ZSt9terminatev;
void (*__cxa_unexpected_handler)() = _ZSt10unexpectedv;

void abort_message(const char* format, ...)
{
	va_list	list;
	va_start(list, format);
	_simple_vdprintf(STDERR_FILENO, format, list);
	va_end(list);
}	

void __cxa_bad_typeid()
{
	_ZN4dyld4haltEPKc("__cxa_bad_typeid()");
}

// to work with libc++
void _ZNKSt3__120__vector_base_commonILb1EE20__throw_length_errorEv()
{
	_ZN4dyld4haltEPKc("std::vector<>::_throw_length_error()");
}

// libc.a sometimes missing memset
#undef memset
void* memset(void* b, int c, size_t len)
{
	uint8_t* p = (uint8_t*)b;
	for(size_t i=len; i > 0; --i)
		*p++ = c;
	return b;
}


// <rdar://problem/10111032> wrap calls to stat() with check for EAGAIN
int _ZN4dyld7my_statEPKcP4stat(const char* path, struct stat* buf)
{
	int result;
	do {
		result = stat(path, buf);
	} while ((result == -1) && (errno == EAGAIN));
	
	return result;
}

// <rdar://problem/13805025> dyld should retry open() if it gets an EGAIN
int _ZN4dyld7my_openEPKcii(const char* path, int flag, int other)
{
	int result;
	do {
		result = open(path, flag, other);
	} while ((result == -1) && (errno == EAGAIN));
	
	return result;
}


//
// The dyld in the iOS simulator cannot do syscalls, so it calls back to 
// host dyld.
//

#if TARGET_IPHONE_SIMULATOR
int myopen(const char* path, int oflag, int extra) __asm("_open");
int myopen(const char* path, int oflag, int extra) {
	return gSyscallHelpers->open(path, oflag, extra);
}

int close(int fd) {
	return gSyscallHelpers->close(fd);
}

ssize_t pread(int fd, void* buf, size_t nbytes, off_t offset) {
	return gSyscallHelpers->pread(fd, buf , nbytes, offset);
}

ssize_t write(int fd, const void *buf, size_t nbytes) {
	return gSyscallHelpers->write(fd, buf , nbytes);
}

void* mmap(void* addr, size_t len, int prot, int flags, int fd, off_t offset) {
	return gSyscallHelpers->mmap(addr, len, prot, flags, fd, offset);
}

int munmap(void* addr, size_t len) {
	return gSyscallHelpers->munmap(addr, len);
}

int madvise(void* addr, size_t len, int advice) {
	return gSyscallHelpers->madvise(addr, len, advice);
}

int stat(const char* path, struct stat* buf) {
	return gSyscallHelpers->stat(path, buf);
}

int myfcntl(int fd, int cmd, void* result) __asm("_fcntl");
int myfcntl(int fd, int cmd, void* result) {
	return gSyscallHelpers->fcntl(fd, cmd, result);
}

int myioctl(int fd, unsigned long request, void* result) __asm("_ioctl");
int myioctl(int fd, unsigned long request, void* result) {
	return gSyscallHelpers->ioctl(fd, request, result);
}

int issetugid() {
	return gSyscallHelpers->issetugid();
}

char* getcwd(char* buf, size_t size) {
	return gSyscallHelpers->getcwd(buf, size);
}

char* realpath(const char* file_name, char* resolved_name) {
	return gSyscallHelpers->realpath(file_name, resolved_name);
}



kern_return_t vm_allocate(vm_map_t target_task, vm_address_t *address,
						  vm_size_t size, int flags) {
	return gSyscallHelpers->vm_allocate(target_task, address, size, flags);
}

kern_return_t vm_deallocate(vm_map_t target_task, vm_address_t address,
							vm_size_t size) {
	return gSyscallHelpers->vm_deallocate(target_task, address, size);
}

kern_return_t vm_protect(vm_map_t target_task, vm_address_t address,
							vm_size_t size, boolean_t max, vm_prot_t prot) {
	return gSyscallHelpers->vm_protect(target_task, address, size, max, prot);
}


void _ZN4dyld3logEPKcz(const char* format, ...) {
	va_list	list;
	va_start(list, format);
	gSyscallHelpers->vlog(format, list);
	va_end(list);
}

void _ZN4dyld4warnEPKcz(const char* format, ...) {
	va_list	list;
	va_start(list, format);
	gSyscallHelpers->vwarn(format, list);
	va_end(list);
}


int pthread_mutex_lock(pthread_mutex_t* m) {
	return gSyscallHelpers->pthread_mutex_lock(m);
}

int pthread_mutex_unlock(pthread_mutex_t* m) {
	return gSyscallHelpers->pthread_mutex_unlock(m);
}

mach_port_t mach_thread_self() {
	return gSyscallHelpers->mach_thread_self();
}

kern_return_t mach_port_deallocate(ipc_space_t task, mach_port_name_t name) {
	return gSyscallHelpers->mach_port_deallocate(task, name);
}

mach_port_name_t task_self_trap() {
	return gSyscallHelpers->task_self_trap();
}

kern_return_t mach_timebase_info(mach_timebase_info_t info) {
	return gSyscallHelpers->mach_timebase_info(info);
}

bool OSAtomicCompareAndSwapPtrBarrier(void* old, void* new, void * volatile *value) {
	return gSyscallHelpers->OSAtomicCompareAndSwapPtrBarrier(old, new, value);
}

void OSMemoryBarrier()  {
	return gSyscallHelpers->OSMemoryBarrier();
}

uint64_t mach_absolute_time(void) {
	return gSyscallHelpers->mach_absolute_time();
} 

DIR* opendir(const char* path) {
	if ( gSyscallHelpers->version < 3 )
		return NULL;
	return gSyscallHelpers->opendir(path);
}

int	readdir_r(DIR* dirp, struct dirent* entry, struct dirent **result) {
	if ( gSyscallHelpers->version < 3 )
		return EPERM;
	return gSyscallHelpers->readdir_r(dirp, entry, result);
}

int closedir(DIR* dirp) {
	if ( gSyscallHelpers->version < 3 )
		return EPERM;
	return gSyscallHelpers->closedir(dirp);
}


typedef void (*LoadFuncPtr)(void* shm, void* image, uint64_t timestamp);
typedef void (*UnloadFuncPtr)(void* shm, void* image);

static LoadFuncPtr   sLoadPtr = NULL;
static UnloadFuncPtr sUnloadPtr = NULL;

// Lookup of coresymbolication functions in host dyld.
static void findCSProcs() {
	struct dyld_all_image_infos* imageInfo = (struct dyld_all_image_infos*)(gSyscallHelpers->getProcessInfo());
	const struct mach_header* hostDyldMH = imageInfo->dyldImageLoadAddress;

	// find symbol table and slide of host dyld
	uintptr_t slide = 0;
	const macho_nlist* symbolTable = NULL;
	const char* symbolTableStrings = NULL;
	const struct dysymtab_command* dynSymbolTable = NULL;
	const uint32_t cmd_count = hostDyldMH->ncmds;
	const struct load_command* const cmds = (struct load_command*)(((char*)hostDyldMH)+sizeof(macho_header));
	const struct load_command* cmd = cmds;
	const uint8_t* linkEditBase = NULL;
	for (uint32_t i = 0; i < cmd_count; ++i) {
		switch (cmd->cmd) {
			case LC_SEGMENT_COMMAND:
				{
					const macho_segment_command* seg = (macho_segment_command*)cmd;
					if ( (seg->fileoff == 0) && (seg->filesize != 0) )
						slide = (uintptr_t)hostDyldMH - seg->vmaddr;
					if ( strcmp(seg->segname, "__LINKEDIT") == 0 )
						linkEditBase = (uint8_t*)(seg->vmaddr - seg->fileoff + slide);
				}
				break;
			case LC_SYMTAB:
				{
					const struct symtab_command* symtab = (struct symtab_command*)cmd;
					if ( linkEditBase == NULL )
						return;
					symbolTableStrings = (const char*)&linkEditBase[symtab->stroff];
					symbolTable = (macho_nlist*)(&linkEditBase[symtab->symoff]);
				}
				break;
			case LC_DYSYMTAB:
				dynSymbolTable = (struct dysymtab_command*)cmd;
				break;
		}
		cmd = (const struct load_command*)(((char*)cmd)+cmd->cmdsize);
	}
	if ( symbolTableStrings == NULL )
		return;
	if ( dynSymbolTable == NULL )
		return;

	// scan local symbols in host dyld looking for load/unload functions
	const macho_nlist* const localsStart = &symbolTable[dynSymbolTable->ilocalsym];
	const macho_nlist* const localsEnd= &localsStart[dynSymbolTable->nlocalsym];
	for (const macho_nlist* s = localsStart; s < localsEnd; ++s) {
 		if ( ((s->n_type & N_TYPE) == N_SECT) && ((s->n_type & N_STAB) == 0) ) {
			const char* name = &symbolTableStrings[s->n_un.n_strx];
			if ( strcmp(name, "__Z28coresymbolication_load_imageP25CSCppDyldSharedMemoryPagePK11ImageLoadery") == 0 )
				sLoadPtr = (LoadFuncPtr)(s->n_value + slide);
			else if ( strcmp(name, "__Z30coresymbolication_unload_imageP25CSCppDyldSharedMemoryPagePK11ImageLoader") == 0 )
				sUnloadPtr = (UnloadFuncPtr)(s->n_value + slide);
		}
	}
}

//void coresymbolication_unload_image(void*, const ImageLoader*);
void _Z28coresymbolication_load_imagePvPK11ImageLoadery(void* shm, void* image, uint64_t time) {
	// look up function in host dyld just once
	if ( sLoadPtr == NULL ) 
		findCSProcs();
	if ( sLoadPtr != NULL ) 
		(*sLoadPtr)(shm, image, time);
} 

//void coresymbolication_load_image(void**, const ImageLoader*, uint64_t);
void _Z30coresymbolication_unload_imagePvPK11ImageLoader(void* shm, void* image) {
	// look up function in host dyld just once
	if ( sUnloadPtr == NULL ) 
		findCSProcs();
	if ( sUnloadPtr != NULL ) 
		(*sUnloadPtr)(shm, image);
}


int* __error(void) {
	return gSyscallHelpers->errnoAddress();
} 

void mach_init() {
	mach_task_self_ = task_self_trap();
	//_task_reply_port = _mach_reply_port();
}

mach_port_t mach_task_self_ = MACH_PORT_NULL;

extern int myerrno_fallback  __asm("_errno");
int myerrno_fallback = 0;

#endif  // TARGET_IPHONE_SIMULATOR


