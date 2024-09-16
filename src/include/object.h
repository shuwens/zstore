#pragma once

#include "common.h"
#include <cstdint>

// struct Object {
//     int len;
//     void *data;   /* points to after 'name' */
//     char name[0]; /* null terminated */
// };

// Object and Map related

typedef uint16_t keylen_t;
typedef uint32_t timestamp_t;

struct async_read_ctx_t {
    /*
        Common structures
    */

    // fdb_kvs_handle *handle;
    struct filemgr *file;
    struct docio_handle *dhandle;
    // err_log_callback *log_callback;
    int rderrno;
    void *buf;
    uint64_t offset;
    // docio_cb_fn docio_cb1; // from where docio_* is called
    // docio_cb_fn docio_cb2; // inside docio.cc
    struct _fdb_key_cmp_info *key_cmp_info;
    void *args;
    bool nested;

    /*
        fdb_get
    */

    // fdb_doc *udoc;
    struct docio_object *doc;
    // size_t keylen;
    // user_fdb_async_get_cb_fn user_cb;
    // user_fdb_async_get_cb_fn_nodoc user_cb_nodoc;
    uint32_t doclen;

    /*
        hbtrie_find
    */

    void *keybuf;
    struct docio_length *length;
    uint32_t pos;
    void *valuebuf;
    // docio_cb_fn hbtrie_find_cb;

    /*
        async btreeblock read
    */

    struct btree *btree;
    struct btreeblk_handle *bhandle;
    struct btreeblk_block *block;
    uint8_t *addr;
    void *key;
    uint16_t klen;
    void *value_buf;
    // docio_cb_fn btree_cb;
    int btreeres;
    struct btree_find_state_machine *btree_sm;

    /*
     * Latency tracking
     */

    uint64_t submission;
    uint64_t btree_done;
    uint64_t read_queued;
    uint64_t taken_off_queue;
    uint64_t cache_read;
    uint64_t sent_to_kvssd;
    uint64_t queued_in_kvssd;
    uint64_t returned_from_kvssd;
    uint64_t completion;

    // mutex_t lock;
};

struct obj_handle {
    struct filemgr *file;
    // bid_t curblock;
    uint32_t curpos;
    uint16_t cur_bmp_revnum_hash;
    // for buffer purpose
    // bid_t lastbid;
    uint64_t lastBmpRevnum;
    void *readbuffer;
    uint64_t last_read_size;
    // err_log_callback *log_callback;
    bool compress_document_body;
};

// this structure will occupy 16 bytes
struct obj_length {
    obj_length()
        : keylen(0), metalen(0), bodylen(0), bodylen_ondisk(0), flag(0x0),
          checksum(0x0), reserved(0x0)
    {
    }
    keylen_t keylen;
    uint16_t metalen;
    uint32_t bodylen;
    uint32_t bodylen_ondisk;
    uint8_t flag;
    uint8_t checksum;
    uint16_t reserved;
};

struct ZstoreObject {
    ZstoreObject()
        : timestamp(0), key(nullptr), seqnum(0), meta(nullptr), body(nullptr),
          vernum(0)
    {
    }
    struct obj_length length;
    timestamp_t timestamp;
    void *key;
    union {
        // fdb_seqnum_t seqnum;
        uint64_t seqnum;
        uint64_t doc_offset;
    };
    void *meta;
    void *body;
    uint64_t vernum;
};

// fdb_status obj_init(struct obj_handle *handle, struct filemgr *file,
//                     bool compress_document_body);
Result<ZstoreObject> obj_init(struct obj_handle *handle, struct filemgr *file,
                              bool compress_document_body);

void obj_free(struct obj_handle *handle);

// bid_t obj_append_doc_raw(struct obj_handle *handle, void *key, keylen_t
// keylen,
//                          uint64_t vernum, uint64_t size, void *buf);

#define obj_COMMIT_MARK_SIZE (sizeof(struct obj_length) + sizeof(uint64_t))
// bid_t obj_append_commit_mark(struct obj_handle *handle, uint64_t doc_offset);
// bid_t obj_append_doc(struct obj_handle *handle, struct obj_object *doc,
//                      uint8_t deleted, uint8_t txn_enabled);
// bid_t obj_append_doc_system(struct obj_handle *handle, struct obj_object
// *doc);

/**
 * Retrieve the length info of a KV item at a given file offset.
 *
 * @param handle Pointer to the doc I/O handle
 * @Param length Pointer to obj_length instance to be populated
 * @param offset File offset to a KV item
 * @return FDB_RESULT_SUCCESS on success
 */
fdb_status obj_read_doc_length(struct obj_handle *handle,
                               struct obj_length *length, uint64_t offset);

/**
 * Read a key and its length at a given file offset.
 *
 * @param handle Pointer to the doc I/O handle
 * @param offset File offset to a KV item
 * @param keylen Pointer to a key length variable
 * @param keybuf Pointer to a key buffer
 * @return FDB_RESULT_SUCCESS on success
 */
fdb_status obj_read_doc_key(struct obj_handle *handle, uint64_t offset,
                            keylen_t *keylen, void *keybuf);

fdb_status obj_read_doc_key_async(struct obj_handle *handle, uint64_t offset,
                                  keylen_t *keylen, void *keybuf,
                                  struct async_read_ctx_t *args);

/**
 * Read a key and its metadata at a given file offset.
 *
 * @param handle Pointer to the doc I/O handle
 * @param offset File offset to a KV item
 * @param doc Pointer to obj_object instance
 * @param read_on_cache_miss Flag indicating if a disk read should be performed
 *        on cache miss
 * @return next offset right after a key and its metadata on succcessful read,
 *         otherwise, the corresponding error code is returned.
 */
int64_t obj_read_doc_key_meta(struct obj_handle *handle, uint64_t offset,
                              struct obj_object *doc, bool read_on_cache_miss);

/**
 * Read a KV item at a given file offset.
 *
 * @param handle Pointer to the doc I/O handle
 * @param offset File offset to a KV item
 * @param doc Pointer to obj_object instance
 * @param read_on_cache_miss Flag indicating if a disk read should be performed
 *        on cache miss
 * @return next offset right after a key and its value on succcessful read,
 *         otherwise, the corresponding error code is returned.
 */
int64_t obj_read_doc(struct obj_handle *handle, uint64_t offset,
                     struct obj_object *doc, bool read_on_cache_miss);

/**
 * Read a KV item at a given file offset.
 *
 * @param handle Pointer to the doc I/O handle
 * @param offset File offset to a KV item
 * @param doc Pointer to obj_object instance
 * @param read_on_cache_miss Flag indicating if a disk read should be performed
 *        on cache miss
 * @return next offset right after a key and its value on succcessful read,
 *         otherwise, the corresponding error code is returned.
 */
fdb_status obj_read_doc_async(struct obj_handle *handle, uint64_t offset,
                              struct obj_object *doc, bool read_on_cache_miss,
                              struct async_read_ctx_t *args);

int64_t obj_read_hashed_doc(struct obj_handle *handle, uint64_t offset,
                            struct obj_object *doc, bool read_on_cache_miss);

size_t obj_batch_read_docs(struct obj_handle *handle, uint64_t *offset_array,
                           struct obj_object *doc_array, size_t array_size,
                           size_t data_size_threshold,
                           size_t batch_size_threshold,
                           struct async_io_handle *aio_handle,
                           bool keymeta_only);

/**
 * Check if the given block is a valid document block. The bitmap revision
 * number of the document block should match the passed revision number.
 *
 * @param handle Pointer to obj handle.
 * @param bid ID of the block.
 * @param sb_bmp_revnum Revision number of bitmap in superblock. If the value is
 *        -1, this function does not care about revision number.
 * @return True if valid.
 */
bool obj_check_buffer(struct obj_handle *handle, bid_t bid,
                      uint64_t sb_bmp_revnum);

/**
 * Check if the given KV pair is a document or not. For now, checks if document
 * size doesn't equal blocksize
 *
 * @param handle Pointer to obj handle.
 * @param bid ID of the block.
 * @param sb_bmp_revnum Revision number of bitmap in superblock. If the value is
 *        -1, this function does not care about revision number.
 * @return True if valid.
 */
bool obj_check_buffer_kvssd(struct obj_handle *handle, bid_t bid);
bool obj_check_buffer_kvssd_direct(struct obj_handle *handle, void *buf,
                                   uint32_t len);
int64_t obj_buf_to_doc(struct obj_handle *handle, uint64_t offset,
                       struct obj_object *doc, void *buf);

INLINE void obj_reset(struct obj_handle *dhandle)
{
    dhandle->curblock = BLK_NOT_FOUND;
}
void free_obj_object(struct obj_object *doc, uint8_t key_alloc,
                     uint8_t meta_alloc, uint8_t body_alloc);
