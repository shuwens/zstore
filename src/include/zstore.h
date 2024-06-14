#pragma once

#include <errno.h>
#include <fstream>
#include <iostream>
#include <libxnvme.h>
#include <list>
#include <map>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>
#include <sstream>
#include <string>
#include <vector>

#include "item.h"
#include "global.h"

#include <CivetServer.h>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
// #include <civetweb.h>
// #include <curlpp/Easy.hpp>

#include "../s3/aws_s3.h"
#include "../s3/aws_s3_misc.h"
#include "../s3/multidict.h"

#include "CivetServer.h"
#include <cstring>
#include <unistd.h>

#define EVP_MAX_MD_SIZE 64 /* SHA512 */

// typedef cURLpp::Easy Zstore_Connection;
typedef mg_connection Zstore_Connection;

class Zstore : public CivetHandler
{
private:
    std::string name;
    int verbose;

    // int DoRequest(struct mg_connection *conn);

    // std::string GenRequestSignature(const AWS_IO &io, const std::string &uri,
    //                                 const std::string &mthd);

public:
    Zstore(const std::string &name);
    ~Zstore();

    void SetVerbosity(int v) { verbose = v; }
    std::list<AWS_S3_Bucket> buckets;

    // Upload object
    // void PutObject(const std::string &bkt, const std::string &key, AWS_IO
    // &io,
    //                Zstore_Connection **reqPtr = NULL);
    // void PutObject(const std::string &bkt, const std::string &key,
    //                const std::string &localpath, AWS_IO &io,
    //                Zstore_Connection **reqPtr = NULL);

    // Get object data (GET /key)
    // void GetObject(const std::string &bkt, const std::string &key, AWS_IO
    // &io,
    //                Zstore_Connection **reqPtr = NULL);

    // Get meta-data on object (HEAD)
    // Headers are same as for GetObject(), but no data is retrieved.
    // void GetObjectMData(const std::string &bkt, const std::string &key,
    //                     AWS_IO &io, Zstore_Connection **reqPtr = NULL);

    // Delete object (DELETE)
    // void DeleteObject(const std::string &bkt, const std::string &key,
    //                   AWS_IO &io, Zstore_Connection **reqPtr = NULL);
};
