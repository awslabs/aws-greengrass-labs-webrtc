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
  echo 1>&2 "Usage: $0 AwsProfile IamRole ComponentArtifactsBucket SecretName"
  exit 1
fi

AwsProfile=$1
IamRole=$2
ComponentArtifactsBucket=$3
SecretName=$4

# aws profile
AwsRegion=$(aws configure get $AwsProfile.region)

# policy name
ListRoles="ListRoles"
ListPolicies="ListPolicies"
IamPolicyArtifact="GreengrassV2ComponentArtifactPolicy"
IamPolicySecretManager="GreengrassV2ComponentSecretManagerPolicy"
IamPolicyWebRTC="GreengrassV2ComponentWebRTCPolicy"

# create temp patch
TempPath="temp"
mkdir -p $TempPath

aws --profile $AwsProfile iam list-roles > $TempPath/$ListRoles.json
ListedRoleName=($(jq -r '.Roles[].RoleName' $TempPath/$ListRoles.json))
IamRoleExisted=0
for i in "${ListedRoleName[@]}"
do
  if [ $i = $IamRole ]; then
    echo "The role exists"
    IamRoleExisted=1
    break
  fi
done

if [ $IamRoleExisted -eq 0 ]; then
  echo "The role does not exist"
  exit 1
fi

aws --profile $AwsProfile iam list-policies --scope=Local > $TempPath/$ListPolicies.json
ListedPolicyName=($(jq -r '.Policies[].PolicyName' $TempPath/$ListPolicies.json))
ListedPolicyArn=($(jq -r '.Policies[].Arn' $TempPath/$ListPolicies.json))
IamPolicyArtifactExisted=-1
IamPolicySecretManagerExisted=-1
IamPolicyWebRTCExisted=-1
index=0
for i in "${ListedPolicyName[@]}"
do
  if [ $i = $IamPolicyArtifact ]; then
    echo "The policy $IamPolicyArtifact exists"
    IamPolicyArtifactExisted=$index
  elif [ $i = $IamPolicySecretManager ]; then
    echo "The policy $IamPolicySecretManager exists"
    IamPolicySecretManagerExisted=$index
  elif [ $i = $IamPolicyWebRTC ]; then
    echo "The policy $IamPolicyWebRTC exists"
    IamPolicyWebRTCExisted=$index
  fi
  index=$(($index+1))
done

if [ $IamPolicyArtifactExisted -eq -1 ]; then
  echo "Creating $IamPolicyArtifact for the role"
  # https://docs.aws.amazon.com/zh_tw/greengrass/v2/developerguide/device-service-role.html#device-service-role-access-s3-bucket
  cat > $TempPath/IamPermissionS3.json <<EOF
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": [
        "s3:GetObject"
      ],
      "Resource": [
        "arn:aws:s3:::$ComponentArtifactsBucket/*"
      ]
    }
  ]
}
EOF
  aws --profile $AwsProfile iam create-policy --policy-name $IamPolicyArtifact --policy-document file://$TempPath/IamPermissionS3.json > $TempPath/$IamPolicyArtifact.json
  echo "Attaching $IamPolicyArtifact to the role"
  aws --profile $AwsProfile iam attach-role-policy --role-name $IamRole --policy-arn $(jq -r '.Policy.Arn' $TempPath/$IamPolicyArtifact.json)
else
  echo "Attaching $IamPolicyArtifact to the role"
  aws --profile $AwsProfile iam attach-role-policy --role-name $IamRole --policy-arn ${ListedPolicyArn[IamPolicyArtifactExisted]}
fi

if [ $IamPolicyWebRTCExisted -eq -1 ]; then
  echo "Creating $IamPolicyWebRTC for the role"
  echo '{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Action": [
                "kinesisvideo:DescribeSignalingChannel",
                "kinesisvideo:CreateSignalingChannel",
                "kinesisvideo:DeleteSignalingChannel",
                "kinesisvideo:GetSignalingChannelEndpoint",
                "kinesisvideo:GetIceServerConfig",
                "kinesisvideo:ConnectAsMaster",
                "kinesisvideo:ConnectAsViewer"
            ],
            "Resource": "arn:aws:kinesisvideo:*:*:channel/*/*"
        }
    ]
}' > $TempPath/IamPermissionWebrtc.json
  aws --profile $AwsProfile iam create-policy --policy-name $IamPolicyWebRTC --policy-document file://$TempPath/IamPermissionWebrtc.json > $TempPath/$IamPolicyWebRTC.json
  echo "Attaching $IamPolicyWebRTC to the role"
  aws --profile $AwsProfile iam attach-role-policy --role-name $IamRole --policy-arn $(jq -r '.Policy.Arn' $TempPath/$IamPolicyWebRTC.json)
else
  echo "Attaching $IamPolicyWebRTC to the role"
  aws --profile $AwsProfile iam attach-role-policy --role-name $IamRole --policy-arn ${ListedPolicyArn[IamPolicyWebRTCExisted]}
fi

if [ $IamPolicySecretManagerExisted -eq -1 ]; then
  aws --profile $AwsProfile secretsmanager describe-secret --secret-id $SecretName > $TempPath/$SecretName.json
  SecretTargetArn=$(jq -r '.ARN' $TempPath/$SecretName.json)
  echo "SecretARN: $SecretTargetArn"
  echo "Creating $IamPolicySecretManager for the role"
  cat > $TempPath/IamPermissionSecretManager.json<<EOF
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Action": [
                "secretsmanager:GetSecretValue"
            ],
            "Resource": "$SecretTargetArn"
        }
    ]
}
EOF
  aws --profile $AwsProfile iam create-policy --policy-name $IamPolicySecretManager --policy-document file://$TempPath/IamPermissionSecretManager.json > $TempPath/$IamPolicySecretManager.json
  echo "Attaching $IamPolicySecretManager to the role"
  aws --profile $AwsProfile iam attach-role-policy --role-name $IamRole --policy-arn $(jq -r '.Policy.Arn' $TempPath/$IamPolicySecretManager.json)
else
  echo "Attaching $IamPolicySecretManager to the role"
  aws --profile $AwsProfile iam attach-role-policy --role-name $IamRole --policy-arn ${ListedPolicyArn[IamPolicySecretManagerExisted]}
fi

rm -rf $TempPath