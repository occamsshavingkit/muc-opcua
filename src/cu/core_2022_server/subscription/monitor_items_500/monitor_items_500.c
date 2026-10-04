#include "monitor_items_500.h"
#include "muc_opcua/capacities.h"

#if MUC_OPCUA_CU_MONITOR_ITEMS_500
_Static_assert(MU_INTERN_MAX_MONITORED_ITEMS >= MU_CU_MONITOR_ITEMS_500_REQUIRED_ITEMS,
               "Monitor Items 500 requires >= 500 MonitoredItems per Subscription");
#endif

const unsigned char mu_cu_monitor_items_500_enabled = 1u;
