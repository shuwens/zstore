
#include <errno.h>
#include <libxnvme.h>

#include <iostream>
#include <string>
#include <openssl/md5.h>
#include <openssl/buffer.h>
#include <openssl/hmac.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>

#include "../include/utils.h"
#include "../include/zstore.h"
#include "../s3/aws_s3_misc.h"

using namespace std;


// Constructor
Zstore::Zstore(const std::string &name) : name(name) {}

Zstore::~Zstore()
{
    // Teardown connections, etc
}




// TODO: Put/Get api that we should have

//************************************************************************************************
// Objects
//************************************************************************************************

// void Zstore::PutObject(const std::string &bkt, const std::string &key,
//         AWS_IO &io, Zstore_Connection **reqPtr)
// {
//     std::ostringstream urlstrm;
//     urlstrm << "http://" << bkt << ".s3.amazonaws.com/" << key;
//
//     // if(acl != "") io.sendHeaders.Set("x-amz-acl", acl);
//
//     std::istream &fin = *io.istrm;
//     uint8_t md5[EVP_MAX_MD_SIZE];
//     size_t mdLen = ComputeMD5(md5, fin);
//     io.sendHeaders.Set("Content-MD5", EncodeB64(md5, mdLen));
//
//     fin.clear();
//     fin.seekg(0, std::ios_base::end);
//     std::ifstream::pos_type endOfFile = fin.tellg();
//     fin.seekg(0, std::ios_base::beg);
//     std::ifstream::pos_type startOfFile = fin.tellg();
//
//     io.bytesReceived = 0;
//     io.bytesToPut = static_cast<size_t>(endOfFile - startOfFile);
//
//     Send(urlstrm.str(), bkt + "/" + key, "PUT", io, reqPtr);
// }
//
// void Zstore::PutObject(const std::string &bkt, const std::string &key,
//         const std::string &path, AWS_IO &io,
//         Zstore_Connection **reqPtr)
// {
//     std::ifstream fin(path.c_str(), std::ios_base::binary | std::ios_base::in);
//     if (!fin) {
//         std::cerr << "Could not read file " << path << std::endl;
//         return;
//     }
//     io.istrm = &fin;
//     PutObject(bkt, key, io, reqPtr);
// }

// void Zstore::GetObject(const std::string &bkt, const std::string &key,
//         AWS_IO &io, Zstore_Connection **reqPtr)
// {
//     std::ostringstream urlstrm;
//     urlstrm << "http://" << bkt << ".s3.amazonaws.com/" << key;
//     Send(urlstrm.str(), bkt + "/" + key, "GET", io, reqPtr);
// }
//
// void Zstore::GetObjectMData(const std::string &bkt, const std::string &key,
//         AWS_IO &io, Zstore_Connection **reqPtr)
// {
//     std::ostringstream urlstrm;
//     urlstrm << "http://" << bkt << ".s3.amazonaws.com/" << key;
//     Send(urlstrm.str(), bkt + "/" + key, "HEAD", io, reqPtr);
// }
//
// void Zstore::DeleteObject(const std::string &bkt, const std::string &key,
//         AWS_IO &io, Zstore_Connection **reqPtr)
// {
//     std::ostringstream urlstrm;
//     urlstrm << "http://" << bkt << ".s3.amazonaws.com/" << key;
//     Send(urlstrm.str(), bkt + "/" + key, "DELETE", io, reqPtr);
// }



/*
   void AWS::ParseBucketsList(list<AWS_S3_Bucket> & buckets, const std::string &
   xml)
   {
   string::size_type crsr = 0;
   string data;
   string name, date;
   string ownerName, ownerID;
   ExtractXML(ownerID, crsr, "ID", xml);
   ExtractXML(ownerName, crsr, "DisplayName", xml);

   while(ExtractXML(data, crsr, "Name", xml))
   {
   name = data;
   if(ExtractXML(data, crsr, "CreationDate", xml))
   date = data;
   else
   date = "";
   buckets.push_back(AWS_S3_Bucket(name, date));
   }
   }

   void AWS::ParseObjectsList(list<AWS_S3_Object> & objects, const std::string &
   xml)
   {
   string::size_type crsr = 0;
   string data;

   while(ExtractXML(data, crsr, "Key", xml))
   {
   AWS_S3_Object obj;
   obj.key = data;

   if(ExtractXML(data, crsr, "LastModified", xml))
   obj.lastModified = data;
   if(ExtractXML(data, crsr, "ETag", xml)) {
// eTag starts and ends with &quot;, remove these
//obj.eTag = data;
obj.eTag = data.substr(6, data.size() - 12);
}
if(ExtractXML(data, crsr, "Size", xml))
obj.size = data;

if(ExtractXML(data, crsr, "ID", xml))
obj.ownerID = data;
if(ExtractXML(data, crsr, "DisplayName", xml))
obj.ownerDisplayName = data;

if(ExtractXML(data, crsr, "StorageClass", xml))
obj.storageClass = data;

objects.push_back(obj);
}
}

std::list<AWS_S3_Bucket> & AWS::GetBuckets(bool getContents, bool refresh,
Zstore_Connection ** conn)
{
if(refresh || buckets.empty())
RefreshBuckets(getContents, conn);
return buckets;
}

void AWS::RefreshBuckets(bool getContents, Zstore_Connection ** conn)
{
std::ostringstream bucketList;
AWS_IO io(NULL, &bucketList);
ListBuckets(io, conn);

buckets.clear();
ParseBucketsList(buckets, bucketList.str());

if(getContents) {
    list<AWS_S3_Bucket>::iterator bkt;
    for(bkt = buckets.begin(); bkt != buckets.end(); ++bkt)
        GetBucketContents(*bkt, conn);
}
}

void AWS::GetBucketContents(AWS_S3_Bucket & bucket, Zstore_Connection ** conn)
{
    std::ostringstream objectList;
    AWS_IO io(NULL, &objectList);
    ListBucket(bucket.name, io, conn);
    ParseObjectsList(bucket.objects, objectList.str());
}


// read by libcurl...read data to send
struct ReadDataCB {
    AWS_IO & io;
    ReadDataCB(AWS_IO & ioio): io(ioio) {}
    size_t operator()(char * buf, size_t size, size_t nmemb) {return
        io.Read(buf, size, nmemb);}
};

// write by libcurl...handle data received
struct WriteDataCB {
    AWS_IO & io;
    WriteDataCB(AWS_IO & ioio): io(ioio) {}
    size_t operator()(char * buf, size_t size, size_t nmemb) {return
        io.Write(buf, size, nmemb);}
};

// Handle header data.
//"The header callback will be called once for each header and
// only complete header lines are passed on to the callback."
struct HeaderCB {
    AWS_IO & io;
    HeaderCB(AWS_IO & ioio): io(ioio) {}
    size_t operator()(char * buf, size_t size, size_t nmemb) {return
        io.HandleHeader(buf, size, nmemb);}
};


void AWS::CopyObject(const std::string & srcbkt, const std::string & srckey,
        const std::string & dstbkt, const std::string & dstkey,
        bool copyMD, AWS_IO & io, Zstore_Connection ** reqPtr)
{
    std::ostringstream urlstrm;
    urlstrm << "http://" << dstbkt << ".s3.amazonaws.com/" << dstkey;
    io.sendHeaders.Set("x-amz-copy-source", string("/") + srcbkt + "/" +
            srckey); io.sendHeaders.Set("x-amz-metadata-directive", copyMD? "COPY" :
                "REPLACE");
    //    io.sendHeaders["x-amz-copy-source-if-match"] =  etag
    //    io.sendHeaders["x-amz-copy-source-if-none-match"] =  etag
    //    io.sendHeaders["x-amz-copy-source-if-unmodified-since"] =  time_stamp
    //    io.sendHeaders["x-amz-copy-source-if-modified-since"] =  time_stamp
    Send(urlstrm.str(), dstbkt + "/" + dstkey, "PUT", io, reqPtr);
}

//************************************************************************************************
// Buckets
//************************************************************************************************

void AWS::ListBuckets(AWS_IO & io, Zstore_Connection ** reqPtr)
{
    Send("http://s3.amazonaws.com/", "", "GET", io, reqPtr);
}

void AWS::CreateBucket(const std::string & bkt, AWS_IO & io, Zstore_Connection
        ** reqPtr)
{
    std::ostringstream urlstrm;
    urlstrm << "http://" << bkt << ".s3.amazonaws.com";
    io.bytesToPut = 0;
    Send(urlstrm.str(), bkt + "/", "PUT", io, reqPtr);
}

void AWS::ListBucket(const std::string & bkt, AWS_IO & io, Zstore_Connection **
        reqPtr)
{
    std::ostringstream urlstrm;
    urlstrm << "http://" << bkt << ".s3.amazonaws.com";
    Send(urlstrm.str(), bkt + "/", "GET", io, reqPtr);
}

void AWS::DeleteBucket(const std::string & bkt, AWS_IO & io, Zstore_Connection
        ** reqPtr)
{
    std::ostringstream urlstrm;
    urlstrm << "http://" << bkt << ".s3.amazonaws.com";
    Send(urlstrm.str(), bkt + "/", "DELETE", io, reqPtr);
}

*/
