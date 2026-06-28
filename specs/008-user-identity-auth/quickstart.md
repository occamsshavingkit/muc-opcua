# Integration Guide: UserName/Password User Identity Token Authentication

## Integration Example

To register a user authentication handler on the server config:

```c
#include "micro_opcua/micro_opcua.h"
#include <string.h>

static opcua_statuscode_t my_auth_handler(void *handle,
                                          const mu_string_t *username,
                                          const mu_bytestring_t *password,
                                          const mu_string_t *policy_id)
{
    (void)handle;
    (void)policy_id;

    /* Simple hardcoded credential check for demonstration */
    if (username->length == 5 && memcmp(username->data, "admin", 5) == 0 &&
        password->length == 5 && memcmp(password->data, "admin", 5) == 0) {
        return MU_STATUS_GOOD;
    }

    return MU_STATUS_BAD_IDENTITYTOKENREJECTED;
}

int main(void)
{
    mu_server_config_t config;
    memset(&config, 0, sizeof(config));
    
    /* ... other config settings ... */
    
    config.user_auth_handler = my_auth_handler;
    config.user_auth_handler_handle = NULL;
    
    /* Initialize and poll server */
    return 0;
}
```
