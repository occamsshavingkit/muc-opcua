/* include/muc_opcua/opcua_ids.h */
#ifndef MUC_OPCUA_OPCUA_IDS_H
#define MUC_OPCUA_OPCUA_IDS_H

#include "muc_opcua/opcua_types.h"

#define MU_ID_FINDSERVERSREQUEST 422
#define MU_ID_FINDSERVERSRESPONSE 425
#define MU_ID_REGISTERSERVERREQUEST 437
#define MU_ID_REGISTERSERVERRESPONSE 440
#define MU_ID_REGISTERSERVER2REQUEST 12193
#define MU_ID_REGISTERSERVER2RESPONSE 12194
#define MU_ID_GETENDPOINTSREQUEST 428
#define MU_ID_GETENDPOINTSRESPONSE 431

#define MU_ID_OPENSECURECHANNELREQUEST 446
#define MU_ID_OPENSECURECHANNELRESPONSE 449
#define MU_ID_CLOSESECURECHANNELREQUEST 452
#define MU_ID_CLOSESECURECHANNELRESPONSE 455

#define MU_ID_CREATESESSIONREQUEST 461
#define MU_ID_CREATESESSIONRESPONSE 464
#define MU_ID_ACTIVATESESSIONREQUEST 467
#define MU_ID_ACTIVATESESSIONRESPONSE 470
#define MU_ID_CLOSESESSIONREQUEST 473
#define MU_ID_CLOSESESSIONRESPONSE 476
#define MU_ID_CANCELREQUEST 478
#define MU_ID_CANCELRESPONSE 481

#define MU_ID_ADDNODESREQUEST 488
#define MU_ID_ADDNODESRESPONSE 491
#define MU_ID_ADDREFERENCESREQUEST 494
#define MU_ID_ADDREFERENCESRESPONSE 497
#define MU_ID_DELETENODESREQUEST 500
#define MU_ID_DELETENODESRESPONSE 503
#define MU_ID_DELETEREFERENCESREQUEST 506
#define MU_ID_DELETEREFERENCESRESPONSE 509

#define MU_ID_ANONYMOUSIDENTITYTOKEN_ENCODING_DEFAULTBINARY 321

#define MU_ID_SERVICEFAULT 397 /* ServiceFault_Encoding_DefaultBinary */

#define MU_ID_BROWSEREQUEST 527
#define MU_ID_BROWSERESPONSE 530
#define MU_ID_BROWSENEXTREQUEST 533
#define MU_ID_BROWSENEXTRESPONSE 536
#define MU_ID_TRANSLATEBROWSEPATHSTONODEIDSREQUEST 554
#define MU_ID_TRANSLATEBROWSEPATHSTONODEIDSRESPONSE 557
#define MU_ID_REGISTERNODESREQUEST 560
#define MU_ID_REGISTERNODESRESPONSE 563
#define MU_ID_UNREGISTERNODESREQUEST 566
#define MU_ID_UNREGISTERNODESRESPONSE 569

#define MU_ID_QUERYFIRSTREQUEST 615
#define MU_ID_QUERYFIRSTRESPONSE 618
#define MU_ID_QUERYNEXTREQUEST 621
#define MU_ID_QUERYNEXTRESPONSE 624

#define MU_ID_READREQUEST 631
#define MU_ID_READRESPONSE 634
#define MU_ID_HISTORYDATA_ENCODING_DEFAULTBINARY 658
#define MU_ID_READRAWMODIFIEDDETAILS_ENCODING_DEFAULTBINARY 649
#define MU_ID_UPDATEDATADETAILS_ENCODING_DEFAULTBINARY 682
#define MU_ID_DELETERAWMODIFIEDDETAILS_ENCODING_DEFAULTBINARY 688
#define MU_ID_HISTORYREADREQUEST 664
#define MU_ID_HISTORYREADRESPONSE 667
#define MU_ID_HISTORYUPDATEREQUEST 700
#define MU_ID_HISTORYUPDATERESPONSE 703
#define MU_ID_WRITEREQUEST 673
#define MU_ID_WRITERESPONSE 676
#define MU_ID_CALLREQUEST 712
#define MU_ID_CALLRESPONSE 715

#define MU_ID_CREATEMONITOREDITEMSREQUEST 751
#define MU_ID_CREATEMONITOREDITEMSRESPONSE 754
#define MU_ID_MODIFYMONITOREDITEMSREQUEST 763
#define MU_ID_MODIFYMONITOREDITEMSRESPONSE 766
#define MU_ID_SETMONITORINGMODEREQUEST 767
#define MU_ID_SETMONITORINGMODERESPONSE 770
#define MU_ID_SETTRIGGERINGREQUEST 773
#define MU_ID_SETTRIGGERINGRESPONSE 776
#define MU_ID_DELETEMONITOREDITEMSREQUEST 781
#define MU_ID_DELETEMONITOREDITEMSRESPONSE 784
#define MU_ID_CREATESUBSCRIPTIONREQUEST 787
#define MU_ID_CREATESUBSCRIPTIONRESPONSE 790
#define MU_ID_MODIFYSUBSCRIPTIONREQUEST 793
#define MU_ID_MODIFYSUBSCRIPTIONRESPONSE 796
#define MU_ID_SETPUBLISHINGMODEREQUEST 799
#define MU_ID_SETPUBLISHINGMODERESPONSE 802
#define MU_ID_PUBLISHREQUEST 826
#define MU_ID_PUBLISHRESPONSE 829
#define MU_ID_REPUBLISHREQUEST 832
#define MU_ID_REPUBLISHRESPONSE 835
#define MU_ID_DELETESUBSCRIPTIONSREQUEST 847
#define MU_ID_DELETESUBSCRIPTIONSRESPONSE 850

#define MU_ID_TRANSFERSUBSCRIPTIONSREQUEST 841
#define MU_ID_TRANSFERSUBSCRIPTIONSRESPONSE 844

#define MU_ID_AGGREGATEFILTER_ENCODING_DEFAULTBINARY 730
#define MU_ID_AGGREGATETYPE_AVERAGE 2342
#define MU_ID_AGGREGATETYPE_MINIMUM 2346
#define MU_ID_AGGREGATETYPE_MAXIMUM 2347

/* Additional OPC-10000-13 AggregateFunctionType IDs (MUC_OPCUA_AGGREGATE_FULL).
 * Every value is the AggregateFunction_<Name> Object NodeId from the OPC
 * Foundation UA NodeSet NodeIds.csv. MU_ID_AGGREGATETYPE_MINIMUM (2346) /
 * _MAXIMUM (2347) are AggregateFunction_Minimum / _Maximum. Twelve of the
 * values below were previously wrong (some swapped, e.g. Count<->AnnotationCount
 * and Start<->End; PercentGood/PercentBad/WorstQuality collided with the Data
 * Access DataItemType nodes 2365/2366/2367) — since subscription_aggregate.c
 * dispatches on these ids, a wrong value mis-handled the client's requested
 * aggregate. Verified against NodeIds.csv. */
#define MU_ID_AGGREGATETYPE_INTERPOLATIVE 2341
#define MU_ID_AGGREGATETYPE_TIME_AVERAGE 2343
#define MU_ID_AGGREGATETYPE_TOTAL 2344
#define MU_ID_AGGREGATETYPE_RANGE 2350
#define MU_ID_AGGREGATETYPE_ANNOTATION_COUNT 2351
#define MU_ID_AGGREGATETYPE_COUNT 2352
#define MU_ID_AGGREGATETYPE_START 2357
#define MU_ID_AGGREGATETYPE_END 2358
#define MU_ID_AGGREGATETYPE_DELTA 2359
#define MU_ID_AGGREGATETYPE_DURATION_GOOD 2360
#define MU_ID_AGGREGATETYPE_DURATION_BAD 2361
#define MU_ID_AGGREGATETYPE_PERCENT_GOOD 2362
#define MU_ID_AGGREGATETYPE_PERCENT_BAD 2363
#define MU_ID_AGGREGATETYPE_WORST_QUALITY 2364
#define MU_ID_AGGREGATETYPE_TIME_AVERAGE_2 11285
#define MU_ID_AGGREGATETYPE_MINIMUM_2 11286
#define MU_ID_AGGREGATETYPE_MAXIMUM_2 11287
#define MU_ID_AGGREGATETYPE_WORST_QUALITY_2 11292
#define MU_ID_AGGREGATETYPE_TOTAL_2 11304
#define MU_ID_AGGREGATETYPE_DURATION_IN_STATE_ZERO 11307
#define MU_ID_AGGREGATETYPE_DELTA_BOUNDS 11507

/* Data Access Server Facet (OPC-10000-8 §5.3/§5.6). NOTE: there is no single
   "EURange"/"EngineeringUnits" NodeId — each is a per-type Property instance
   declaration (AnalogItemType EURange=2369, EngineeringUnits=2371, ...; see
   base_nodes.c) and on an instance the Property carries an integrator-assigned
   NodeId, matched by its BrowseName. Only the type/DataType NodeIds are globally
   fixed and grounded here. */
#define MU_ID_ANALOGITEMTYPE 2368        /* AnalogItemType VariableType */
#define MU_ID_RANGE_DATATYPE 884         /* Range DataType (low, high) */
#define MU_ID_EUINFORMATION_DATATYPE 887 /* EUInformation DataType */

/* Server Nano Core Type System & Base Info NodeIds (OPC-10000-5).
 * Consumed by the optional CU implementations in base_nodes.c; each is
 * gated by its corresponding MUC_OPCUA_CU_* Kconfig symbol. */
#define MU_NODEID_TIMEZONEDATATYPE 8912   /* TimeZoneDataType DataType, OPC-10000-5 §12.2.12.11 */
#define MU_NODEID_OPTIONSETTYPE 11487     /* OptionSetType VariableType, OPC-10000-5 §7.17 */
#define MU_NODEID_SELECTIONLISTTYPE 16309 /* SelectionListType VariableType, OPC-10000-5 §7.18 */
#define MU_NODEID_BASEINTERFACETYPE 17602 /* BaseInterfaceType ObjectType, OPC-10000-5 §6.9 */
#define MU_NODEID_HASINTERFACE 17603      /* HasInterface ReferenceType, OPC-10000-5 §11.20 */
#define MU_NODEID_HASADDIN 17604          /* HasAddIn ReferenceType, OPC-10000-5 §11.21 */
#define MU_NODEID_CURRENCYUNITTYPE 23498  /* CurrencyUnitType DataType, OPC-10000-5 §12.2.12.2 */

/* T002 lookup outcome (OPC-10000-5 via opc-ua-reference MCP, 2026-07).
 * The following instance-declaration / folder NodeIds are NOT numerically
 * assigned in the OPC-10000-5 spec text — that spec defines them by
 * BrowseName only (ServerType table §6.3.1; AddressSpace folders §8.2).
 * Their numeric NodeIds live in the OPC UA NodeSet (NodeIds.csv / Opc.Ua.Nodeset.xml),
 * which is not currently vendored in this repo and is not exposed by the
 * opc-ua-reference MCP search tools. Tasks T010-T015 should either:
 *   (a) vendor a minimal NodeIds.csv snapshot and look them up there, or
 *   (b) follow the research.md fallback (Decision 5 / "Note on Server Property
 *       NodeIds") and define the nodes conceptually by BrowseName + type,
 *       letting the integrator assign an instance NodeId.
 *
 *   Unresolved (no spec-grounded numeric NodeId found):
 *     - Server.LocalTime Property            (OPC-10000-5 §6.3.1, ServerType)
 *     - Server.EstimatedReturnTime Property   (OPC-10000-5 §6.3.1, ServerType)
 *     - Locations Folder under Objects        (OPC-10000-5 §8.2.12)
 *     - InterfaceTypes Folder under Types     (OPC-10000-5 §8.2.11)
 *     - CurrencyUnit Property                 (OPC-10000-5 §12.2.12.2)
 *     - ValueAsText Property                  (per-type instance declaration,
 *                                              e.g. i=11461 on MultiStateValueDiscreteType
 *                                              per docs/conformance/data-access.md —
 *                                              no single global NodeId)
 *     - DefaultInstanceBrowseName Property    (no numeric NodeId in spec text)
 *     - EngineeringUnits Property             (per-type instance declaration,
 *                                              e.g. i=2371 on AnalogItemType —
 *                                              no single global NodeId; see comment
 *                                              block above next to MU_ID_ANALOGITEMTYPE)
 */

#endif /* MUC_OPCUA_OPCUA_IDS_H */
