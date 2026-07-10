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

/* Data Access Facet requires base nodes for AnalogItem metadata exposure. */
#if defined(MUC_OPCUA_DATA_ACCESS) && MUC_OPCUA_DATA_ACCESS && !MUC_OPCUA_BASE_NODES
#error "MUC_OPCUA_DATA_ACCESS requires MUC_OPCUA_BASE_NODES"
#endif

/* Event filter where-clause requires the event subsystem. */
#if defined(MUC_OPCUA_EVENT_FILTER_WHERE) && MUC_OPCUA_EVENT_FILTER_WHERE && !MUC_OPCUA_EVENTS
#error "MUC_OPCUA_EVENT_FILTER_WHERE requires MUC_OPCUA_EVENTS"
#endif

/* The Standard Event Subscription facet's WhereClause evaluation is created and
   flagged through the standard MonitoredItem filter path (per-item filterResult),
   so it requires the standard subscription facet. */
#if defined(MUC_OPCUA_EVENT_FILTER_WHERE) && MUC_OPCUA_EVENT_FILTER_WHERE && !MUC_OPCUA_SUBSCRIPTIONS_STANDARD
#error "MUC_OPCUA_EVENT_FILTER_WHERE requires MUC_OPCUA_SUBSCRIPTIONS_STANDARD"
#endif

/* Auditing requires events for audit event delivery. */
#if defined(MUC_OPCUA_AUDITING) && MUC_OPCUA_AUDITING && !MUC_OPCUA_EVENTS
#error "MUC_OPCUA_AUDITING requires MUC_OPCUA_EVENTS"
#endif

/* Complex types require base nodes for the type hierarchy. */
#if defined(MUC_OPCUA_COMPLEX_TYPES) && MUC_OPCUA_COMPLEX_TYPES && !MUC_OPCUA_BASE_NODES
#error "MUC_OPCUA_COMPLEX_TYPES requires MUC_OPCUA_BASE_NODES"
#endif

/* Full aggregate set requires the standard subscription facet. */
#if defined(MUC_OPCUA_AGGREGATE_FULL) && MUC_OPCUA_AGGREGATE_FULL && !MUC_OPCUA_SUBSCRIPTIONS_STANDARD
#error "MUC_OPCUA_AGGREGATE_FULL requires MUC_OPCUA_SUBSCRIPTIONS_STANDARD"
#endif

/* Redundancy (TransferSubscriptions) requires subscriptions. */
#if defined(MUC_OPCUA_REDUNDANCY) && MUC_OPCUA_REDUNDANCY && !MUC_OPCUA_SUBSCRIPTIONS
#error "MUC_OPCUA_REDUNDANCY requires MUC_OPCUA_SUBSCRIPTIONS"
#endif

/* Namespaces metadata requires base nodes. */
#if defined(MUC_OPCUA_NAMESPACES) && MUC_OPCUA_NAMESPACES && !MUC_OPCUA_BASE_NODES
#error "MUC_OPCUA_NAMESPACES requires MUC_OPCUA_BASE_NODES"
#endif

/* Standard profile implies all mandatory standard facets. */
#if defined(MUC_OPCUA_STANDARD_PROFILE) && MUC_OPCUA_STANDARD_PROFILE
#if !MUC_OPCUA_DATA_ACCESS
#error "MUC_OPCUA_STANDARD_PROFILE requires MUC_OPCUA_DATA_ACCESS"
#endif
#if !MUC_OPCUA_METHOD_SERVER
#error "MUC_OPCUA_STANDARD_PROFILE requires MUC_OPCUA_METHOD_SERVER"
#endif
#endif

/* The first client tier is intentionally the only compiled client surface today.
   Higher tiers are roadmap entries and must not be enabled until their service
   surfaces exist. */
#if defined(MUC_OPCUA_CLIENT_MICRO) || defined(MUC_OPCUA_CLIENT_EMBEDDED) || defined(MUC_OPCUA_CLIENT_FULL)
#error "Only MUC_OPCUA_CLIENT_NANO is implemented"
#endif

#endif /* MUC_OPCUA_FEATURES_H */
