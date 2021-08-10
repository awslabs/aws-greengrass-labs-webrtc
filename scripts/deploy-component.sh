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
if [ $# -ne 6 ]; then
  echo 1>&2 "Usage: $0 AwsProfile IotThingName ComponentName ComponentVersion ComponentArtifactsBucket SecretName"
  exit 1
fi
AwsProfile=$1
IotThingName=$2
ComponentName=$3
ComponentVersion=$4
ComponentArtifactsBucket=$5
# secret manager related
SecretName=$6

# path to necessary folder
TempPath="temp"
DeploymentName="Deployment"
# aws profile
AwsRegion=$(aws configure get $AwsProfile.region)

# create temp patch
mkdir -p $TempPath

aws --profile $AwsProfile iot describe-thing --thing-name $IotThingName > $TempPath/$IotThingName.json
ThingTargetArn=$(jq -r '.thingArn' $TempPath/$IotThingName.json)
echo "TargetARN: $ThingTargetArn"
aws --profile $AwsProfile secretsmanager describe-secret --secret-id $SecretName > $TempPath/$SecretName.json
SecretTargetArn=$(jq -r '.ARN' $TempPath/$SecretName.json)
echo "SecretARN: $SecretTargetArn"

cat > $TempPath/$DeploymentName.json<<EOF
{
    "targetArn": "$ThingTargetArn",
    "deploymentName": "Deployment for MyGreengrassCore",
    "components": {
        "aws.greengrass.SecretManager":{
            "componentVersion": "2.0.8",
            "configurationUpdate": {
                "reset": [],
                "merge": "{\"cloudSecrets\":[{\"arn\": \"$SecretTargetArn\"}]}"
              }
        },
        "aws.greengrass.TokenExchangeService":{
            "componentVersion": "2.0.3"
        },
        "$ComponentName":{
            "componentVersion": "$ComponentVersion",
            "configurationUpdate": {
                  "reset": [],
                  "merge": "{\"IpcTimeout\" : \"10\",\"RecipesBucketName\" : \"$ComponentArtifactsBucket\",\"SecretArn\": \"$SecretName\",\"accessControl\": {\"aws.greengrass.SecretManager\": {\"$ComponentName:secrets:1\": {\"policyDescription\": \"Allows access to a secret.\",\"operations\": [\"aws.greengrass#GetSecretValue\"],\"resources\": [\"*\"]}}}}"
              }
        }
    },
    "deploymentPolicies": {
        "failureHandlingPolicy": "ROLLBACK",
        "componentUpdatePolicy": {
            "timeoutInSeconds": 60,
            "action": "NOTIFY_COMPONENTS"
        }
    },
    "iotJobConfiguration": {}
}
EOF

aws greengrassv2 create-deployment  --cli-input-json file://$TempPath/$DeploymentName.json
