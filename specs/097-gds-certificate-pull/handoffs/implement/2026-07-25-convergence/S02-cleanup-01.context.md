# T039 context digest

Implement only T039. Create
`src/cu/core_2022_server/certificate_manager/cert_manager.h` with the internal
declaration for `mu_certificate_manager_register`. Include it from
`src/core/server/init.c` and `cert_manager.c`, then remove the corresponding
ad-hoc `extern` declaration from `init.c`.

Do not include or change Push Model declarations: T057 removes Push registration
from the Pull CU. Do not change the public API, build gates, or tasks.md. Do not
commit.
