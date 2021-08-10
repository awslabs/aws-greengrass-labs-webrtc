# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License").
# You may not use this file except in compliance with the License.
# A copy of the License is located at
#
# http://aws.amazon.com/apache2.0
#
# or in the "license" file accompanying this file. This file is distributed
# on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
# express or implied. See the License for the specific language governing
# permissions and limitations under the License.
#!/bin/bash
if [ $# -ne 4 ]; then
  echo 1>&2 "Usage: $0 AwsProfile SecretFileName SecretName ComponentArtifactsBucket"
  exit 3
fi
# aws profile
AwsProfile=$1
SecretFileName=$2
SecretName=$3
# information of this component
ComponentArtifactsBucket=$4
# path to necessary folder
TempPath="temp"
AwsRegion=$(aws configure get $AwsProfile.region)
# secret manager related
SecretARN="MySecretManager"
ListSecrets="ListSecrets"
# create temp patch
ListBuckets="ListBuckets"

mkdir -p $TempPath

SecretExisted=0
aws --profile $AwsProfile secretsmanager list-secrets > $TempPath/$ListSecrets.json
ListedSecretName=($(jq -r '.SecretList[].Name' $TempPath/$ListSecrets.json))

for i in "${ListedSecretName[@]}"
do
  if [ $i = $SecretName ]; then
    echo "The same secret exists"
    SecretExisted=1
    break
  fi
done

if [ $SecretExisted -eq 0 ]; then
  # creating the secret
  echo "Creating the new secret"
  aws --profile $AwsProfile secretsmanager create-secret --name $SecretName --description "greengrass component webrtc secret" --secret-string file://$SecretFileName > $TempPath/$SecretARN.json
else
  echo "You need to handle the duplicated secret or you already created the secret"
fi

# creating the bucket
BucketExisted=0
aws --profile $AwsProfile s3api list-buckets > $TempPath/$ListBuckets.json
ListedBucketName=($(jq -r '.Buckets[].Name' $TempPath/$ListBuckets.json))

for i in "${ListedBucketName[@]}"
do
  if [ $i = $ComponentArtifactsBucket ]; then
    echo "The same bucket exists"
    BucketExisted=1
    break
  fi
done

if [ $BucketExisted -eq 0 ]; then
  echo "Creating the new bucket"
  aws --profile $AwsProfile s3 mb s3://$ComponentArtifactsBucket --region $AwsRegion > $TempPath/tmps3.json
else
  echo "You need to handle the duplicated bucket or you already created the bucket"
fi

rm -rf $TempPath