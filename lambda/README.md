# Build the tippecanoe layer (binary)

1. On an AWS Linux 2 instance or image:

    sudo yum update
    sudo yum install git clang sqlite-devel zlib-devel

2. Create a ZIP archive with the `tippecanoe` binary at `bin/tippecanoe` and upload as a Lambda layer
3. Copy [lambda_function.py](lambda_function.py) (python 3.9 runtime)
4. set the environment variable `OUTPUT_BUCKET` to your output bucket; give lambda IAM role access to input/output buckets
5. Enable ACLs on the output bucket

```json
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Sid": "AllowGet",
            "Effect": "Allow",
            "Action": "s3:GetObject",
            "Resource": "arn:aws:s3:::protomaps-tippecanoe-test/*"
        },
        {
            "Sid": "AllowPut",
            "Effect": "Allow",
            "Action": [
                "s3:PutObject",
                "s3:PutObjectAcl"
            ],
            "Resource": "arn:aws:s3:::protomaps-tippecanoe-test-output/*"
        }
    ]
}
```