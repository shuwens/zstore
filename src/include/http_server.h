#pragma once

#include "boost_utils.h"
#include "common.h"
#include "configuration.h"
#include "global.h"
#include "object.h"
#include "tinyxml2.h"
#include "zstore_controller.h"
#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <chrono>
#include <functional>
#include <regex>
#include <utility>

using namespace boost::asio::experimental::awaitable_operators;
namespace http = boost::beast::http; // from <boost/beast/http.hpp>

using HttpMsg = http::message_generator;
using tcp = asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
using tcp_stream = typename boost::beast::tcp_stream::rebind_executor<
    asio::use_awaitable_t<>::executor_with_default<asio::any_io_executor>>::
    other;

void async_sleep_impl(
    asio::any_completion_handler<void(boost::system::error_code)> handler,
    asio::any_io_executor ex, std::chrono::nanoseconds duration)
{
    auto timer = std::make_shared<asio::steady_timer>(ex, duration);
    timer->async_wait(asio::consign(std::move(handler), timer));
}

template <typename CompletionToken>
inline auto async_sleep(asio::any_io_executor ex,
                        std::chrono::nanoseconds duration,
                        CompletionToken &&token)
{
    return asio::async_initiate<CompletionToken,
                                void(boost::system::error_code)>(
        async_sleep_impl, token, std::move(ex), duration);
}

// TODO: use max keys
void create_s3_list_objects_response(
    tinyxml2::XMLDocument &doc, std::string bucket_name, int max_keys,
    std::vector<ObjectKeyHash> object_key_hashes)
{
    // Add the root element <ListBucketResult> with namespace
    tinyxml2::XMLElement *root = doc.NewElement("ListBucketResult");
    root->SetAttribute("xmlns", "http://s3.amazonaws.com/doc/2006-03-01/");
    doc.InsertFirstChild(root);

    // Add the <Name> element (bucket name)
    tinyxml2::XMLElement *name = doc.NewElement("Name");
    name->SetText(bucket_name.c_str());
    root->InsertEndChild(name);

    // Add <Prefix> element
    tinyxml2::XMLElement *prefix = doc.NewElement("Prefix");
    prefix->SetText(""); // No prefix in this example
    root->InsertEndChild(prefix);

    // Add <Marker> element
    tinyxml2::XMLElement *marker = doc.NewElement("Marker");
    marker->SetText(""); // No marker in this example
    root->InsertEndChild(marker);

    // Add <MaxKeys> element
    tinyxml2::XMLElement *maxKeys = doc.NewElement("MaxKeys");
    maxKeys->SetText(std::to_string(max_keys).c_str());
    root->InsertEndChild(maxKeys);

    // Add <IsTruncated> element
    tinyxml2::XMLElement *isTruncated = doc.NewElement("IsTruncated");
    isTruncated->SetText("false"); // Not truncated in this example
    root->InsertEndChild(isTruncated);

    std::string _lastModified = "2024-01-10T15:30:00.000Z";
    std::string _eTag = "\"9b2cf535f27731c974343645a3985328\"";
    int _size = 123456;
    std::string _storageClass = "STANDARD";

    // Add <Contents> elements for each object
    for (const auto &hash : object_key_hashes) {
        tinyxml2::XMLElement *contents = doc.NewElement("Contents");

        tinyxml2::XMLElement *key = doc.NewElement("Key");
        key->SetText(hashToCString(hash).c_str());
        contents->InsertEndChild(key);

        tinyxml2::XMLElement *lastModified = doc.NewElement("LastModified");
        lastModified->SetText(_lastModified.c_str());
        contents->InsertEndChild(lastModified);

        tinyxml2::XMLElement *eTag = doc.NewElement("ETag");
        eTag->SetText(_eTag.c_str());
        contents->InsertEndChild(eTag);

        tinyxml2::XMLElement *size = doc.NewElement("Size");
        size->SetText(std::to_string(_size).c_str());
        contents->InsertEndChild(size);

        tinyxml2::XMLElement *storageClass = doc.NewElement("StorageClass");
        storageClass->SetText(_storageClass.c_str());
        contents->InsertEndChild(storageClass);

        root->InsertEndChild(contents);
    }
}

std::pair<std::string, std::string> parse_url(const std::string &str)
{
    // Check if the string starts with "/"
    if (str.empty() || str[0] != '/') {
        return {"", ""}; // Return empty strings if the format is invalid
    }

    // Find the position of the first delimiter after the initial '/'
    size_t firstPos = str.find('/', 1); // Start searching from index 1

    if (firstPos == std::string::npos) {
        // Case: "/aaa" format, where there's no second "/"
        return {str.substr(1),
                ""}; // Extract the substring after the initial '/'
    } else {
        // Case: "/aa/bb" format
        std::string part1 = str.substr(
            1,
            firstPos - 1); // Extract between the initial '/' and the second '/'
        std::string part2 = str.substr(
            firstPos + 1); // Extract the remainder after the second '/'
        return {part1, part2};
    }
}

// This function implements the core logic of async
auto awaitable_on_request(HttpRequest req, ZstoreController &zctrl_)
    -> asio::awaitable<HttpMsg>
{
    auto url = req.target();
    auto [bucket, object_key] = parse_url(url);
    if (Configuration::Debugging())
        log_debug("Bucket: {}, Object Key: {}, url {}", bucket, object_key,
                  url);
    ObjectKeyHash key_hash = computeSHA256(object_key);

    if (Configuration::Debugging()) {
        if (req.method() == http::verb::get)
            log_debug("req {} target {}, body {}", "GET", req.target(),
                      req.body());
        else if (req.method() == http::verb::post)
            log_debug("req {} target {}, body ", "POST", req.target());
        else if (req.method() == http::verb::put)
            log_debug("req {} target {}, body ", "PUT", req.target());
        else if (req.method() == http::verb::delete_)
            log_debug("req {} target {}, body {}", "DELETE", req.target(),
                      req.body());
        // co_return handle_request(std::move(req));
    }

    if (req.method() == http::verb::get) {
        if (object_key.contains("?max-keys=")) {
            // TODO: not sure if this is correct

            // List operation
            std::regex pattern(R"(^\/([^?]+)\?max-keys=(\d+))");
            std::smatch matches;
            std::string str(req.target());
            if (std::regex_match(str, matches, pattern)) {
                // The bucket name is the first capture group, max-keys is
                // the second
                std::string bucket_name = matches[1];
                int max_keys = std::stoi(matches[2]);

                std::cout << "Bucket Name: " << bucket_name << std::endl;
                std::cout << "Max Keys: " << max_keys << std::endl;

                tinyxml2::XMLDocument doc;

                auto object_key_hashes = zctrl_.ListObjects().value();
                create_s3_list_objects_response(doc, bucket_name, max_keys,
                                                object_key_hashes);

                // Convert the XML document to a string
                tinyxml2::XMLPrinter printer;
                doc.Print(&printer);
                std::string xml_content = printer.CStr();
                req.body() = xml_content;

                co_return handle_request(std::move(req));

            } else {
                std::cerr << "URL does not match the expected format"
                          << std::endl;
                co_return handle_request(std::move(req));
            }
        }

        // Get bucket will always return 404
        if (object_key == "") {
            if (Configuration::Debugging())
                log_error(
                    "Object key is empty. Ignoring the request as we dont "
                    "care about bucket {}.",
                    bucket);
            // We ignore Get bucket
            co_return handle_not_found_request(std::move(req));
        }

        // NOTE: READ path: see section 3.4
        auto e = zctrl_.GetObject(key_hash);
        MapEntry entry;

        if (!e.has_value()) {
            if (Configuration::Debugging())
                log_error("GET: Object {} not found", object_key);
            // co_return handle_not_found_request(std::move(req));
            entry = createMapEntry(
                        zctrl_.GetDevTupleForRandomReads(key_hash).value(),
                        zctrl_.mTotalCounts - 1, 1, zctrl_.mTotalCounts, 1,
                        zctrl_.mTotalCounts + 1, 1)
                        .value();
        } else {
            entry = e.value();
        }

        if (zctrl_.SearchRecentWriteMap(key_hash).value()) {
            if (zctrl_.mOption == 3 || zctrl_.mKeyExperiment == 6) {
                // log_info("Object {} is recently modified", object_key);
                // log_error("Unimplemented!!!");
            } else {
                log_info("Object {} is recently modified", object_key);
                log_error("Unimplemented!!!");
            }
        }

        auto [first, _, _] = entry;
        auto [tgt, lba, _] = first;
        if (Configuration::Debugging())
            log_debug("Reading from tgt {} lba {}", tgt, lba);

        auto dev1 = zctrl_.GetDevice(tgt);
        auto s1 = MakeReadRequest(&zctrl_, dev1, lba).value();
        co_await zoneRead(s1);
        co_await async_sleep(co_await asio::this_coro::executor,
                             std::chrono::microseconds(0), asio::use_awaitable);
        assert(s1->success && "Read request failed");
        // s1->response_body.assign(s1->dataBuffer, s1->bufferSize);

        // Here is the part where we operate differently based on object
        // size
        // if (Configuration::GetObjectSizeInBytes() >
        //     Configuration::GetChunkSize()) {
        //     if (Configuration::Debugging())
        //         log_debug("Object is larger than chunk size");
        //     // Object is larger than chunk size, we need to fetch chunk list
        //     // and read each chunk, and merge chunks into a single object
        //     // if (res.has_value()) {
        //     u64 num_chunks = Configuration::GetObjectSizeInBytes() /
        //                      Configuration::GetChunkSize();
        //     if (Configuration::Debugging())
        //         log_debug("Num chunks {}", num_chunks);
        //     // ChunkList chunk_list_read =
        //     deserializeMap(&s1->response_body); ChunkList chunk_list_read =
        //         deserializeDummyMap(s1->response_body, num_chunks);
        //     s1->Clear();
        //     zctrl_.mRequestContextPool->ReturnRequestContext(s1);
        //
        //     // u64 remaining_data_len =
        //     Configuration::GetObjectSizeInBytes();
        //     // if (Configuration::Debugging())
        //     //     log_debug("Remaining data len {}",
        //     //               remaining_data_len /
        //     //               Configuration::GetBlockSize());
        //
        //     std::vector<RequestContext *> chunk_read_reqs;
        //     for (u64 i = 0; i < num_chunks; i++) {
        //         auto [_, lba] = chunk_list_read[i];
        //         if (Configuration::Debugging())
        //             log_debug("Reading chunk lba {}", lba);
        //         auto slot = MakeReadRequest(&zctrl_, dev1, lba).value();
        //         chunk_read_reqs.push_back(slot);
        //     }
        //
        //     auto ex = co_await asio::this_coro::executor;
        //     using Task = decltype(co_spawn(ex, zoneRead(s1),
        //     asio::deferred)); std::vector<Task> chunks_to_read; for (auto
        //     &slot : chunk_read_reqs) {
        //         chunks_to_read.push_back(
        //             co_spawn(ex, zoneRead(slot), asio::deferred));
        //     }
        //
        //     // Await them all and let them execute in parallel
        //     auto grp = asio::experimental::make_parallel_group(
        //         std::move(chunks_to_read));
        //
        //     auto rr = co_await (grp.async_wait(
        //         asio::experimental::wait_for_all(), asio::use_awaitable));
        //
        //     // TODO: check for success
        //     mergeChunksIntoObject(chunk_read_reqs, req.body());
        //     for (auto &slot : chunk_read_reqs) {
        //         slot->Clear();
        //         zctrl_.mRequestContextPool->ReturnRequestContext(slot);
        //     }
        //
        //     co_return handle_request(std::move(req));
        //     // } else {
        //     //     // yields 378k IOPS
        //     //     s1->Clear();
        //     //     zctrl_.mRequestContextPool->ReturnRequestContext(s1);
        //     //     co_return handle_not_found_request(std::move(req));
        //     // }
        // } else {
        // Single read since object is smaller than chunk size
        // if (res.has_value()) {
        // yields 320 to 310k IOPS
        // ZstoreObject deserialized_obj;
        // bool success = ReadBufferToZstoreObject(s1->dataBuffer, s1->size,
        // deserialized_obj);
        req.body() = s1->response_body; // not expensive
        s1->Clear();
        zctrl_.mRequestContextPool->ReturnRequestContext(s1);
        co_return handle_request(std::move(req));
        // } else {
        //     // yields 378k IOPS
        //     s1->Clear();
        //     zctrl_.mRequestContextPool->ReturnRequestContext(s1);
        //     co_return handle_not_found_request(std::move(req));
        // }
        // }
    } else if (req.method() == http::verb::post ||
               req.method() == http::verb::put) {
        if (object_key == "") {
            if (Configuration::Debugging())
                log_error(
                    "Object key is empty. Ignoring the request as we dont "
                    "care about bucket {}.",
                    bucket);
            // We ignore Put bucket
            co_return handle_request(std::move(req));
        }

        if (Configuration::Debugging())
            // log_debug("key {}, key hash {}, value {}", object_key, key_hash,
            //           req.body());
            log_debug("key {}, key hash {}", object_key, key_hash);

        // NOTE: Write path: see section 3.3
        auto object_value = req.body();

        // TODO:  populate the map with consistent hashes
        auto dev_tuple = zctrl_.GetDevTuple(key_hash).value();
        // auto dev_tuple =
        // zctrl_.GetDevTupleForRandomReads(key_hash).value();

        auto entry = zctrl_.CreateFakeObject(key_hash, dev_tuple).value();
        auto [first, second, third] = entry;
        auto [tgt1, _, _] = first;
        auto [tgt2, _, _] = second;
        auto [tgt3, _, _] = third;

        // update and broadcast BF
        auto rc2 = zctrl_.UpdateRecentWriteMap(key_hash);
        assert(rc2.has_value());

        auto dev1 = zctrl_.GetDevice(tgt1);
        auto dev2 = zctrl_.GetDevice(tgt2);
        auto dev3 = zctrl_.GetDevice(tgt3);

        // Here is the part where we operate differently based on object
        // size
        if (Configuration::GetObjectSizeInBytes() >
            Configuration::GetChunkSize()) {
            u64 num_chunks = Configuration::GetObjectSizeInBytes() /
                             Configuration::GetChunkSize();

            ZstoreObject obj;
            obj.entry.type = LogEntryType::kData;
            obj.entry.seqnum = 42;
            obj.entry.chunk_seqnum = 24;
            obj.datalen = Configuration::GetObjectSizeInBytes();
            obj.body = std::malloc(obj.datalen);
            std::memset(obj.body, req.body().data()[0],
                        obj.datalen); // Fill with example data (0xCD)
            // std::strcpy(original_obj.key_hash, key_hash);
            obj.key_size = kHashSize;
            if (Configuration::Debugging())
                log_debug("Object size: {}", obj.datalen);

            std::vector<ZstoreObject> chunk_vec = splitObjectIntoChunks(obj);
            if (Configuration::Debugging())
                log_debug("Chunk vec size: {}", chunk_vec.size());
            std::free(obj.body);

            // u64 remaining_data_len = Configuration::GetObjectSizeInBytes();
            // if (Configuration::Debugging())
            //     log_debug("Remaining data len {}",
            //               remaining_data_len /
            //               Configuration::GetBlockSize());

            std::vector<RequestContext *> chunk_write_reqs;
            chunk_write_reqs.reserve(num_chunks * 3);
            char *buffer =
                (char *)spdk_zmalloc(Configuration::GetObjectSizeInBytes(),
                                     Configuration::GetBlockSize(), NULL,
                                     SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
            if (Configuration::Debugging())
                log_debug("Buffer size: {}, chunk write req: {}",
                          Configuration::GetObjectSizeInBytes(),
                          num_chunks * 3);

            for (u64 i = 0; i < num_chunks; i++) {
                if (Configuration::Debugging())
                    log_debug("Chunk vec size: {}", chunk_vec.size());
                buffer = (char *)chunk_vec[i].body;

                auto s1 = MakeWriteChunk(&zctrl_, dev1, buffer).value();
                auto s2 = MakeWriteChunk(&zctrl_, dev2, buffer).value();
                auto s3 = MakeWriteChunk(&zctrl_, dev3, buffer).value();
                chunk_write_reqs.push_back(s1);
                chunk_write_reqs.push_back(s2);
                chunk_write_reqs.push_back(s3);
                if (Configuration::Debugging())
                    log_debug("Chunk write reqs size: {}",
                              chunk_write_reqs.size());
            }

            auto ex = co_await asio::this_coro::executor;
            using Task = decltype(co_spawn(ex, zoneAppend(chunk_write_reqs[0]),
                                           asio::deferred));
            std::vector<Task> chunks_to_write;
            for (auto &slot : chunk_write_reqs) {
                chunks_to_write.push_back(
                    co_spawn(ex, zoneAppend(slot), asio::deferred));
            }
            if (Configuration::Debugging())
                log_debug("1111");
            // Await them all and let them execute in parallel
            auto grp = asio::experimental::make_parallel_group(
                std::move(chunks_to_write));
            if (Configuration::Debugging())
                log_debug("2222");

            auto rr = co_await (grp.async_wait(
                asio::experimental::wait_for_all(), asio::use_awaitable));
            if (Configuration::Debugging())
                log_debug("2222");
            for (auto &slot : chunk_write_reqs) {
                slot->Clear();
                zctrl_.mRequestContextPool->ReturnRequestContext(slot);
            }
            if (Configuration::Debugging())
                log_debug("2222");

            // TODO: why is std map default size 48?
            // write the chunk list
            ChunkList chunk_list;
            if (Configuration::Debugging())
                log_info("11111 Chunk list size: {}", sizeof(chunk_list));
            for (u64 i = 0; i < num_chunks; i++) {
                // log_info("Chunk list: {} {}", i, kChunkSize);
                if (chunk_write_reqs[i]->success)
                    chunk_list[i] =
                        std::make_tuple(i, chunk_write_reqs[i]->append_lba);
                else
                    chunk_list[i] = std::make_tuple(i, 42);
            }
            std::memcpy(buffer,
                        serializeMap(chunk_list, Configuration::GetChunkSize()),
                        Configuration::GetChunkSize());

            if (Configuration::Debugging())
                log_info("22222 Chunk list size: {}", sizeof(chunk_list));

            auto s1 = MakeWriteChunk(&zctrl_, dev1, buffer).value();
            auto s2 = MakeWriteChunk(&zctrl_, dev2, buffer).value();
            auto s3 = MakeWriteChunk(&zctrl_, dev3, buffer).value();
            if (Configuration::Debugging())
                log_info("33333 Chunk list size: {}", sizeof(chunk_list));
            co_await (zoneAppend(s1) && zoneAppend(s2) && zoneAppend(s3));
            assert(s1->success && s2->success && s3->success &&
                   "Write request failed");

            co_await async_sleep(co_await asio::this_coro::executor,
                                 std::chrono::microseconds(0),
                                 asio::use_awaitable);
            if (Configuration::Debugging())
                log_info("44444 Chunk list size: {}", sizeof(chunk_list));
            auto new_entry =
                createMapEntry(
                    std::make_tuple(std::make_pair(tgt1, dev1->GetZoneId()),
                                    std::make_pair(tgt2, dev2->GetZoneId()),
                                    std::make_pair(tgt3, dev3->GetZoneId())),
                    s1->append_lba, 1, s2->append_lba, 1, s3->append_lba, 1)
                    .value();

            s1->Clear();
            s2->Clear();
            s3->Clear();
            zctrl_.mRequestContextPool->ReturnRequestContext(s1);
            zctrl_.mRequestContextPool->ReturnRequestContext(s2);
            zctrl_.mRequestContextPool->ReturnRequestContext(s3);
            if (Configuration::Debugging())
                log_info("55555 Chunk list size: {}", sizeof(chunk_list));
            // update lba in map
            // auto rc = zctrl_.PutObject(key_hash, new_entry).value();

            // spdk_free(buffer);
            // chunk_write_reqs.clear();
            // chunk_vec.clear();
            co_return handle_request(std::move(req));

        } else {
            if (Configuration::Debugging()) {
                log_debug("Writing to tgt1 {} tgt2 {} tgt3 {}", tgt1, tgt2,
                          tgt3);
                // log_debug("Writing to dev1 {} dev2 {} dev3 {}",
                // dev1->GetZoneId(),
                //           dev2->GetZoneId(), dev3->GetZoneId());
            }

            // TODO
            ZstoreObject original_obj;
            original_obj.entry.type = LogEntryType::kData;
            original_obj.entry.seqnum = 42;
            original_obj.entry.chunk_seqnum = 24;
            original_obj.datalen = Configuration::GetObjectSizeInBytes();
            original_obj.body = std::malloc(original_obj.datalen);
            std::memset(original_obj.body, req.body().data()[0],
                        original_obj.datalen); // Fill with example data (0xCD)
            // std::strcpy(original_obj.key_hash, key_hash);
            original_obj.key_size = kHashSize;
            // static_cast<uint16_t>(std::strlen(original_obj.key_hash));

            // 2. Serialize to buffer
            std::vector<char> buffer = WriteZstoreObjectToBuffer(original_obj);

            auto dev = zctrl_.GetDevice("Zstore2Dev1");
            // debug_buffer

            // log_debug("Writing buffer {}, body {}", buffer, req.body());

            auto s1 = MakeWriteRequest(&zctrl_, dev, req).value();
            // auto s2 = MakeWriteRequest(&zctrl_, dev2, req).value();
            // auto s3 = MakeWriteRequest(&zctrl_, dev3, req).value();

            // log_debug("dispatching write requests");
            // co_await (zoneAppend(s1) && zoneAppend(s2) && zoneAppend(s3));
            //             assert(s1->success && s2->success && s3->success &&
            //                    "Write request failed");

            co_await (zoneAppend(s1));
            assert(s1->success && "Write request failed");

            co_await async_sleep(co_await asio::this_coro::executor,
                                 std::chrono::microseconds(0),
                                 asio::use_awaitable);

            // auto new_entry =
            //     createMapEntry(
            //         std::make_tuple(std::make_pair(tgt1, dev1->GetZoneId()),
            //                         std::make_pair(tgt2, dev2->GetZoneId()),
            //                         std::make_pair(tgt3, dev3->GetZoneId())),
            //         s1->append_lba, 1, s2->append_lba, 1, s3->append_lba, 1)
            //         .value();

            auto new_entry =
                createMapEntry(
                    std::make_tuple(std::make_pair(tgt1, dev1->GetZoneId()),
                                    std::make_pair(tgt1, dev1->GetZoneId()),
                                    std::make_pair(tgt1, dev1->GetZoneId())),
                    s1->append_lba, 1, s1->append_lba, 1, s1->append_lba, 1)
                    .value();

            s1->Clear();
            zctrl_.mRequestContextPool->ReturnRequestContext(s1);
            // s2->Clear();
            // zctrl_.mRequestContextPool->ReturnRequestContext(s2);
            // s3->Clear();
            // zctrl_.mRequestContextPool->ReturnRequestContext(s3);

            // update lba in map
            auto rc = zctrl_.PutObject(key_hash, new_entry).value();
            co_return handle_request(std::move(req));
        }
    } else if (req.method() == http::verb::delete_) {
        if (Configuration::Debugging())
            log_debug("Delete request for key {}", key_hash);
        MapEntry entry = zctrl_.DeleteObject(key_hash).value();
        auto [first, second, third] = entry;
        auto rc1 = zctrl_.AddGcObject(first);
        auto rc2 = zctrl_.AddGcObject(second);
        auto rc3 = zctrl_.AddGcObject(third);
        assert(rc1.has_value() && rc2.has_value() && rc3.has_value() &&
               "Add all writes to GC map");

        // TODO: We need to put stuff into GC map
        co_return handle_request(std::move(req));
    }

    else {
        log_error("Request is not a supported operation\n");
        co_return handle_request(std::move(req));
    }
}

// Handles an HTTP server connection
asio::awaitable<void> do_session(tcp_stream stream, ZstoreController &zctrl)
{
    // This buffer is required to persist across reads
    beast::flat_buffer buffer;

    // This lambda is used to send messages
    try {
        for (;;) {
            // Set the timeout.
            stream.expires_after(std::chrono::seconds(30));

            // Read a request
            HttpRequest req;
            co_await http::async_read(stream, buffer, req);

            // HttpMsg msg = co_await
            // (awaitable_on_request(std::move(req)));
            HttpMsg msg =
                co_await (awaitable_on_request(std::move(req), zctrl));
            // fmt::print("Result: {}\n", msg);

            // Determine if we should close the connection
            bool keep_alive = msg.keep_alive();

            // Send the response
            co_await beast::async_write(stream, std::move(msg),
                                        asio::use_awaitable);

            if (!keep_alive) {
                // This means we should close the connection, usually
                // because the response indicated the "Connection: close"
                // semantic.
                break;
            }
        }
    } catch (boost::system::system_error &se) {
        if (se.code() != http::error::end_of_stream)
            throw;
    }

    // Send a TCP shutdown
    boost::beast::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_send, ec);

    // At this point the connection is closed gracefully
    // we ignore the error because the client might have
    // dropped the connection already.
}

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
asio::awaitable<void> do_listen(tcp::endpoint endpoint, ZstoreController &zctrl)
{
    // Open the acceptor
    auto acceptor = asio::use_awaitable.as_default_on(
        tcp::acceptor(co_await asio::this_coro::executor));
    acceptor.open(endpoint.protocol());

    // Allow address reuse
    acceptor.set_option(asio::socket_base::reuse_address(true));

    // Bind to the server address
    acceptor.bind(endpoint);

    // Start listening for connections
    acceptor.listen(asio::socket_base::max_listen_connections);

    for (;;)
        asio::co_spawn(
            acceptor.get_executor(),
            do_session(tcp_stream(co_await acceptor.async_accept()), zctrl),
            [](std::exception_ptr e) {
                if (e)
                    try {
                        std::rethrow_exception(e);
                    } catch (std::exception &e) {
                        std::cerr << "Error in session: " << e.what() << "\n";
                    }
            });
}
