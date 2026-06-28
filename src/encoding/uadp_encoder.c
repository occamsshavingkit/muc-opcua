#include "micro_opcua/pubsub.h"
#include "micro_opcua/status.h"
#include <string.h>

/*
 * Encode a minimal UADP NetworkMessage (OPC 10000-14 7.2.2)
 * For this MVP, we only support a hardcoded publisher ID format and dummy payload.
 */
opcua_statuscode_t mu_encode_uadp_network_message(const mu_pubsub_connection_t *conn, 
                                                  const mu_pubsub_writer_group_t *wg,
                                                  opcua_byte_t *buffer, size_t buffer_size, 
                                                  size_t *bytes_written) {
    if (!conn || !wg || !buffer || !bytes_written) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    
    if (buffer_size < 9) {
        return MU_STATUS_BAD_ENCODINGERROR;
    }
    
    // UADPVersion (bits 0-3) = 1
    // PublisherId enabled (bit 4) = 1
    // GroupHeader enabled (bit 5) = 0
    // PayloadHeader enabled (bit 6) = 0
    // ExtendedFlags1 enabled (bit 7) = 1
    // Total Flags1 = 0x51 (0101 0001)
    buffer[0] = 0x51;
    
    // PublisherIdType (bits 0-2) = 2 (UInt32)
    // DataSetClassId enabled (bit 3) = 0
    // Security enabled (bit 4) = 0
    // Timestamp enabled (bit 5) = 0
    // PicoSeconds enabled (bit 6) = 0
    // ExtendedFlags2 enabled (bit 7) = 0
    // Total ExtendedFlags1 = 0x20 (0010 0000)
    buffer[1] = 0x20;
    
    // PublisherId (UInt32) - little endian
    buffer[2] = (opcua_byte_t)(conn->publisher_id & 0xFF);
    buffer[3] = (opcua_byte_t)((conn->publisher_id >> 8) & 0xFF);
    buffer[4] = (opcua_byte_t)((conn->publisher_id >> 16) & 0xFF);
    buffer[5] = (opcua_byte_t)((conn->publisher_id >> 24) & 0xFF);
    
    // Payload: Dummy dataset message for MVP
    buffer[6] = 0x01;
    buffer[7] = 0x02;
    buffer[8] = 0x03;
    
    *bytes_written = 9;
    return MU_STATUS_GOOD;
}
