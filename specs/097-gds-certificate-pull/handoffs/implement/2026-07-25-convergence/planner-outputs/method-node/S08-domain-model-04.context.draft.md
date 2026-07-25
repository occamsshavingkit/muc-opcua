# T045 context digest draft

Modify only `src/address_space/base_nodes.c` and the receipt. Follow the accepted
T043 and T044 method-node, Argument metadata, static-value, reference-array,
compile-gate, and numeric node-order patterns already present in that file.

Implement the project-defined compatibility contract from
`specs/097-gds-certificate-pull/data-model.md`:

- Method NodeId: namespace 0, numeric 12484, BrowseName `FinishRequest`.
- InputArguments property: namespace 0, project-reserved numeric 60005.
- OutputArguments property: namespace 0, project-reserved numeric 60006.
- Input: `requestId` as scalar UInt32.
- Outputs in order: `certificate` as scalar ByteString, `privateKey` as scalar
  ByteString, and `issuerCertificates` as a one-dimensional ByteString array.
- Add forward HasProperty(46) references from the method to both properties.
- Add a forward HasComponent(47) reference from CertificateDirectoryType (15594)
  to the method. Do not attach FinishRequest to CertificateType or its subtypes.

Ground signature semantics in OPC-10000-12 §7.9.9 and encode Argument values
using the repository's existing OPC-10000-6 UA Binary patterns. Treat NodeIds
12484, 60005, and 60006 as explicit project compatibility identifiers, not claims
about current OPC Foundation standard namespace-0 assignments. Preserve method
NodeId 12747 and property NodeId 60007 for T046 GetRejectedList.

Do not add behavior, tests, generated-ID declarations, edit tasks.md, touch other
production files, dispatch workers, or commit.
