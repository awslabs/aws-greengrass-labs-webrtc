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
# https://docs.aws.amazon.com/greengrass/v2/developerguide/manual-installation.html
if [ $# -ne 9 ]; then
  echo 1>&2 "Usage: $0 AwsProfile IotThingName ComponentName ComponentVersion ComponentArtifactsBucket SecretName SecretFileName Os Architecture"
  exit 1
fi
# e.g. default
AwsProfile=$1
# e.g. MyGreengrassCore
IotThingName=$2
# information of this component
# e.g. com.aws-sample.webrtc
ComponentName=$3
# e.g. 1.0.0
ComponentVersion=$4
# e.g. greengrass-webrtc-bucket
ComponentArtifactsBucket=$5
# secret manager related
# e.g. GreengrassV2ComponentWebRTCSecret
SecretName=$6
# e.g. rtsp-camera-configuration.json
SecretFileName=$7
# e.g. linux
Os=$8
# e.g. armv7l or x64
Architecture=$9

# path to necessary folder
CertPath="certs"
TempPath="temp"

# aws profile
AwsRegion=$(aws configure get $AwsProfile.region)
ListIotThingGroup="ListIotThingGroup"
IotThingGroupName="MyGreengrassCoreGroup"
ListRoles="ListRoles"
IamRole="GreengrassV2TokenExchangeRole"
# iam policy name
ListPolicies="ListPolicies"
IamPolicyBasic="GreengrassV2TokenExchangeRoleAccess"
# iot policy name
ListIotPolicies="ListIotPolicies"
IotPolicyCert="GreengrassV2IoTThingPolicy"
IotRoleAliasPolicy="GreengrassTESCertificatePolicyGreengrassCoreTokenExchangeRoleAlias"

# role alias
ListIotRoleAliases="ListIotRoleAliases"
IotRoleAlias="GreengrassCoreTokenExchangeRoleAlias"

# the config.yml for the installation of greengrass core
ListThingPrincipals="ListThingPrincipals"
GreengrassRootPath="/greengrass/v2"
RootCAName="AmazonRootCA1.pem"
IotCertName="device.pem.crt"
privateKeyName="private.pem.key"
publicKeyName="public.pem.key"

# create temp patch
mkdir -p $CertPath
mkdir -p $TempPath

echo "Dowloading RootCA"
curl -o $CertPath/$RootCAName https://www.amazontrust.com/repository/AmazonRootCA1.pem

echo "Setting IoT"

aws --profile $AwsProfile iot list-things > $TempPath/$IotThingName.json
ListedThingName=($(jq -r '.things[].thingName' $TempPath/$IotThingName.json))

IotThingExisted=0
for i in "${ListedThingName[@]}"
do
  if [ $i = $IotThingName ]; then
    echo "The iot thing exists"
    let IotThingExisted=1
    break
  fi
done

if [ $IotThingExisted -eq 0 ]; then
  echo "Creating iot thing"
  aws --profile $AwsProfile iot create-thing --thing-name $IotThingName > $TempPath/$IotThingName.json
fi

# creating the certificate
aws --profile $AwsProfile iot list-thing-principals --thing-name $IotThingName > $TempPath/$ListThingPrincipals.json
ListThingPrincipalsName=($(jq -r '.principals[]' $TempPath/$ListThingPrincipals.json))
echo "There is/are ${#ListThingPrincipalsName[@]} certificates attaching to this iot thing"

if [ ${#ListThingPrincipalsName[@]} -eq 1 ]; then
  CertificateArn=${ListThingPrincipalsName[0]}
elif [ ${#ListThingPrincipalsName[@]} -gt 1 ]; then
  index=0
  for i in "${ListThingPrincipalsName[@]}"
  do
    echo "[$index]: $i"  
    index=$(($index+1))
  done
  read -p "Please select the certifacte you want to setup" IndexOfCert
  CertificateArn=${ListThingPrincipalsName[$IndexOfCert]}
else
  echo "Creating the new certificate"
  aws --profile $AwsProfile iot create-keys-and-certificate --set-as-active --certificate-pem-outfile $CertPath/$IotCertName --public-key-outfile $CertPath/$publicKeyName --private-key-outfile $CertPath/$privateKeyName > $TempPath/certificate
  CertificateArn=$(jq -r '.certificateArn' $TempPath/certificate)
  aws --profile $AwsProfile iot attach-thing-principal --thing-name $IotThingName --principal $CertificateArn
fi

echo "CertificateArn: $CertificateArn"

# creating the iot policies
aws --profile $AwsProfile iot list-policies > $TempPath/$ListIotPolicies.json
ListedIotPolicyName=($(jq -r '.policies[].policyName' $TempPath/$ListIotPolicies.json))
IotPolicyCertExisted=-1
IotRoleAliasPolicyExisted=-1
index=0
for i in "${ListedIotPolicyName[@]}"
do
  if [ $i = $IotPolicyCert ]; then
    echo "The policy $IotPolicyCert exists"
    IotPolicyCertExisted=$index
  elif [ $i = $IotRoleAliasPolicy ]; then
    echo "The policy $IotRoleAliasPolicy exists"
    IotRoleAliasPolicyExisted=$index
  fi
  index=$(($index+1))
done

if [ $IotPolicyCertExisted -eq -1 ]; then
  echo '{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": [
        "iot:Publish",
        "iot:Subscribe",
        "iot:Receive",
        "iot:Connect",
        "greengrass:*"
      ],
      "Resource": [
        "*"
      ]
    }
  ]
}' > $TempPath/GreengrassV2ComponentIoTPolicy.json
  aws --profile $AwsProfile iot create-policy --policy-name $IotPolicyCert --policy-document file://$TempPath/GreengrassV2ComponentIoTPolicy.json  > $TempPath/$IotPolicyCert.json
fi
echo "Attaching $IotPolicyCert to the certificate"
aws --profile $AwsProfile iot attach-policy --policy-name $IotPolicyCert --target $CertificateArn

aws --profile $AwsProfile iot list-thing-groups > $TempPath/$ListIotThingGroup.json
ListIotThingGroupName=($(jq -r '.thingGroups[].groupName' $TempPath/$ListIotThingGroup.json))
IotThingGroupExisted=-1
index=0
for i in "${ListIotThingGroupName[@]}"
do
  if [ $i = $IotThingGroupName ]; then
    echo "The IotThingGroup $IotThingGroupName exists"
    IotThingGroupExisted=$index
    break;
  fi
  index=$(($index+1))
done

if [ $IotThingGroupExisted -eq -1 ]; then
  aws --profile $AwsProfile iot create-thing-group --thing-group-name $IotThingGroupName > $TempPath/$IotThingGroupName.json
fi
echo "Attaching $IotThingName to $IotThingGroupName"
aws --profile $AwsProfile iot add-thing-to-thing-group --thing-name $IotThingName --thing-group-name $IotThingGroupName

aws --profile $AwsProfile iot describe-endpoint --endpoint-type iot:CredentialProvider --output text > $TempPath/IotCredEndpoint.txt
aws --profile $AwsProfile iot describe-endpoint --endpoint-type iot:Data-ATS --output text > $TempPath/IotDataEndpoint.txt

aws --profile $AwsProfile iam list-roles > $TempPath/$ListRoles.json
ListedRoleName=($(jq -r '.Roles[].RoleName' $TempPath/$ListRoles.json))
IamRoleExisted=0
for i in "${ListedRoleName[@]}"
do
  if [ $i = $IamRole ]; then
    echo "The role $IamRole exists"
    IamRoleExisted=1
    break
  fi
done

if [ $IamRoleExisted -eq 0 ]; then
  echo "Creating the role, $IamRole"
  echo '{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Principal": {
        "Service": "credentials.iot.amazonaws.com"
      },
      "Action": "sts:AssumeRole"
    }
  ]
}' > $TempPath/IamTrustPolicy.json
  aws --profile $AwsProfile iam create-role --role-name $IamRole --assume-role-policy-document file://$TempPath/IamTrustPolicy.json > $TempPath/IotRole.json
fi

aws --profile $AwsProfile iam list-policies --scope=Local > $TempPath/$ListPolicies.json
ListedPolicyName=($(jq -r '.Policies[].PolicyName' $TempPath/$ListPolicies.json))
ListedPolicyArn=($(jq -r '.Policies[].Arn' $TempPath/$ListPolicies.json))
IamPolicyBasicExisted=-1
index=0
for i in "${ListedPolicyName[@]}"
do
  if [ $i = $IamPolicyBasic ]; then
    echo "The policy $IamPolicyBasic exists"
    IamPolicyBasicExisted=$index
    break
  fi
  index=$(($index+1))
done

if [ $IamPolicyBasicExisted -eq -1 ]; then
  echo "Creating $IamPolicyBasic"
  echo '{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": [
        "iot:DescribeCertificate",
        "iot:Connect",
        "iot:Publish",
        "iot:Subscribe",
        "iot:Receive",
        "logs:CreateLogGroup",
        "logs:CreateLogStream",
        "logs:PutLogEvents",
        "logs:DescribeLogStreams",
        "s3:GetBucketLocation"
      ],
      "Resource": "*"
    }
  ]
}' >  $TempPath/IamPermissionIoT.json
  aws --profile $AwsProfile iam create-policy --policy-name $IamPolicyBasic --policy-document file://$TempPath/IamPermissionIoT.json > $TempPath/IamRolePolicy.json
  echo "Attaching $IamPolicyBasic to $IamRole"
  aws --profile $AwsProfile iam attach-role-policy --role-name $IamRole --policy-arn $(jq -r '.Policy.Arn' $TempPath/IamRolePolicy.json)
else
  echo "Attaching $IamPolicyBasic to $IamRole"
  aws --profile $AwsProfile iam attach-role-policy --role-name $IamRole --policy-arn ${ListedPolicyArn[IamPolicyBasicExisted]}
fi

aws iot describe-role-alias --role-alias $IotRoleAlias > $TempPath/$ListIotRoleAliases.json

if [ $? -eq 0 ]; then
  echo "The role alias $IotRoleAlias exists"
  IotRoleAliasArn=$(jq -r '.roleAliasDescription.roleAliasArn' $TempPath/$ListIotRoleAliases.json)
else
  echo "Creating $IotRoleAlias"
  aws --profile $AwsProfile iot create-role-alias --role-alias $IotRoleAlias --role-arn $(jq -r '.Role.Arn' $TempPath/IotRole.json) > $TempPath/IotRoleAlias.json
  IotRoleAliasArn=$(jq -r '.roleAliasArn' $TempPath/IotRoleAlias.json)
fi

if [ $IotRoleAliasPolicyExisted -eq -1 ]; then
  echo "Creating $IotRoleAliasPolicy"
  cat > $TempPath/GreengrassV2ComponentIoTRoleAliasPolicy.json <<EOF
{
  "Version":"2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": "iot:AssumeRoleWithCertificate",
      "Resource":"$IotRoleAliasArn"
    }
  ]
}
EOF
  aws --profile $AwsProfile iot create-policy --policy-name $IotRoleAliasPolicy --policy-document file://$TempPath/GreengrassV2ComponentIoTRoleAliasPolicy.json > $TempPath/$IotRoleAliasPolicy.json
fi
echo "Attaching $IotRoleAliasPolicy to the certificate"
aws --profile $AwsProfile iot attach-policy --policy-name $IotRoleAliasPolicy --target $CertificateArn

if [ ${#ListThingPrincipalsName[@]} -lt 1 ]; then

  echo "Generating config.yaml."
  cat > config.yaml <<EOF
system:
  certificateFilePath: "$GreengrassRootPath/$IotCertName"
  privateKeyPath: "$GreengrassRootPath/$privateKeyName"
  rootCaPath: "$GreengrassRootPath/$RootCAName"
  rootpath: "$GreengrassRootPath"
  thingName: "$IotThingName"
services:
  aws.greengrass.Nucleus:
    componentType: "NUCLEUS"
    version: "2.3.0"
    configuration:
      awsRegion: "$AwsRegion"
      iotCredEndpoint: "$(cat $TempPath/IotCredEndpoint.txt)"
      iotDataEndpoint: "$(cat $TempPath/IotDataEndpoint.txt)"
      iotRoleAlias: "$IotRoleAlias"
      mqtt:
        port: 443
      greengrassDataPlanePort: 443
EOF
else
  echo "Does not generate the config.yaml since the setting is tied to the original certificate"
fi
dirPath=$(dirname $BASH_SOURCE)
source ./$dirPath/create-aws-resource.sh $AwsProfile $SecretFileName $SecretName $ComponentArtifactsBucket
source ./$dirPath/attach-necessary-policies.sh $AwsProfile $IamRole $ComponentArtifactsBucket $SecretName
source ./$dirPath/generate-component.sh $ComponentName $ComponentVersion $ComponentArtifactsBucket $SecretName $Os $Architecture

rm -rf $TempPath