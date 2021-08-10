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
#!/usr/bin/python3
import concurrent.futures
import json
import sys
import traceback
import os
import datetime
import subprocess
import netifaces
import signal

import awsiot.greengrasscoreipc
from awsiot.greengrasscoreipc.model import (
    GetSecretValueRequest,
    GetSecretValueResponse,
    UnauthorizedError
)

if len(sys.argv) != 4:
    print('Provide necessary parameters in the component configuration.', file=sys.stdout)
    exit(1)

exec_path = sys.argv[1]
secret_id = sys.argv[2]
ipc_timeout = int(sys.argv[3])

print('Current time: {'+str(datetime.datetime.now()) + '}.', file=sys.stdout)

try:
    with open(exec_path+'/setting.json') as json_file:
        setting_json = json.load(json_file)
    print('Got the setting.', file=sys.stdout)
except Exception:
    print('Exception occurred when getting setting.', file=sys.stderr)
    traceback.print_exc()
    exit(1)

try:
    ipc_client = awsiot.greengrasscoreipc.connect()
    request = GetSecretValueRequest()
    request.secret_id = secret_id
    operation = ipc_client.new_get_secret_value()
    operation.activate(request)
    future_response = operation.get_response()
    print('Sending the IPC request.', file=sys.stdout)

    try:
        response = future_response.result(ipc_timeout)
        secret_json = json.loads(response.secret_value.secret_string)
        print('Got IPC response successfully.', file=sys.stdout)
    except concurrent.futures.TimeoutError:
        print('Timeout occurred while getting secret: ' + secret_id, file=sys.stderr)
    except UnauthorizedError as e:
        print('Unauthorized error while getting secret: ' + secret_id, file=sys.stderr)
        raise e
    except Exception as e:
        print('Exception while getting secret: ' + secret_id, file=sys.stderr)
        raise e
except Exception:
    print('Exception occurred when using IPC.', file=sys.stderr)
    traceback.print_exc()
    exit(1)


def is_interface_up(interface):
    addr = netifaces.ifaddresses(interface)
    return netifaces.AF_INET in addr

net_ifs = netifaces.interfaces()

for net_if in net_ifs:
    print(net_if + ':' + str(is_interface_up(net_if)), file=sys.stdout)

thread_list = []
shell_env = os.environ.copy()

def sigterm_handler(signum, frame):
    print("receive sigterm", file=sys.stdout)
    exit(0)

signal.signal(signal.SIGTERM, sigterm_handler)

print("starting the processes", file=sys.stdout)
for rtsp_cam_config in secret_json['RtspConfig']:
    print("RtspUrl:" + rtsp_cam_config['RtspUrl'] + " with " + rtsp_cam_config['ChannelName'], file=sys.stdout)
    shell_env["AWS_KVS_CACERT_PATH"] = exec_path+"/AmazonRootCA1.pem"
    shell_env["AWS_KVS_LOG_LEVEL"] = setting_json['AwsKvsLogLevel']
    shell_env["AWS_RTSP_CHANNEL"] = rtsp_cam_config['ChannelName']
    shell_env["AWS_RTSP_URI"] = rtsp_cam_config['RtspUrl']
    if 'username' in rtsp_cam_config:
        shell_env["AWS_RTSP_USERNAME"] = rtsp_cam_config['username']
    if 'password' in rtsp_cam_config:
        shell_env["AWS_RTSP_PASSWORD"] = rtsp_cam_config['password']
    thread = subprocess.Popen(exec_path+"/kvsWebrtcClientMasterGstRtspSample", env=shell_env, encoding="utf-8")
    thread_list.append(thread)

for thread in thread_list:
    print("waiting the processes", file=sys.stdout)
    thread.wait()
