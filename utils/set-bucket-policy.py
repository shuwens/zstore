# https://stackoverflow.com/questions/50400687/ceph-radosgw-bucket-policy-make-all-objects-public-read-by-default
# https://stackoverflow.com/questions/57626977/aws-s3-access-denied-when-open-object-url-in-browser


import boto3
import json

access_key = "50I9XIIDPA2TC8LKYUKM"
secret_key = "J10qS4Bb4vIWxyJW3gWR4XATae0AbhRovkiiTOkj"

conn = boto3.client('s3', 'default',
                    endpoint_url="http://12.12.12.2",
                    aws_access_key_id = access_key,
                    aws_secret_access_key = secret_key)

bucket= "bucket-data"

bucket_policy = {
  "Version":"2012-10-17",
  "Statement":[
    {
      "Sid":"AddPerm",
      "Effect":"Allow",
      "Principal": "*",
      "Action":["s3:GetObject", "s3:PutObject", "s3:PutObjectAcl", "iam:CreateUser"],
      "Resource":["arn:aws:s3:::{0}/*".format(bucket)]
    }
  ]
}

bucket_policy = json.dumps(bucket_policy)
conn.put_bucket_policy(Bucket=bucket, Policy=bucket_policy)
