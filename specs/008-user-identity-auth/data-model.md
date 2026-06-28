# Data Model: UserName/Password User Identity Token Authentication

## Structures & Types

We define the binary structure representation for `UserNameIdentityToken` in memory without dynamic allocations:

```c
typedef struct {
    mu_string_t policy_id;
    mu_string_t username;
    mu_bytestring_t password;
    mu_string_t encryption_algorithm;
} mu_username_identity_token_t;
```

And `AnonymousIdentityToken`:

```c
typedef struct {
    mu_string_t policy_id;
} mu_anonymous_identity_token_t;
```

## Config Integration

We add the pluggable user authentication handler typedef and fields to the server configuration:

```c
typedef opcua_statuscode_t (*mu_user_auth_handler_t)(void *handle,
                                                      const mu_string_t *username,
                                                      const mu_bytestring_t *password,
                                                      const mu_string_t *policy_id);
```

In `mu_server_config_t`:
```c
    mu_user_auth_handler_t user_auth_handler;
    void *user_auth_handler_handle;
```
