import boto3
from botocore.config import Config

s3 = boto3.client(
    "s3",
    endpoint_url="http://12.12.12.2:80",
    aws_access_key_id="50I9XIIDPA2TC8LKYUKM",
    aws_secret_access_key="J10qS4Bb4vIWxyJW3gWR4XATae0AbhRovkiiTOkj",
    config=Config(signature_version="s3v4"),
)

bucket = "bucket-data"

print(" Generating pre-signed url...")

with open("presign_urls", "a") as the_file:
    for i in range(1, 10000):
        the_file.write(
            s3.generate_presigned_url(
                "put_object",
                Params={"Bucket": bucket, "Key": str(i)},
                ExpiresIn=3600,
                HttpMethod="PUT",
            )
            + "\n"
        )
