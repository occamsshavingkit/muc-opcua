# T039 context digest draft

`src/core/server/init.c` currently declares `mu_certificate_manager_register`
with an ad-hoc `extern`. T039 restores the internal header promised by checked
task T014. Create `cert_manager.h` with the registration declaration, include it
from `init.c` and `cert_manager.c`, and remove only that ad-hoc declaration.

Push Model registration is handled by later task T057 and is outside T039.
Do not edit the public certificate-manager API, CMake/Kconfig, or tasks.md.
