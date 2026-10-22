/* SPDX-License-Identifier: GPL-2.0-or-later
 * Exercise the real UWP bounce-buffer path on a portable host. Inject short
 * host reads, the failure mode fixed in Uncharted 0.3.6. This does not test
 * Windows mappings or actual disk eviction; those need the UWP memory test.
 */
#define _GNU_SOURCE
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
static unsigned short_reads;
static ssize_t limited_read(int fd,void *buffer,size_t size) {
    if(size>4093) { size=4093; ++short_reads; }
    return read(fd,buffer,size);
}
static ssize_t limited_pread(int fd,void *buffer,size_t size,off_t offset) {
    if(size>4093) { size=4093; ++short_reads; }
    return pread(fd,buffer,size,offset);
}
#define read limited_read
#define pread limited_pread
#define UNCHARTED_UWP
#include "../src/runtime_file.c"
#undef read
#undef pread
static int32_t saved_errno;
static int mapped=1;
int32_t *runtime_errno(void) { return &saved_errno; }
int32_t runtime_guest_errno(int e) { return e; }
int runtime_memory_is_mapped(uintptr_t address,uint64_t size) { (void)address; (void)size; return mapped; }
uintptr_t runtime_lookup(const RuntimeExport *table,size_t count,const char *name) {
    for(size_t i=0;i<count;++i) if(!strcmp(table[i].name,name)) return (uintptr_t)table[i].function;
    return 0;
}
int main(void) {
    assert(setenv("BB_IO_STATS","1",1)==0);
    char path[]="/tmp/uncharted-io-XXXXXX";
    int fd=mkstemp(path); assert(fd>=3);
    const size_t size=(3u<<20)+137;
    unsigned char *source=malloc(size),*destination=malloc(size+31);
    assert(source&&destination);
    for(size_t i=0;i<size;++i) source[i]=(unsigned char)(i*29u+(i>>13));
    assert(paged_io(fd,source,size,0,1,1)==(ssize_t)size);
    assert(lseek(fd,0,SEEK_CUR)==0); /* positioned writes do not move the file offset */
    memset(destination,0,size+31);
    assert(paged_io(fd,destination,size+31,0,1,0)==(ssize_t)size); /* EOF is a valid short result */
    assert(!memcmp(source,destination,size));
    assert(lseek(fd,0,SEEK_CUR)==0);
    assert(!runtime_file_mount("/app0","/tmp"));
    char guest_path[256];
    snprintf(guest_path,sizeof(guest_path),"/app0/%s",strrchr(path,'/')+1);
    int guest_fd=(int)runtime_file_open(guest_path,0,0); assert(guest_fd>=3);
    /* Guest API entry points select bounce I/O for mapped pages and preserve
     * full reads for ordinary host memory. Both must survive short reads. */
    for(mapped=0;mapped<=1;++mapped) {
        memset(destination,0,size);
        assert(runtime_file_pread(guest_fd,destination,size,0)==(int64_t)size);
        assert(!memcmp(source,destination,size));
        assert(runtime_file_lseek(guest_fd,0,SEEK_SET)==0);
        memset(destination,0,size);
        assert(runtime_file_read(guest_fd,destination,size)==(int64_t)size);
        assert(!memcmp(source,destination,size));
        assert(runtime_file_lseek(guest_fd,0,SEEK_CUR)==(int64_t)size);
    }
    assert(io_active==0&&io_calls==4&&io_bytes==4*size);
    assert(io_last_finished_ns>0&&io_errors==0);
    assert(do_pread(guest_fd,destination,16,-1)==-EINVAL);
    assert(io_active==0&&io_calls==5&&io_errors==1);
    /* Reporting resets interval totals, never the active-read gauge or last
     * completion timestamp. A concurrent reader is represented without locks. */
    uint64_t pending=io_started(); assert(pending&&io_active==1);
    runtime_file_profile_report();
    assert(io_active==1&&io_calls==0&&io_errors==0&&io_last_finished_ns>0);
    io_finished(pending,0,0); assert(io_active==0&&io_calls==1);
    assert(short_reads>100);
    assert(paged_io(-1,destination,16,0,0,0)==-1&&errno==EBADF);
    assert(paged_io(fd,NULL,0,0,0,0)==0);
    assert(!runtime_file_close(guest_fd));
    close(fd); unlink(path); free(source); free(destination);
    puts("PASS: paged I/O short reads, multi-chunk data integrity, offsets, EOF, fallback and errors");
    return 0;
}
