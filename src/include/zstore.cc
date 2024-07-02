
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
#include "../s3/aws_s3_amisc.h"

using namespace std;
// Constructor
Zstore::Zstore(const std::string &name) : name(name) {}

Zstore::~Zstore()
{
    // Teardown connections, etc
}

