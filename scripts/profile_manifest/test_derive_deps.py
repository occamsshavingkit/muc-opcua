import json, derive_deps as d


def _fixtures():
    graph = {"profiles": {
        "1219": {"name": "Exposes Type System Server Facet",
                 "profileUri": "…/ExposesTypeSystem",
                 "child_profiles": [], "child_cus": [
                     {"id": 9001, "name": "Base Info ServerType", "isOptional": False},
                     {"id": 9002, "name": "Base Info Type Information", "isOptional": False}]}},
        "cu_master": {}}
    manifest = {"items": [
        {"id": "opc_facet_1219", "kind": "facet",
         "kconfig_symbol": "MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER",
         "opc_reference": {"profile_id": "1219"}},
        {"id": "opc_cu_3189", "kind": "conformance_unit",
         "kconfig_symbol": "MUC_OPCUA_CU_BASE_INFO_SERVERTYPE",
         "opc_reference": {"cu_id": "3189", "cu_name": "Base Info ServerType"}},
    ]}
    return graph, manifest


def test_facet_symbol_by_graph_id():
    graph, manifest = _fixtures()
    idx = d.build_index(manifest)
    assert d.facet_symbol_for_graph_id(idx, "1219") == "MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER"


def test_item_for_cu_name_prefers_symbol_carrier():
    graph, manifest = _fixtures()
    idx = d.build_index(manifest)
    it = d.selectable_item_for_cu_name(idx, "Base Info ServerType")
    assert it["kconfig_symbol"] == "MUC_OPCUA_CU_BASE_INFO_SERVERTYPE"


def test_depends_on_single_facet():
    graph, manifest = _fixtures()
    idx = d.build_index(manifest)
    deps, op = d.derive_depends_on(graph, idx, "Base Info ServerType")
    assert deps == ["MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER"] and op is None


def test_depends_on_multi_facet_is_or():
    graph, manifest = _fixtures()
    # add a second modeled facet that also contains ServerType
    graph["profiles"]["1322"] = {"name": "Core 2022 Server Facet", "child_profiles": [],
        "child_cus": [{"id": 9001, "name": "Base Info ServerType", "isOptional": False}]}
    manifest["items"].append({"id": "opc_facet_1322", "kind": "facet",
        "kconfig_symbol": "MUC_OPCUA_FACET_CORE_2022_SERVER", "opc_reference": {"profile_id": "1322"}})
    idx = d.build_index(manifest)
    deps, op = d.derive_depends_on(graph, idx, "Base Info ServerType")
    assert set(deps) == {"MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER", "MUC_OPCUA_FACET_CORE_2022_SERVER"}
    assert op == "or"
