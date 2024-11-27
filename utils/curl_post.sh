import boto3
client = boto3.client("s3")

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
