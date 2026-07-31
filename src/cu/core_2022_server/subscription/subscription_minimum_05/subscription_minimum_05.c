#include "subscription_minimum_05.h"
#include "muc_opcua/capacities.h"

_Static_assert(MU_INTERN_MAX_SUBSCRIPTIONS >= MU_CU_SUBSCRIPTION_MINIMUM_05_REQUIRED_SUBSCRIPTIONS,
               "Subscription Minimum 05 requires >= 5 Subscriptions per Session");

const unsigned char mu_cu_subscription_minimum_05_enabled = 1u;
