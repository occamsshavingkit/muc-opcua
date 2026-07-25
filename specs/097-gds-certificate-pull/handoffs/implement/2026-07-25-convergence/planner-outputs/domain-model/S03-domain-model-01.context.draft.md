# T040 context digest draft

Existing forward edges from BaseObjectType(58) to CertificateGroupType(12555)
and CertificateType(12556) are correct. Add these missing forward HasSubtype(45)
edges: 58→15594, 12556→12557, 12556→12558, 12556→15017,
12557→12559, and 12557→15421. Existing inverse child→parent edges are
already correct and must remain unchanged.
