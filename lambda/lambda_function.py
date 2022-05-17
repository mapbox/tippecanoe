import os
import json
import subprocess
import boto3
# if the input bucket is publicly readable
# from botocore import UNSIGNED
# from botocore.config import Config

def lambda_handler(event, context):
    bucket = event['Records'][0]['s3']['bucket']['name']
    key = event['Records'][0]['s3']['object']['key']
    #s3 = boto3.client('s3',config=Config(signature_version=UNSIGNED))
    s3 = boto3.client('s3')
    s3.download_file(bucket,key,'/tmp/input.geojsonseq')
    p = subprocess.check_call(['tippecanoe','-o','/tmp/output.mbtiles','/tmp/input.geojsonseq','--force'])
    s3.upload_file('/tmp/output.mbtiles',os.environ['OUTPUT_BUCKET'],key + ".mbtiles")
    return {
        'statusCode': 200,
        'body': json.dumps('Done!')
    }
