import os
import asyncio
import configparser
import sys

from random import randint
from iotc import IOTCConnectType, IOTCLogLevel, IOTCEvents
from iotc.aio import IoTCClient

config = configparser.ConfigParser()
config.read(os.path.join(os.path.dirname(__file__), 'samples.ini'))

device_id = config['DEVICE_PARAMS']['DeviceId']
scope_id = config['DEVICE_PARAMS']['ScopeId']
key = config['DEVICE_PARAMS']['DeviceKey']

# optional model Id for auto-provisioning
try:
    model_id = config['DEVICE_PARAMS']['ModelId']
except:
    model_id = None


async def on_props(propName, propValue):
    print(propValue)
    return True


async def on_commands(command, ack):
    print(command.name)
    await ack(command.name, 'Command received', command.request_id)


async def on_enqueued_commands(command_name, command_data):
    print(command_name)
    print(command_data)


# change connect type to reflect the used key (device or group)
client = IoTCClient(device_id, scope_id,
                    IOTCConnectType.IOTC_CONNECT_SYMM_KEY, key)
if model_id != None:
    client.set_model_id(model_id)

client.set_log_level(IOTCLogLevel.IOTC_LOGGING_ALL)
client.on(IOTCEvents.IOTC_PROPERTIES, on_props)
client.on(IOTCEvents.IOTC_COMMAND, on_commands)
client.on(IOTCEvents.IOTC_ENQUEUED_COMMAND, on_enqueued_commands)


# iotc.setQosLevel(IOTQosLevel.IOTC_QOS_AT_MOST_ONCE)


async def main():
    await client.connect()
    while client.is_connected():
        await client.send_telemetry({
            'accelerometerX': str(randint(20, 45)),
            'accelerometerY': str(randint(20, 45)),
            "accelerometerZ": str(randint(20, 45))
        })
        await asyncio.sleep(3)


asyncio.run(main())
