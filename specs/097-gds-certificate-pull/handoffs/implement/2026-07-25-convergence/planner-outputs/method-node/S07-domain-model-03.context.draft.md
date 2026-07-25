# T044 context digest draft

Modify only `src/address_space/base_nodes.c` and the receipt. Follow the accepted
T043 method-node, Argument metadata, static-value, reference-array, compile-gate,
and numeric node-order patterns already present in that file.

Implement the project-defined compatibility contract from
`specs/097-gds-certificate-pull/data-model.md`:

- Method NodeId: namespace 0, numeric 12483, BrowseName `StartNewKeyPairRequest`.
- InputArguments property: namespace 0, project-reserved numeric 60003.
- OutputArguments property: namespace 0, project-reserved numeric 60004.
- Inputs in order: `keySpec` as scalar ByteString and `certificateGroupId` as
  scalar NodeId.
- Output: `requestId` as scalar UInt32.
- Add forward HasProperty(46) references from the method to both properties.
- Add forward HasComponent(47) references from CertificateType (12556),
  ApplicationCertificateType (12557), HttpsCertificateType (12558),
  UserCertificateType (15017), RsaSha256ApplicationCertificateType (12559), and
  RsaMinApplicationCertificateType (15421) to the method.

Ground signature semantics in OPC-10000-12 §7.9.7 and encode Argument values
using the repository's existing OPC-10000-6 UA Binary patterns. Treat NodeIds
12483, 60003, and 60004 as explicit project compatibility identifiers, not claims
about current OPC Foundation standard namespace-0 assignments. Preserve method
NodeIds 12484 and 12747 for T045 and T046. Preserve property NodeIds 60005-60007
for their later metadata.

Do not add behavior, tests, generated-ID declarations, edit tasks.md, touch other
production files, dispatch workers, or commit.
