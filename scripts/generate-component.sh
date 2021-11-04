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
  echo 1>&2 "Usage: $0 ComponentName ComponentVersion ComponentArtifactsBucket SecretName Os Architecture"
  exit 1
fi
# information of this component
# aws.greengrass.labs.webrtc
ComponentName=$1
# 1.0.0
ComponentVersion=$2
# "aws-greengrass-labs-webrtc-bucket"
ComponentArtifactsBucket=$3
# "aws-greengrass-labs-webrtc-secret"
SecretName=$4
Os=$5
Architecture=$6
# setting.json
# the debug level of webrtc c sdk
AwsKvsLogLevel="4"
# the config.yml for the installation of greengrass core
RootCAName="AmazonRootCA1.pem"

mkdir -p components/artifacts/$ComponentName/$ComponentVersion
# copy the script of component
cp source/run_webrtc.py components/artifacts/$ComponentName/$ComponentVersion
# downloading necessary files for tls connection
echo "Dowloading RootCA"
curl -o components/artifacts/$ComponentName/$ComponentVersion/$RootCAName https://www.amazontrust.com/repository/AmazonRootCA1.pem

echo "Generating related setting"
# generating the setting.json
cat > components/artifacts/$ComponentName/$ComponentVersion/setting.json <<EOF
{
    "AwsKvsLogLevel" : "$AwsKvsLogLevel"
}
EOF
# generating the recipes
cat > components/recipes/$ComponentName-$ComponentVersion.json<<EOF
{
  "RecipeFormatVersion": "2020-01-25",
  "ComponentName": "$ComponentName",
  "ComponentVersion": "$ComponentVersion",
  "ComponentDescription": "The WebRTC GreenGrass Component.",
  "ComponentPublisher": "Amazon",
  "ComponentDependencies": {
    "aws.greengrass.TokenExchangeService": {
      "VersionRequirement": "^2.0.0",
      "DependencyType": "HARD"
    },
    "aws.greengrass.SecretManager": {
      "VersionRequirement": "^2.0.0",
      "DependencyType": "HARD"
    }
  },
  "ComponentConfiguration": {
    "DefaultConfiguration": {
      "IpcTimeout" : "10",
      "RecipesBucketName" : "$ComponentArtifactsBucket",
      "SecretArn": "$SecretName",
      "accessControl": {
        "aws.greengrass.SecretManager": {
          "$ComponentName:secrets:1": {
            "policyDescription": "Allows access to a secret.",
            "operations": [
              "aws.greengrass#GetSecretValue"
            ],
            "resources": [
              "*"
            ]
          }
        }
      }
    }
  },
  "Manifests": [
    {
      "Platform": {
        "os": "linux"
      },
      "Lifecycle": {
        "Install": {
          "Script": "python3 -m pip install awsiotsdk && python3 -m pip install netifaces"
        },
        "Run": {
          "Script": "python3 -u {artifacts:path}/run_webrtc.py '{artifacts:path}' '{configuration:/SecretArn}' '{configuration:/IpcTimeout}'"
        }
      },
      "Artifacts": [
        {
          "URI": "s3://$ComponentArtifactsBucket/artifacts/$ComponentName/$ComponentVersion/run_webrtc.py",
          "Permission": {
            "Read": "OWNER",
            "Execute" : "OWNER"
          }
        },
        {
          "URI": "s3://$ComponentArtifactsBucket/artifacts/$ComponentName/$ComponentVersion/awsGreengrassLabsWebRTC",
          "Permission": {
            "Read": "OWNER",
            "Execute" : "OWNER"
          }
        },
        {
          "URI": "s3://$ComponentArtifactsBucket/artifacts/$ComponentName/$ComponentVersion/setting.json",
          "Permission": {
            "Read": "OWNER",
            "Execute" : "OWNER"
          }
        },
        {
          "URI": "s3://$ComponentArtifactsBucket/artifacts/$ComponentName/$ComponentVersion/$RootCAName",
          "Permission": {
            "Read": "OWNER",
            "Execute" : "OWNER"
          }
        }
      ]
    }
  ]
}
EOF
