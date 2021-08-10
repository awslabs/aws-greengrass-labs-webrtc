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
if [ $# -ne 3 ]; then
  echo 1>&2 "Usage: $0 AwsProfile ComponentName ComponentVersion"
  exit 3
fi

AwsProfile=$1
ComponentName=$2
ComponentVersion=$3
AwsRegion=$(aws configure get $AwsProfile.region)
Account=$(aws sts get-caller-identity --profile $AwsProfile | jq -r '.Account')

aws greengrassv2 delete-component --profile $AwsProfile --arn arn:aws:greengrass:$AwsRegion:$Account:components:$ComponentName:versions:$ComponentVersion --region $AwsRegion
