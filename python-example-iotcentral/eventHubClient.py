import asyncio
from azure.eventhub.aio import EventHubConsumerClient


# ----------- USER CONFIGURATION -----------------------------------------------------

EVENTHUB_CONNECTION_STRING = "connectionString"
EVENTHUB_NAME = "eventHubName"
CONSUMER_GROUP = "consumerGroup"


async def on_event(partition_context, event):
    # Print the event data.
    print("Received the event: \"{}\" from the partition with ID: \"{}\"".format(event.body_as_str(encoding='UTF-8'), partition_context.partition_id))

    # Update the checkpoint so that the program doesn't read the events
    # that it has already read when you run it next time.
    await partition_context.update_checkpoint(event)


async def main():
    # Create a consumer client for the event hub.
    client = EventHubConsumerClient.from_connection_string(
        EVENTHUB_CONNECTION_STRING,
        consumer_group=CONSUMER_GROUP,
        eventhub_name=EVENTHUB_NAME
    )
    async with client:
        # Call the receive method. Read from the beginning of the partition (starting_position: "-1")
        await client.receive(on_event=on_event,  starting_position="-1")

if __name__ == '__main__':
    loop = asyncio.get_event_loop()
    # Run the main method.
    loop.run_until_complete(main())  
