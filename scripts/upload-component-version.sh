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
if [ $# -ne 5 ]; then
  echo 1>&2 "Usage: $0 WorkPath AwsProfile ComponentName ComponentVersion ComponentArtifactsBucket"
  exit 3
fi

WorkPath=$1
AwsProfile=$2
ComponentName=$3
ComponentVersion=$4
ComponentArtifactsBucket=$5
AwsRegion=$(aws configure get $AwsProfile.region)

cd $WorkPath/components/artifacts/$ComponentName/$ComponentVersion

for FILE in *;
do
  aws s3api put-object --profile $AwsProfile --bucket $ComponentArtifactsBucket --key artifacts/$ComponentName/$ComponentVersion/$FILE --body $FILE; 
done

cd ../../..
aws greengrassv2 create-component-version --profile $AwsProfile --inline-recipe fileb://recipes/$ComponentName-$ComponentVersion.json --region $AwsRegion 
cd ..
