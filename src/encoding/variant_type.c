/* src/encoding/variant_type.c
 * Variant DataType assignability rules.
 * Grounding: OPC-10000-4 5.11.4.2 Table 53 — "subtypes of the Attribute
 * DataType shall be accepted by the Server." */
#include "muc_opcua/types.h"

/* Returns true if `actual` may be written to an Attribute whose declared
 * DataType is `expected`. Per OPC-10000-4 5.11.4.2, subtypes of the declared
 * DataType shall be accepted. The OPC UA built-in types enumerated in
 * mu_builtin_type_t (Boolean, SByte, Byte, Int16, UInt16, Int32, UInt32,
 * Int64, UInt64, Float, Double, String, DateTime, Guid, ByteString,
 * XmlElement, NodeId, ExpandedNodeId, StatusCode, QualifiedName,
 * LocalizedText, ExtensionObject, DataValue, Variant, DiagnosticInfo) form a
 * flat set with no in-enumeration subtyping, so for any pairing of two
 * built-in types the rule is exact equality.
 *
 * The switch-on-`expected` structure is preserved so that, once a TypeDef
 * table (custom DataTypes declared as subypes of a built-in base via the
 * address space) is available, the per-base-type subtype rules can be added
 * inline without modifying call sites such as the Write value-type check in
 * src/core/dispatch_attribute.c. */
opcua_boolean_t mu_variant_type_is_assignable(mu_builtin_type_t expected, mu_builtin_type_t actual) {
    if (expected == actual) {
        return true;
    }

    switch (expected) {
    /* Built-in base types: no enumerated subtypes. When TypeDef-based custom
     * DataTypes are introduced, each base case can resolve `actual` against
     * its subtype table here. Until then, fall through to the default and
     * reject any non-identical pairing (preserving the historical exact-match
     * Write behaviour). */
    case MU_TYPE_BOOLEAN:
    case MU_TYPE_SBYTE:
    case MU_TYPE_BYTE:
    case MU_TYPE_INT16:
    case MU_TYPE_UINT16:
    case MU_TYPE_INT32:
    case MU_TYPE_UINT32:
    case MU_TYPE_INT64:
    case MU_TYPE_UINT64:
    case MU_TYPE_FLOAT:
    case MU_TYPE_DOUBLE:
    case MU_TYPE_STRING:
    case MU_TYPE_DATETIME:
    case MU_TYPE_GUID:
    case MU_TYPE_BYTESTRING:
    case MU_TYPE_XMLELEMENT:
    case MU_TYPE_NODEID:
    case MU_TYPE_EXPANDEDNODEID:
    case MU_TYPE_STATUSCODE:
    case MU_TYPE_QUALIFIEDNAME:
    case MU_TYPE_LOCALIZEDTEXT:
    case MU_TYPE_EXTENSIONOBJECT:
    case MU_TYPE_DATAVALUE:
    case MU_TYPE_VARIANT:
    case MU_TYPE_DIAGNOSTICINFO:
    case MU_TYPE_NULL:
        return false;

    default:
        return false;
    }
}
