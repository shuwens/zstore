
# https://repost.aws/questions/QU2Yq1fA_mRB-yAqC2_XpkVw/s3-signed-urls-via-the-cli-for-put-using-curl
import boto3
client = boto3.client("s3")

print("\nGenerating presigned get for test/dpdk.sh\n")
info = client.generate_presigned_url(
    ClientMethod="get_object",
    Params={"Bucket": "test", "Key": "dpdk.sh"},
    ExpiresIn=3600,
)
print(info)
# info = client.generate_presigned_get(Bucket="test", Key="dpdk.sh")
# print(" ".join(f"{k}='{v}'" for (k, v) in info["fields"].items()))


print("\nGenerating presigned delete for test/dpdk.sh\n")
info = client.generate_presigned_url(
    ClientMethod="delete_object",
    Params={"Bucket": "test", "Key": "dpdk.sh"},
    ExpiresIn=3600,
)
print(info)
# print(" ".join(f"{k}='{v}'" for (k, v) in info["fields"].items()))


print("\nGenerating presigned post for test/dpdk.sh\n")
# get presigned info
# note other params can be passed here, including expiration
# see https://boto3.amazonaws.com/v1/documentation/api/latest/reference/services/s3.html#S3.Client.generate_presigned_post
info = client.generate_presigned_post(Bucket="test", Key="dpdk.sh")

# print the "fields" out in the right format to feed to httpie
print(" ".join(f"{k}='{v}'" for (k, v) in info["fields"].items()))

# for curl, use this
print(" ".join(f"-F {k}='{v}'" for (k, v) in info["fields"].items()))

# confirm the URL
print(info["url"])
