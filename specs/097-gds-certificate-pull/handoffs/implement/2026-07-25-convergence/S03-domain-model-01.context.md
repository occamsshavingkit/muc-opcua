# T040 context digest

Add only these six forward HasSubtype(45) references in
`src/address_space/base_nodes.c`:

- BaseObjectType(58) → CertificateDirectoryType(15594), OPC-10000-12 §7.9.2.
- CertificateType(12556) → ApplicationCertificateType(12557), §7.8.4.2.
- CertificateType(12556) → HttpsCertificateType(12558), §7.8.4.3.
- CertificateType(12556) → UserCertificateType(15017), §7.8.4.4.
- ApplicationCertificateType(12557) → RsaSha256ApplicationCertificateType(12559), §7.8.4.9.
- ApplicationCertificateType(12557) → RsaMinApplicationCertificateType(15421), §7.8.4.8.

Keep existing BaseObjectType→CertificateGroupType(12555) (§7.8.3.1) and
BaseObjectType→CertificateType(12556) (§7.8.4.1) edges. Preserve all inverse
edges. Use the Pull-CU gate for edges whose target nodes are Pull-only. Do not
change method nodes, instance/component edges, tests, or tasks.md. Do not commit.
