# Client API Contract: Nano Client

## Lifecycle Functions

### mu_client_init

```c
mu_status_code_t mu_client_init(
    mu_client_t *client,
    const mu_client_config_t *config,
    mu_client_transport_t *transport
);
```

Initialize a client instance with caller-provided storage.

- `client`: Caller-allocated `mu_client_t` struct
- `config`: Configuration (endpoint, timeout, buffers, security)
- `transport`: Platform TCP adapter
- Returns: `MU_STATUS_GOOD` or `Bad_InvalidArgument`

### mu_client_connect

```c
mu_status_code_t mu_client_connect(mu_client_t *client);
```

Connect to the configured endpoint URL. Resolves hostname, opens TCP
connection, negotiates Hello/Acknowledge, opens SecureChannel.

- Returns: `MU_STATUS_GOOD` or error StatusCode
- State transition: DISCONNECTED → CHANNEL_OPEN

### mu_client_disconnect

```c
mu_status_code_t mu_client_disconnect(mu_client_t *client);
```

Close SecureChannel and TCP connection.

- Returns: `MU_STATUS_GOOD`
- State transition: any connected state → DISCONNECTED

## Session Functions

### mu_client_create_session

```c
mu_status_code_t mu_client_create_session(mu_client_t *client);
```

Create a session with the server.

- Returns: `MU_STATUS_GOOD` or error StatusCode
- State transition: CHANNEL_OPEN → SESSION_CREATED

### mu_client_activate_session

```c
mu_status_code_t mu_client_activate_session(
    mu_client_t *client,
    const mu_identity_token_t *identity
);
```

Activate the session with user identity.

- `identity`: User identity token (anonymous for Nano)
- Returns: `MU_STATUS_GOOD` or error StatusCode
- State transition: SESSION_CREATED → ACTIVE

### mu_client_close_session

```c
mu_status_code_t mu_client_close_session(mu_client_t *client);
```

Close the session. Does not close the SecureChannel.

- Returns: `MU_STATUS_GOOD`
- State transition: ACTIVE → CHANNEL_OPEN

## Discovery Functions

### mu_client_get_endpoints

```c
mu_status_code_t mu_client_get_endpoints(
    mu_client_t *client,
    mu_endpoint_description_t *endpoints,
    size_t *num_endpoints
);
```

Get available server endpoints.

- `endpoints`: Output array of endpoint descriptions
- `num_endpoints`: [in] max endpoints to return, [out] returned count
- Returns: `MU_STATUS_GOOD` or error StatusCode

## Attribute Service Functions

### mu_client_read

```c
mu_status_code_t mu_client_read(
    mu_client_t *client,
    const mu_read_params_t *params,
    mu_read_value_t *results,
    size_t *num_results
);
```

Read one or more node values.

- `params`: Read parameters (timestampsToReturn, maxAge)
- `results`: Output array of read values
- `num_results`: [in] max results, [out] returned count
- Returns: `MU_STATUS_GOOD` or error StatusCode

## View Service Functions

### mu_client_browse

```c
mu_status_code_t mu_client_browse(
    mu_client_t *client,
    const mu_browse_params_t *params,
    mu_browse_result_t *results,
    size_t *num_results
);
```

Browse a node's references.

- `params`: Browse parameters (nodeId, direction, nodeClassMask, resultMask)
- `results`: Output array of browse results
- `num_results`: [in] max results, [out] returned count
- Returns: `MU_STATUS_GOOD`, `Bad_NoContinuePoint`, or error StatusCode

### mu_client_browse_next

```c
mu_status_code_t mu_client_browse_next(
    mu_client_t *client,
    mu_browse_result_t *results,
    size_t *num_results
);
```

Get next batch of browse results (when continuation point exists).

- `results`: Output array of browse results
- `num_results`: [in] max results, [out] returned count
- Returns: `MU_STATUS_GOOD`, `Bad_NoContinuePoint`, or error StatusCode

### mu_client_translate_browse_paths_to_node_ids

```c
mu_status_code_t mu_client_translate_browse_paths_to_node_ids(
    mu_client_t *client,
    const mu_browse_path_t *paths,
    size_t num_paths,
    mu_node_id_t *targets
);
```

Resolve relative browse paths to NodeIds.

- `paths`: Input array of browse path descriptions
- `num_paths`: Number of paths
- `targets`: Output array of resolved NodeIds
- Returns: `MU_STATUS_GOOD` or error StatusCode
