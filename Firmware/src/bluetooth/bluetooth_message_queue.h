#pragma once

//#define DEBUG_MESSAGE_QUEUE

#include "nrf_nvic.h"

#if defined(DEBUG_MESSAGE_QUEUE)
#include "nrf_log.h"
#endif


namespace Bluetooth
{
    class Message;

	/// <summary>
	/// FIFO queue for bluetooth messages
	/// </summary>
	template <int Size>
	class MessageQueue
	{
        struct QueuedMessage {
            int size;
            int stride;
            uint8_t data[1]; // Real size is indicated by size
        };
		
        uint8_t data[Size];
		int count;
		int reader;
		int writer;

	public:
		/// <summary>
		/// Constructor
		/// </summary>
		MessageQueue()
			: count(0)
			, reader(0)
			, writer(0)
		{
		}

        static int computeStride(int msgSize) {
            return (sizeof(int) * 2 + msgSize + 3) & ~3;
        }

		/// <summary>
		/// Add a message to the queue
		/// Returns true if the message could be queued
		/// </summary>
		bool enqueue(const Message* msg, int msgSize)
		{
            uint8_t nested;
            sd_nvic_critical_region_enter(&nested);
            bool ret = false;
            int msgStride = computeStride(msgSize);
            #if defined(DEBUG_MESSAGE_QUEUE)
            NRF_LOG_INFO("Before queueing message type %d of size %d (stride: %d)", msg->type, msgSize, msgStride);
            debugPrint();
            #endif
            if (count == 0 || writer > reader) {
                // Buffer looks like this:
                // [...RMMMMW...]
                // Does it fit at the end?
                if (writer + msgStride <= Size) {

                    // Yes, add the message there
                    QueuedMessage* newMsg = reinterpret_cast<QueuedMessage*>(&data[writer]);
                    newMsg->size = msgSize;
                    newMsg->stride = msgStride;
                    memcpy(newMsg->data, msg, msgSize);

                    // Update the count and writer index
                    writer += msgStride;
                    if (writer == Size)
                        writer = 0;
                    count += 1;
                    ret = true;
                }
                // Nope, does it fit at the begining?
                else if (reader > msgSize) {
                    // Yes, add a dummy message
                    QueuedMessage* dummyMsg = reinterpret_cast<QueuedMessage*>(&data[writer]);
                    dummyMsg->size = 0;
                    dummyMsg->stride = Size - writer;

                    // And then add the new message at the begining
                    QueuedMessage* newMsg = reinterpret_cast<QueuedMessage*>(&data[0]);
                    newMsg->size = msgSize;
                    newMsg->stride = msgStride;
                    memcpy(newMsg->data, msg, msgSize);

                    // Update the count and writer index
                    writer = msgStride;
                    count += 1;
                    ret = true;
                }
                // Else nope, no room
            } else {
                if (reader != writer) {
                    // Buffer looks like this
                    // [MMMW...RMMM]
                    if (writer + msgStride <= reader) {

                        // There is room, add a message there!
                        QueuedMessage* newMsg = reinterpret_cast<QueuedMessage*>(&data[writer]);
                        newMsg->size = msgSize;
                        newMsg->stride = msgStride;
                        memcpy(newMsg->data, msg, msgSize);

                        // Update the count and writer index
                        writer += msgStride;
                        count += 1;
                        ret = true;
                    }
                    // else no room!
                }
                // Else buffer looks like this
                // [MMMRMMM]
                //     W
                // It is full!
            }

            #if defined(DEBUG_MESSAGE_QUEUE)
            NRF_LOG_INFO("After queueing");
            debugPrint();
            #endif
            sd_nvic_critical_region_exit(nested);
            return ret;
		}

		/// <summary>
		/// returns a pointer to the next message
		/// </summary>
		const Message* peekNext(int& outMsgSize) {

            // Default return value
            outMsgSize = 0;
            const Message* ret = nullptr;

            uint8_t nested;
            sd_nvic_critical_region_enter(&nested);
			if (count > 0) {
                const QueuedMessage* msg = reinterpret_cast<const QueuedMessage*>(&data[reader]);
                outMsgSize = msg->size;
                if (outMsgSize == 0) {
                    msg = reinterpret_cast<const QueuedMessage*>(&data[0]);
                    outMsgSize = msg->size;
                }
                ret = reinterpret_cast<const Message*>(msg->data);

                #if defined(DEBUG_MESSAGE_QUEUE)
                NRF_LOG_INFO("Peeked first message of %d messages", count);
                NRF_LOG_INFO("Reader: %d, Writer: %d", reader, writer);
                NRF_LOG_INFO("Message type %d of size %d (stride: %d)", ret->type, msg->size, msg->stride);
                #endif
            }
            sd_nvic_critical_region_exit(nested);
            return ret;
		}

        bool dequeue() {
            uint8_t nested;
            sd_nvic_critical_region_enter(&nested);
            #if defined(DEBUG_MESSAGE_QUEUE)
            NRF_LOG_INFO("Before dequeueing");
            debugPrint();
            #endif

            bool ret = false;
            if (count > 0) {
                QueuedMessage* msg = reinterpret_cast<QueuedMessage*>(&data[reader]);
                if (msg->size == 0) {
                    reader = 0;
                    msg = reinterpret_cast<QueuedMessage*>(&data[0]);
                }
                reader += msg->stride;
                if (reader == Size) {
                    reader = 0;
                }
                count -= 1;
                ret = true;

                #if defined(DEBUG_MESSAGE_QUEUE)
                auto m = reinterpret_cast<const Message*>(msg->data);
                NRF_LOG_INFO("After dequeueing message type %d of size %d (stride: %d)", m->type, msg->size, msg->stride);
                debugPrint();
                #endif
            } else {
                #if defined(DEBUG_MESSAGE_QUEUE)
                NRF_LOG_INFO("After failing to dequeue message (no message in queue)");
                debugPrint();
                #endif
            }

            sd_nvic_critical_region_exit(nested);
            return ret;
        }

        int getCount() {
            return count;
        }

		/// <summary>
		/// Clear the event queue
		/// </summary>
		void clear()
		{
			count = 0;
			writer = 0;
			reader = 0;
		}
        
        #if defined(DEBUG_MESSAGE_QUEUE)
        void debugPrint() {
            NRF_LOG_INFO("Message Queue has %d messages", count);
            NRF_LOG_INFO("Reader: %d, Writer: %d", reader, writer);
            if (count > 0) {
                int i = 0;
                int current = reader;
                do {
                    const QueuedMessage* msg = reinterpret_cast<const QueuedMessage*>(&data[current]);
                    NRF_LOG_INFO(" [%d] Offset %d, size %d, stride %d", i, current, msg->size, msg->stride);
                    ++i;
                    current += msg->stride;
                    if (current == Size) {
                        current = 0;
                    }
                }
                while (current != writer);
            }
        }
        #endif
	};

}
