# ActivateSession Fixtures

This directory holds ActivateSession binary wire fixtures for the US1 decoder
tests.

Each fixture in this directory must cite OPC-10000-4 §5.7.3.2, titled
`Parameters` under §5.7.3 `ActivateSession`. Table 17, `ActivateSession Service
Parameters`, defines the wire fields covered by these fixtures, including
`clientSoftwareCertificates[]`, `localeIds[]`, and `userIdentityToken`.
