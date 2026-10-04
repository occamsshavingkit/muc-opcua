#include "muc_opcua/features.h"

#ifndef EXPECT_ENHANCED_DATACHANGE
#define EXPECT_ENHANCED_DATACHANGE MUC_OPCUA_ENHANCED_DATACHANGE
#endif

_Static_assert(MUC_OPCUA_ENHANCED_DATACHANGE == EXPECT_ENHANCED_DATACHANGE,
               "enhanced DataChange capability must follow its four-CU closure");
