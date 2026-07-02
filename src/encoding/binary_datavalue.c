/* src/encoding/binary_datavalue.c */
#include "muc_opcua/encoding.h"

opcua_statuscode_t mu_binary_read_datavalue(mu_binary_reader_t *reader, mu_datavalue_t *value) {
    opcua_byte_t encoding_mask;
    opcua_statuscode_t status = mu_binary_read_byte(reader, &encoding_mask);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    value->has_value = (encoding_mask & 0x01) != 0;
    value->has_status = (encoding_mask & 0x02) != 0;
    value->has_source_timestamp = (encoding_mask & 0x04) != 0;
    value->has_server_timestamp = (encoding_mask & 0x08) != 0;
    value->has_source_picoseconds = (encoding_mask & 0x10) != 0;
    value->has_server_picoseconds = (encoding_mask & 0x20) != 0;

    if (value->has_value) {
        status = mu_binary_read_variant(reader, &value->value);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    }

    if (value->has_status) {
        status = mu_binary_read_statuscode(reader, &value->status);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    }

    if (value->has_source_timestamp) {
        status = mu_binary_read_int64(reader, &value->source_timestamp);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    }

    if (value->has_source_picoseconds) {
        status = mu_binary_read_uint16(reader, &value->source_picoseconds);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    }

    if (value->has_server_timestamp) {
        status = mu_binary_read_int64(reader, &value->server_timestamp);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    }

    if (value->has_server_picoseconds) {
        status = mu_binary_read_uint16(reader, &value->server_picoseconds);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_write_datavalue(mu_binary_writer_t *writer, const mu_datavalue_t *value) {
    if (!value) {
        return MU_STATUS_BAD_ENCODINGERROR;
    }

    opcua_byte_t encoding_mask = 0;
    if (value->has_value) {
        encoding_mask |= 0x01;
    }
    if (value->has_status) {
        encoding_mask |= 0x02;
    }
    if (value->has_source_timestamp) {
        encoding_mask |= 0x04;
    }
    if (value->has_source_picoseconds) {
        encoding_mask |= 0x10;
    }
    if (value->has_server_timestamp) {
        encoding_mask |= 0x08;
    }
    if (value->has_server_picoseconds) {
        encoding_mask |= 0x20;
    }

    opcua_statuscode_t status = mu_binary_write_byte(writer, encoding_mask);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    if (value->has_value) {
        status = mu_binary_write_variant(writer, &value->value);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    }

    if (value->has_status) {
        status = mu_binary_write_statuscode(writer, value->status);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    }

    if (value->has_source_timestamp) {
        status = mu_binary_write_int64(writer, value->source_timestamp);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    }

    if (value->has_source_picoseconds) {
        status = mu_binary_write_uint16(writer, value->source_picoseconds);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    }

    if (value->has_server_timestamp) {
        status = mu_binary_write_int64(writer, value->server_timestamp);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    }

    if (value->has_server_picoseconds) {
        status = mu_binary_write_uint16(writer, value->server_picoseconds);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    }

    return MU_STATUS_GOOD;
}
