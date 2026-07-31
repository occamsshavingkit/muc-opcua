#include "subscription_publish_min_10.h"
#include "muc_opcua/capacities.h"

_Static_assert(MU_INTERN_MAX_PUBLISH_REQUESTS >= MU_CU_SUBSCRIPTION_PUBLISH_MIN_10_REQUIRED_REQUESTS,
               "Subscription Publish Min 10 requires >= 10 parked Publish requests");

const unsigned char mu_cu_subscription_publish_min_10_enabled = 1u;
