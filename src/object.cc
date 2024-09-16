#include "include/object.h"
#include <stdlib.h>
#include <string.h>

Result<ZstoreObject> obj_init(struct obj_handle *handle, struct filemgr *file,
                              bool compress_document_body)
{
    // init object handle?
}

// void obj_free(struct obj_handle *handle);

// bid_t obj_append_doc(struct obj_handle *handle, struct obj_object *doc,
//                      uint8_t deleted, uint8_t txn_enabled);

// bid_t obj_append_doc_system(struct obj_handle *handle, struct obj_object
// *doc);

Result<uint32_t> obj_read_length(struct obj_handle *handle,
                                 struct obj_length *length, uint64_t offset)
{
    size_t blocksize = 4096;
    bid_t bid = offset / blocksize;
    uint32_t pos = offset % blocksize;
    void *buf = handle->readbuffer;
    uint32_t restsize = 0;

    if (blocksize > pos) {
        restsize = blocksize - pos;
    }

    // if (handle->file->config->kvssd) {
    //     return pos;
    // } else {
    return bid * blocksize + pos;
    // }
}

Result<void> obj_read_key(struct obj_handle *handle, uint64_t offset,
                          keylen_t *keylen, void *keybuf)
{
    // *keylen = length.keylen;
    // return FDB_RESULT_SUCCESS;
}

// Result<void> obj_read_doc_key_async(struct obj_handle *handle, uint64_t
// offset,
//                                     keylen_t *keylen, void *keybuf,
//                                     struct async_read_ctx_t *args);

int64_t obj_read(struct obj_handle *handle, uint64_t offset,
                 struct ZstoreObject *obj, void *ctx)
{
    uint8_t checksum;
    int64_t _offset = 0;
    int key_alloc = 0;
    int meta_alloc = 0;
    int body_alloc = 0;
    fdb_seqnum_t _seqnum;
    timestamp_t _timestamp;
    void *comp_body = NULL;
    struct docio_length _length, zero_length;
    err_log_callback *log_callback = handle->log_callback;

    _offset = _docio_read_length(handle, offset, &_length, log_callback,
                                 read_on_cache_miss);
    if (_offset < 0) {
        if (read_on_cache_miss) {
            fdb_log(
                log_callback, FDB_LOG_ERROR, (fdb_status)_offset,
                "Error in reading the doc length metadata with offset %" _F64
                " from "
                "a database file '%s'",
                offset, handle->file->filename);
        }
        return _offset;
    }

    memset(&zero_length, 0x0, sizeof(struct docio_length));
    if (memcmp(&_length, &zero_length, sizeof(struct docio_length)) == 0) {
        // If all the fields in docio_length are zero, then it means that the
        // rest of the current block, which starts at offset, is zero-filled
        and
            // can be skipped.
            doc->length = zero_length;
        return (int64_t)FDB_RESULT_SUCCESS;
    }

    // checksum check
    checksum = _docio_length_checksum(_length, handle);
    if (checksum != _length.checksum) {
        fdb_log(
            log_callback, FDB_LOG_ERROR, FDB_RESULT_CHECKSUM_ERROR,
            "doc_length body checksum mismatch error in a database file '%s'"
            " crc %x != %x (crc in doc) keylen %d metalen %d bodylen %d "
            "bodylen_ondisk %d offset %" _F64,
            handle->file->filename, checksum, _length.checksum, _length.keylen,
            _length.metalen, _length.bodylen, _length.bodylen_ondisk, offset);
        return (int64_t)FDB_RESULT_CHECKSUM_ERROR;
    }

    doc->length = _docio_length_decode(_length);
    if (doc->length.flag & DOCIO_TXN_COMMITTED) {
        // transaction commit mark
        // read the corresponding doc offset

        // If TXN_COMMITTED flag is set, this doc is not an actual doc, but a
        // transaction commit marker. Thus, all lengths should be zero.
        if (doc->length.keylen || doc->length.metalen || doc->length.bodylen ||
            doc->length.bodylen_ondisk) {
            fdb_log(log_callback, FDB_LOG_ERROR, FDB_RESULT_FILE_CORRUPTION,
                    "File corruption: Doc length fields in a transaction "
                    "commit marker "
                    "was not zero in a database file '%s' offset %" _F64,
                    handle->file->filename, offset);
            free_docio_object(doc, key_alloc, meta_alloc, body_alloc);
            return (int64_t)FDB_RESULT_FILE_CORRUPTION;
        }
        uint64_t doc_offset;
        if (handle->file->config->kvssd) {
            _offset = _docio_read_doc_component_kvssd(
                handle, offset, _offset, sizeof(doc_offset), &doc_offset,
                log_callback);
        } else {
            _offset = _docio_read_doc_component(
                handle, _offset, sizeof(doc_offset), &doc_offset, log_callback);
        }
        if (_offset < 0) {
            fdb_log(log_callback, FDB_LOG_ERROR, (fdb_status)_offset,
                    "Error in reading an offset of a committed doc from an "
                    "offset %" _F64 " in a database file '%s'",
                    offset, handle->file->filename);
            free_docio_object(doc, key_alloc, meta_alloc, body_alloc);
            return _offset;
        }
        doc->doc_offset = _endian_decode(doc_offset);
        // The offset of the actual document that pointed by this commit
        marker
            // should not be greater than the file size.
            if (doc->doc_offset > filemgr_get_pos(handle->file))
        {
            fdb_log(log_callback, FDB_LOG_ERROR, FDB_RESULT_FILE_CORRUPTION,
                    "File corruption: Offset %" _F64
                    " of the actual doc pointed by the "
                    "commit marker is greater than the size %" _F64
                    " of a database file '%s'",
                    doc->doc_offset, filemgr_get_pos(handle->file),
                    handle->file->filename);
            free_docio_object(doc, key_alloc, meta_alloc, body_alloc);
            return (int64_t)FDB_RESULT_FILE_CORRUPTION;
        }
        return _offset;
    }

    if (doc->length.keylen == 0 ||
        doc->length.keylen > FDB_MAX_KEYLEN_INTERNAL) {
        fdb_log(log_callback, FDB_LOG_ERROR, FDB_RESULT_FILE_CORRUPTION,
                "Error in decoding the doc length metadata (key length: %d) from
                " " a database file '%s' offset %
                    " _F64, doc->length.keylen,
                    handle->file->filename,
                offset);
        return (int64_t)FDB_RESULT_FILE_CORRUPTION;
    }

    if (doc->key == NULL) {
        doc->key = (void *)malloc(doc->length.keylen);
        key_alloc = 1;
    }
    if (doc->meta == NULL && doc->length.metalen) {
        doc->meta = (void *)malloc(doc->length.metalen);
        meta_alloc = 1;
    }
    if (doc->body == NULL && doc->length.bodylen) {
        doc->body = (void *)malloc(doc->length.bodylen);
        body_alloc = 1;
    }

    if (handle->file->config->kvssd) {
        _offset = _docio_read_doc_component_kvssd(handle, offset, _offset,
                                                  doc->length.keylen, doc->key,
                                                  log_callback);
    } else {
        _offset = _docio_read_doc_component(handle, _offset, doc->length.keylen,
                                            doc->key, log_callback);
    }
    if (_offset < 0) {
        fdb_log(log_callback, FDB_LOG_ERROR, (fdb_status)_offset,
                "Error in reading a key with offset %" _F64 ", length %d "
                "from a database file '%s'",
                offset, doc->length.keylen, handle->file->filename);
        free_docio_object(doc, key_alloc, meta_alloc, body_alloc);
        return _offset;
    }

    // read timestamp
    if (handle->file->config->kvssd) {
        _offset = _docio_read_doc_component_kvssd(handle, offset, _offset,
                                                  sizeof(timestamp_t),
                                                  &_timestamp, log_callback);
    } else {
        _offset = _docio_read_doc_component(
            handle, _offset, sizeof(timestamp_t), &_timestamp, log_callback);
    }
    if (_offset < 0) {
        fdb_log(log_callback, FDB_LOG_ERROR, (fdb_status)_offset,
                "Error in reading a timestamp with offset %" _F64 ", length
                    % d " " from a database file '%s' ", offset,
                    sizeof(timestamp_t),
                handle->file->filename);
        free_docio_object(doc, key_alloc, meta_alloc, body_alloc);
        return _offset;
    }
    doc->timestamp = _endian_decode(_timestamp);

    // copy seqeunce number (optional)
    if (handle->file->config->kvssd) {
        _offset = _docio_read_doc_component_kvssd(
            handle, offset, _offset, sizeof(fdb_seqnum_t), (void *)&_seqnum,
            log_callback);
    } else {
        _offset =
            _docio_read_doc_component(handle, _offset, sizeof(fdb_seqnum_t),
                                      (void *)&_seqnum, log_callback);
    }
    if (_offset < 0) {
        fdb_log(log_callback, FDB_LOG_ERROR, (fdb_status)_offset,
                "Error in reading a sequence number with offset %" _F64
                ", length %d "
                "from a database file '%s'",
                offset, sizeof(fdb_seqnum_t), handle->file->filename);
        free_docio_object(doc, key_alloc, meta_alloc, body_alloc);
        return _offset;
    }
    doc->seqnum = _endian_decode(_seqnum);

    if (handle->file->config->kvssd) {
        _offset = _docio_read_doc_component_kvssd(handle, offset, _offset,
                                                  doc->length.metalen,
                                                  doc->meta, log_callback);
    } else {
        _offset = _docio_read_doc_component(
            handle, _offset, doc->length.metalen, doc->meta, log_callback);
    }
    if (_offset < 0) {
        fdb_log(log_callback, FDB_LOG_ERROR, (fdb_status)_offset,
                "Error in reading the doc metadata with offset %" _F64
                ", length %d "
                "from a database file '%s'",
                offset, doc->length.metalen, handle->file->filename);
        free_docio_object(doc, key_alloc, meta_alloc, body_alloc);
        return _offset;
    }

#ifdef _DOC_COMP
    if (doc->length.flag & DOCIO_COMPRESSED) {
        comp_body = (void *)malloc(doc->length.bodylen_ondisk);
        if (handle->file->config->kvssd) {
            _offset = _docio_read_doc_component_comp_kvssd(
                handle, offset, _offset, doc->length.bodylen,
                doc->length.bodylen_ondisk, doc->body, comp_body, log_callback);
        } else {
            _offset = _docio_read_doc_component_comp(
                handle, _offset, doc->length.bodylen,
                doc->length.bodylen_ondisk, doc->body, comp_body, log_callback);
        }
        printf("MS: compression occurred! Need function for compression\n");

        if (_offset < 0) {
            fdb_log(log_callback, FDB_LOG_ERROR, (fdb_status)_offset,
                    "Error in reading a compressed doc with offset %" _F64
                    ", length %d "
                    "from a database file '%s'",
                    offset, doc->length.bodylen, handle->file->filename);
            if (comp_body) {
                free(comp_body);
            }
            free_docio_object(doc, key_alloc, meta_alloc, body_alloc);
            return _offset;
        }
    } else {
        if (handle->file->config->kvssd) {
            _offset = _docio_read_doc_component_kvssd(handle, offset, _offset,
                                                      doc->length.bodylen,
                                                      doc->body, log_callback);
        } else {
            _offset = _docio_read_doc_component(
                handle, _offset, doc->length.bodylen, doc->body, log_callback);
        }
        if (_offset < 0) {
            fdb_log(log_callback, FDB_LOG_ERROR, (fdb_status)_offset,
                    "Error in reading a doc with offset %" _F64 ", length %d
                    " " from a database file '%s' ", offset,
                    doc->length.bodylen,
                    handle->file->filename);
            free_docio_object(doc, key_alloc, meta_alloc, body_alloc);
            return _offset;
        }
    }
#else
    if (handle->file->config->kvssd) {
        _offset = _docio_read_doc_component_kvssd(handle, offset, _offset,
                                                  doc->length.bodylen,
                                                  doc->body, log_callback);
    } else {
        _offset = _docio_read_doc_component(
            handle, _offset, doc->length.bodylen, doc->body, log_callback);
    }
    if (_offset < 0) {
        fdb_log(log_callback, FDB_LOG_ERROR, (fdb_status)_offset,
                "Error in reading a doc with offset %" _F64 ", length %d "
                "from a database file '%s'",
                offset, doc->length.bodylen, handle->file->filename);
        free_docio_object(doc, key_alloc, meta_alloc, body_alloc);
        return _offset;
    }
#endif

#ifdef __CRC32
    uint32_t crc_file, crc;
    if (handle->file->config->kvssd) {
        _offset = _docio_read_doc_component_kvssd(
            handle, offset, _offset, sizeof(crc_file), (void *)&crc_file,
            log_callback);
    } else {
        _offset = _docio_read_doc_component(handle, _offset, sizeof(crc_file),
                                            (void *)&crc_file, log_callback);
    }
    if (_offset < 0) {
        fdb_log(log_callback, FDB_LOG_ERROR, (fdb_status)_offset,
                "Error in reading a doc's CRC value with offset %" _F64
                ", length %d "
                "from a database file '%s'",
                offset, sizeof(crc_file), handle->file->filename);
        if (comp_body) {
            free(comp_body);
        }
        free_docio_object(doc, key_alloc, meta_alloc, body_alloc);
        return _offset;
    }

    crc = get_checksum(reinterpret_cast<const uint8_t *>(&_length),
                       sizeof(_length), handle->file->crc_mode);
    crc = get_checksum(reinterpret_cast<const uint8_t *>(doc->key),
                       doc->length.keylen, crc, handle->file->crc_mode);
    crc = get_checksum(reinterpret_cast<const uint8_t *>(&_timestamp),
                       sizeof(timestamp_t), crc, handle->file->crc_mode);
    crc = get_checksum(reinterpret_cast<const uint8_t *>(&_seqnum),
                       sizeof(fdb_seqnum_t), crc, handle->file->crc_mode);
    crc = get_checksum(reinterpret_cast<const uint8_t *>(doc->meta),
                       doc->length.metalen, crc, handle->file->crc_mode);

    if (doc->length.flag & DOCIO_COMPRESSED) {
        crc = get_checksum(reinterpret_cast<const uint8_t *>(comp_body),
                           doc->length.bodylen_ondisk, crc,
                           handle->file->crc_mode);
        if (comp_body) {
            free(comp_body);
        }
    } else {
        crc = get_checksum(reinterpret_cast<const uint8_t *>(doc->body),
                           doc->length.bodylen, crc, handle->file->crc_mode);
    }
    if (crc != crc_file) {
        fdb_log(log_callback, FDB_LOG_ERROR, FDB_RESULT_CHECKSUM_ERROR,
                "doc_body checksum mismatch error in a database file '%s'"
                " crc %x != %x (crc in doc) keylen %d metalen %d bodylen %d "
                "bodylen_ondisk %d offset %" _F64,
                handle->file->filename, crc, crc_file, _length.keylen,
                _length.metalen, _length.bodylen, _length.bodylen_ondisk,
                offset);
        free_docio_object(doc, key_alloc, meta_alloc, body_alloc);
        return (int64_t)FDB_RESULT_CHECKSUM_ERROR;
    }
#endif

    uint8_t free_meta = meta_alloc && !doc->length.metalen;
    uint8_t free_body = body_alloc && !doc->length.bodylen;
    free_docio_object(doc, 0, free_meta, free_body);

    if (handle->file->config->kvssd) {
        _offset = offset;
    }

    return _offset;
}

// Result<void> obj_read_doc_async(struct obj_handle *handle, uint64_t offset,
//                                 struct obj_object *doc, bool
//                                 read_on_cache_miss, struct async_read_ctx_t
//                                 *args);

bool obj_check_buffer(struct obj_handle *handle, bid_t bid,
                      uint64_t sb_bmp_revnum);
