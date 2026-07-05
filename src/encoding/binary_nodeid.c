/* src/encoding/binary_nodeid.c */
#include "muc_opcua/encoding.h"

opcua_statuscode_t mu_binary_read_nodeid(mu_binary_reader_t *reader, mu_nodeid_t *value) {
    opcua_byte_t encoding_mask;
    opcua_statuscode_t status = mu_binary_read_byte(reader, &encoding_mask);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    opcua_byte_t format = encoding_mask & 0x0F;

    switch (format) {
    case 0x00: /* TwoByte */
        value->identifier_type = MU_NODEID_NUMERIC;
        value->namespace_index = 0;
        {
            opcua_byte_t id;
            status = mu_binary_read_byte(reader, &id);
            if (status != MU_STATUS_GOOD) {
                return status;
            }
            value->identifier.numeric = id;
        }
        break;

    case 0x01: /* FourByte */
        value->identifier_type = MU_NODEID_NUMERIC;
        {
            opcua_byte_t ns;
            status = mu_binary_read_byte(reader, &ns);
            if (status != MU_STATUS_GOOD) {
                return status;
            }
            value->namespace_index = ns;

            opcua_uint16_t id;
            status = mu_binary_read_uint16(reader, &id);
            if (status != MU_STATUS_GOOD) {
                return status;
            }
            value->identifier.numeric = id;
        }
        break;

    case 0x02: /* Numeric */
        value->identifier_type = MU_NODEID_NUMERIC;
        status = mu_binary_read_uint16(reader, &value->namespace_index);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        status = mu_binary_read_uint32(reader, &value->identifier.numeric);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        break;

    case 0x03: /* String */
        value->identifier_type = MU_NODEID_STRING;
        status = mu_binary_read_uint16(reader, &value->namespace_index);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        status = mu_binary_read_string(reader, &value->identifier.string);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        break;

#ifdef MUC_OPCUA_EXTENDED_NODEIDS
    case 0x04: /* Guid */
        value->identifier_type = MU_NODEID_GUID;
        status = mu_binary_read_uint16(reader, &value->namespace_index);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        {
            int i;
            for (i = 0; i < 16; i++) {
                status = mu_binary_read_byte(reader, &value->identifier.guid[i]);
                if (status != MU_STATUS_GOOD) {
                    return status;
                }
            }
        }
        break;

    case 0x05: /* Opaque */
        value->identifier_type = MU_NODEID_OPAQUE;
        status = mu_binary_read_uint16(reader, &value->namespace_index);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        {
            opcua_int32_t opaque_len;
            status = mu_binary_read_int32(reader, &opaque_len);
            if (status != MU_STATUS_GOOD) {
                return status;
            }
            if (opaque_len < 0 || opaque_len > 16) {
                return MU_STATUS_BAD_DECODINGERROR;
            }
            value->identifier.opaque.length = (opcua_uint16_t)opaque_len;
            if (opaque_len > 0) {
                int i;
                for (i = 0; i < opaque_len; i++) {
                    status = mu_binary_read_byte(reader, &value->identifier.opaque.data[i]);
                    if (status != MU_STATUS_GOOD) {
                        return status;
                    }
                }
            }
        }
        break;

#endif /* MUC_OPCUA_EXTENDED_NODEIDS */

    default:
        return MU_STATUS_BAD_DECODINGERROR;
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_write_nodeid(mu_binary_writer_t *writer, const mu_nodeid_t *value) {
    if (!value) {
        return MU_STATUS_BAD_ENCODINGERROR;
    }

    opcua_statuscode_t status;

    switch (value->identifier_type) {
    case MU_NODEID_NUMERIC:
        /* Use TwoByte if possible */
        if (value->namespace_index == 0 && value->identifier.numeric <= 255) {
            status = mu_binary_write_byte(writer, 0x00); /* TwoByte mask */
            if (status != MU_STATUS_GOOD) {
                return status;
            }
            return mu_binary_write_byte(writer, (opcua_byte_t)value->identifier.numeric);
        }
        /* Use FourByte if possible */
        if (value->namespace_index <= 255 && value->identifier.numeric <= 65535) {
            status = mu_binary_write_byte(writer, 0x01); /* FourByte mask */
            if (status != MU_STATUS_GOOD) {
                return status;
            }
            status = mu_binary_write_byte(writer, (opcua_byte_t)value->namespace_index);
            if (status != MU_STATUS_GOOD) {
                return status;
            }
            return mu_binary_write_uint16(writer, (opcua_uint16_t)value->identifier.numeric);
        }
        /* Use full Numeric */
        status = mu_binary_write_byte(writer, 0x02); /* Numeric mask */
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        status = mu_binary_write_uint16(writer, value->namespace_index);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        return mu_binary_write_uint32(writer, value->identifier.numeric);

    case MU_NODEID_STRING:
        status = mu_binary_write_byte(writer, 0x03); /* String mask */
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        status = mu_binary_write_uint16(writer, value->namespace_index);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        return mu_binary_write_string(writer, &value->identifier.string);

#ifdef MUC_OPCUA_EXTENDED_NODEIDS
    case MU_NODEID_GUID:
        status = mu_binary_write_byte(writer, 0x04); /* Guid mask */
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        status = mu_binary_write_uint16(writer, value->namespace_index);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        {
            int i;
            for (i = 0; i < 16; i++) {
                status = mu_binary_write_byte(writer, value->identifier.guid[i]);
                if (status != MU_STATUS_GOOD) {
                    return status;
                }
            }
        }
        return MU_STATUS_GOOD;

    case MU_NODEID_OPAQUE:
        status = mu_binary_write_byte(writer, 0x05); /* Opaque mask */
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        status = mu_binary_write_uint16(writer, value->namespace_index);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        status = mu_binary_write_int32(writer, (opcua_int32_t)value->identifier.opaque.length);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        {
            int i;
            for (i = 0; i < value->identifier.opaque.length; i++) {
                status = mu_binary_write_byte(writer, value->identifier.opaque.data[i]);
                if (status != MU_STATUS_GOOD) {
                    return status;
                }
            }
        }
        return MU_STATUS_GOOD;
#endif /* MUC_OPCUA_EXTENDED_NODEIDS */

    default:
        return MU_STATUS_BAD_ENCODINGERROR;
    }
}

opcua_statuscode_t mu_binary_read_expanded_nodeid(mu_binary_reader_t *reader, mu_expanded_nodeid_t *value) {
    if (!reader->buffer || reader->position >= reader->length) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    opcua_byte_t encoding_mask = reader->buffer[reader->position];

    opcua_statuscode_t status = mu_binary_read_nodeid(reader, &value->node_id);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    if (encoding_mask & 0x80) { /* NamespaceUriFlag */
        status = mu_binary_read_string(reader, &value->namespace_uri);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    } else {
        value->namespace_uri.length = -1;
        value->namespace_uri.data = NULL;
    }

    if (encoding_mask & 0x40) { /* ServerIndexFlag */
        status = mu_binary_read_uint32(reader, &value->server_index);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    } else {
        value->server_index = 0;
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_write_expanded_nodeid(mu_binary_writer_t *writer, const mu_expanded_nodeid_t *value) {
    if (!value) {
        return MU_STATUS_BAD_ENCODINGERROR;
    }

    /* For muc-opcua, we don't emit NamespaceUri or ServerIndex. We just encode as NodeId. */
    /* This satisfies basic server requirements where all nodes are local. */
    return mu_binary_write_nodeid(writer, &value->node_id);
}

opcua_statuscode_t mu_binary_read_qualified_name(mu_binary_reader_t *reader, mu_qualified_name_t *value) {
    opcua_statuscode_t status = mu_binary_read_uint16(reader, &value->namespace_index);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    return mu_binary_read_string(reader, &value->name);
}

opcua_statuscode_t mu_binary_write_qualified_name(mu_binary_writer_t *writer, const mu_qualified_name_t *value) {
    if (!value) {
        return MU_STATUS_BAD_ENCODINGERROR;
    }
    opcua_statuscode_t status = mu_binary_write_uint16(writer, value->namespace_index);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    return mu_binary_write_string(writer, &value->name);
}
