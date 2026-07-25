# T043 context digest draft

Modify only `src/address_space/base_nodes.c` and the receipt. Follow the existing
method-node, Argument metadata, and static-reference patterns in that file.

Implement the project-defined compatibility contract from
`specs/097-gds-certificate-pull/data-model.md`:

- Method NodeId: namespace 0, numeric 12482, BrowseName `StartSigningRequest`.
- InputArguments property: namespace 0, project-reserved numeric 60001.
- OutputArguments property: namespace 0, project-reserved numeric 60002.
- Inputs in order: `CSR` as scalar ByteString and `certificateGroupId` as scalar
  NodeId.
- Output: `requestId` as scalar UInt32.
- Add forward HasProperty(46) references from the method to both properties.
- Add forward HasComponent(47) references from CertificateType (12555),
  RsaMinApplicationCertificateType (12557), RsaSha256ApplicationCertificateType
  (12559), HttpsCertificateType (12558), UserCredentialCertificateType (15421),
  and ApplicationCertificateType (15017) to the method, matching the local
  project model.

Ground the signature semantics in OPC-10000-12 §7.9.6 as named by T043 and encode
Argument values using the repository's existing OPC-10000-6 UA Binary patterns.
Do not represent NodeIds 12482, 60001, or 60002 as current OPC Foundation standard
assignments: repository research found no such mapping in current, 1.04, or 1.03
UA-Nodeset Schema files. They are explicit project compatibility identifiers.
NodeIds 12483 and 12484 are reserved for the StartNewKeyPairRequest and
FinishRequest Methods required by the same local data model and must remain free.

Do not add behavior, tests, generated-ID declarations, edit tasks.md, touch other
production files, dispatch workers, or commit.
