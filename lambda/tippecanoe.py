import os
import json
import subprocess
import sys
import boto3
from botocore import UNSIGNED
from botocore.config import Config

# if the input bucket is publicly readable
#s3 = boto3.client('s3',config=Config(signature_version=UNSIGNED))

def lambda_handler(event, context):
    bucket_name = event['Records'][0]['s3']['bucket']['name']
    input_key = event['Records'][0]['s3']['object']['key']
    tmpdir = '/tmp'
    run_tippecanoe('tippecanoe',tmpdir,bucket_name, input_key)
    return {
        'statusCode': 200,
        'body': json.dumps('Done!')
    }

# input formats: "geojsonseq", "fgb", "geobuf", "geojson", "csv"
def split_key(key):
    if key.endswith(".gz"):
        root, ext = os.path.splitext(key[0:-3])
        return root, ext + ".gz"
    else:
        return os.path.splitext(key)

def run_tippecanoe(executable, tmpdir, bucket_name, input_key):
    s3 = boto3.client('s3')
    root, ext = split_key(input_key)
    input_path = os.path.join(tmpdir,'input' + ext)
    output_path = os.path.join(tmpdir,'output.mbtiles')
    s3.download_file(bucket_name,input_key,input_path)
    p = subprocess.Popen([executable,'-o',output_path,input_path,'--force','-u','-U','5'],stderr=subprocess.PIPE)
    for line in p.stderr:
        try:
            j = json.loads(line)
            s3.put_object(Body=json.dumps(j),Bucket=os.environ['OUTPUT_BUCKET'],Key=root + ".progress",ACL="public-read",ContentType="application/json",ContentDisposition="inline")
        except json.decoder.JSONDecodeError:
            pass
    s3.upload_file(output_path,os.environ['OUTPUT_BUCKET'],root + ".mbtiles")

if __name__ == "__main__":
    tmpdir = 'tmp'
    bucket_name = sys.argv[1]
    input_key = sys.argv[2]
    run_tippecanoe('../tippecanoe', tmpdir, bucket_name, input_key)
