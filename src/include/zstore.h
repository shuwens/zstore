#pragma once

#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <CivetServer.h>
// #include <civetweb.h>
// #include <curlpp/Easy.hpp>

#include "../s3/aws_s3.h"
#include "../s3/multidict.h"

// typedef cURLpp::Easy Zstore_Connection;
typedef mg_connection Zstore_Connection;

class Zstore
{
  private:
    std::string name;

    void Send(const std::string &url, const std::string &uri,
              const std::string &method, AWS_IO &io, Zstore_Connection **conn);

  public:
    Zstore(const std::string &name);
    ~Zstore();

    std::list<AWS_S3_Bucket> buckets;

    // std::list<AWS_S3_Bucket> & GetBuckets(bool getContents, bool refresh,
    //     Zstore_Connection ** conn = NULL);
    // void RefreshBuckets(bool getContents, Zstore_Connection ** conn = NULL);
    //
    // void GetBucketContents(AWS_S3_Bucket & bucket, Zstore_Connection ** conn
    // = NULL);

    //    void GetObjectInfo(std::string & bktName, std::string & key,
    //                       AWS_S3_Object & bucket, Zstore_Connection ** conn =
    //                       NULL) {
    //        AWS_S3_Bucket bucket(bktName, "");
    //        aws.GetBucketContents(bucket);
    //    }

    // To perform multiple operations on the same connection, provide a pointer
    // to a pointer to an Zstore_Connection as the last parameter, initialized
    // to NULL: Zstore_Connection * conn = NULL; PutObject("bucket", "key", io,
    // &conn); PutObject("bucket", "key2", io, &conn); The first operation will
    // create the Zstore_Connection. The user should delete the connection
    // themselves after they are done.

    // Upload object
    void PutObject(const std::string &bkt, const std::string &key, AWS_IO &io,
                   Zstore_Connection **reqPtr = NULL);
    void PutObject(const std::string &bkt, const std::string &key,
                   const std::string &acl, const std::string &localpath,
                   AWS_IO &io, Zstore_Connection **reqPtr = NULL);

    // Get object data (GET /key)
    void GetObject(const std::string &bkt, const std::string &key, AWS_IO &io,
                   Zstore_Connection **reqPtr = NULL);

    // Get meta-data on object (HEAD)
    // Headers are same as for GetObject(), but no data is retrieved.
    void GetObjectMData(const std::string &bkt, const std::string &key,
                        AWS_IO &io, Zstore_Connection **reqPtr = NULL);

    // Delete object (DELETE)
    void DeleteObject(const std::string &bkt, const std::string &key,
                      AWS_IO &io, Zstore_Connection **reqPtr = NULL);
};
