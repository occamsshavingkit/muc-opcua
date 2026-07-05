/* include/muc_opcua/transport.h
 *
 * OPC UA TCP transport-layer constants (OPC-10000-6 §7.1.2).
 */
#ifndef MUC_OPCUA_TRANSPORT_H
#define MUC_OPCUA_TRANSPORT_H

/* OPC-10000-6 §7.1.2.2: the OPC UA TCP protocol version is 0. This is the
 * value the Server reports both in the ACKF message (ProtocolVersion) and in
 * the OpenSecureChannelResponse (ServerProtocolVersion, OPC-10000-4 §5.6.2.2
 * Table 11). */
#define MU_OPC_UA_TCP_PROTOCOL_VERSION 0

#endif /* MUC_OPCUA_TRANSPORT_H */
