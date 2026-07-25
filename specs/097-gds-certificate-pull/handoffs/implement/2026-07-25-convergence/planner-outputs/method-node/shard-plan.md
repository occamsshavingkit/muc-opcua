# Method-node shard plan

## S06-method-node-01 — T043

Add only the project-defined StartSigningRequest address-space declaration to
`src/address_space/base_nodes.c`. The method is namespace-0 numeric NodeId 12482,
with namespace-0 numeric InputArguments and OutputArguments property NodeIds
60001 and 60002. The method identifier is a project compatibility contract: current
OPC Foundation GDS NodeSets model StartSigningRequest on CertificateDirectoryType
with model-local identifiers, so this shard must not claim that 12482, 60001, or
60002 are current standard namespace-0 assignments. The property IDs use the
project-reserved 60000 range because 12483 and 12484 are method IDs required by
StartNewKeyPairRequest and FinishRequest in the same local model.

The method signature follows `data-model.md`: CSR (ByteString) and
certificateGroupId (NodeId) inputs, requestId (UInt32) output. Reuse existing
static UA Binary Argument metadata patterns and add HasProperty(46) references
from the method to both argument properties plus HasComponent(47) references
from each required certificate type subtype to the method. No method behavior,
tests, generated IDs, or task-ledger edits are included.

## S07-domain-model-03 — T044

Add only the project-defined StartNewKeyPairRequest address-space declaration to
`src/address_space/base_nodes.c`. The method is namespace-0 numeric NodeId 12483,
with project-reserved namespace-0 InputArguments and OutputArguments property
NodeIds 60003 and 60004. Preserve method NodeIds 12484 and 12747 for FinishRequest
and GetRejectedList, and reserve property NodeIds 60005-60007 for those later
method-metadata shards.

The method signature follows `data-model.md`: keySpec (ByteString) and
certificateGroupId (NodeId) inputs in that order, with requestId (UInt32) output.
Reuse the T043 static UA Binary Argument metadata patterns. Add HasProperty(46)
references from the method to both argument properties and HasComponent(47)
references from CertificateType and all five modeled subtypes to the method.
Ground declarations in OPC-10000-12 §7.9.7. No method behavior, tests, generated
IDs, other production files, or task-ledger edits are included.

## S08-domain-model-04 — T045

Add only the project-defined FinishRequest address-space declaration to
`src/address_space/base_nodes.c`. The method is namespace-0 numeric NodeId 12484,
with project-reserved namespace-0 InputArguments and OutputArguments property
NodeIds 60005 and 60006. Preserve method NodeId 12747 and property NodeId 60007
for GetRejectedList in T046.

The method signature follows `data-model.md`: requestId (UInt32) input, with
certificate (ByteString), privateKey (ByteString), and issuerCertificates
(ByteString[]) outputs in that order. Reuse the accepted static UA Binary Argument
metadata patterns. Add HasProperty(46) references from the method to both argument
properties and a HasComponent(47) reference from CertificateDirectoryType (15594)
to the method. Ground declarations in OPC-10000-12 §7.9.9. No method behavior,
tests, generated IDs, other production files, or task-ledger edits are included.
