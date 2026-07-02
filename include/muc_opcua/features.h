/* include/muc_opcua/features.h
 *
 * Feature 025 (F9): compile-time dependency guards for the MUC_OPCUA_* feature
 * gates. The build system enables a feature by defining its macro to 1 (an
 * unset macro means "off", which both `#ifdef X` and `#if X` already treat
 * correctly), so this header does NOT define any feature defaults — doing so
 * would flip `#ifdef`-style tests. It only rejects *illegal combinations* so a
 * mis-specified `custom` profile (or a direct -D consumer that bypasses CMake)
 * fails loudly at compile time instead of silently building a half-wired feature
 * that references infrastructure which was never compiled.
 *
 * Included first from muc_opcua/config.h, so every translation unit sees it.
 */
#ifndef MUC_OPCUA_FEATURES_H
#define MUC_OPCUA_FEATURES_H

/* The Standard DataChange Subscription facet extends the base subscription
   engine; without it the standard-facet code has no subscription state to use. */
#if defined(MUC_OPCUA_SUBSCRIPTIONS_STANDARD) && MUC_OPCUA_SUBSCRIPTIONS_STANDARD && !MUC_OPCUA_SUBSCRIPTIONS
#error "MUC_OPCUA_SUBSCRIPTIONS_STANDARD requires MUC_OPCUA_SUBSCRIPTIONS"
#endif

/* Event notifications are delivered through the subscription/MonitoredItem
   machinery, so events cannot be built without the subscription engine. */
#if defined(MUC_OPCUA_EVENTS) && MUC_OPCUA_EVENTS && !MUC_OPCUA_SUBSCRIPTIONS
#error "MUC_OPCUA_EVENTS requires MUC_OPCUA_SUBSCRIPTIONS"
#endif

/* The Base Information Type System node set is a subtree of the Base Information
   node set; exposing it without the base nodes leaves its references dangling. */
#if defined(MUC_OPCUA_BASE_TYPE_SYSTEM) && MUC_OPCUA_BASE_TYPE_SYSTEM && !MUC_OPCUA_BASE_NODES
#error "MUC_OPCUA_BASE_TYPE_SYSTEM requires MUC_OPCUA_BASE_NODES"
#endif

#endif /* MUC_OPCUA_FEATURES_H */
