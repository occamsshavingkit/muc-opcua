import json, graph_deps as d


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


def test_profile_defaults_mandatory_via_chain():
    # Embedded(2268) ⊃ facet 1219 (mandatory) ⊃ ServerType (mandatory) => embedded True; nano/micro False
    graph = {"profiles": {
        "2266": {"name": "Nano …Profile", "child_profiles": [], "child_cus": []},
        "2267": {"name": "Micro …Profile", "child_profiles": [{"id": 2266, "name": "Nano …Profile", "isOptional": False}], "child_cus": []},
        "2268": {"name": "Embedded …Profile", "child_profiles": [
                   {"id": 2267, "name": "Micro …Profile", "isOptional": False},
                   {"id": 1219, "name": "Exposes Type System Server Facet", "isOptional": False}], "child_cus": []},
        "2269": {"name": "Standard …Profile", "child_profiles": [{"id": 2268, "name": "Embedded …Profile", "isOptional": False}], "child_cus": []},
        "1219": {"name": "Exposes Type System Server Facet", "child_profiles": [],
                "child_cus": [{"id": 9001, "name": "Base Info ServerType", "isOptional": False},
                             {"id": 9003, "name": "Base Info EUInformation", "isOptional": True}]}},
        "cu_master": {}}
    pd = d.derive_profile_defaults(graph, "Base Info ServerType")
    assert pd == {"nano": False, "micro": False, "embedded": True, "standard": True}
    pd_opt = d.derive_profile_defaults(graph, "Base Info EUInformation")
    assert pd_opt == {"nano": False, "micro": False, "embedded": False, "standard": False}  # optional => not default


def _full_graph():
    """The Task-4 profile/facet-membership graph, keyed for reuse by apply() tests."""
    return {"profiles": {
        "2266": {"name": "Nano …Profile", "child_profiles": [], "child_cus": []},
        "2267": {"name": "Micro …Profile", "child_profiles": [{"id": 2266, "name": "Nano …Profile", "isOptional": False}], "child_cus": []},
        "2268": {"name": "Embedded …Profile", "child_profiles": [
                   {"id": 2267, "name": "Micro …Profile", "isOptional": False},
                   {"id": 1219, "name": "Exposes Type System Server Facet", "isOptional": False}], "child_cus": []},
        "2269": {"name": "Standard …Profile", "child_profiles": [{"id": 2268, "name": "Embedded …Profile", "isOptional": False}], "child_cus": []},
        "1219": {"name": "Exposes Type System Server Facet", "child_profiles": [],
                "child_cus": [{"id": 9001, "name": "Base Info ServerType", "isOptional": False},
                             {"id": 9003, "name": "Base Info EUInformation", "isOptional": True}]}},
        "cu_master": {}}


def test_apply_preserves_full_and_custom():
    graph = _full_graph()
    manifest = {"items": [
        {"id": "opc_facet_1219", "kind": "facet",
         "kconfig_symbol": "MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER",
         "opc_reference": {"profile_id": "1219"}},
        {"id": "opc_cu_3189", "kind": "conformance_unit",
         "kconfig_symbol": "MUC_OPCUA_CU_BASE_INFO_SERVERTYPE",
         "opc_reference": {"cu_id": "3189", "cu_name": "Base Info ServerType"},
         "depends_on": ["MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER", "MUC_OPCUA_CU_BASE_INFO_BASE_TYPES"],
         "profile_defaults": {"nano": True, "micro": True, "embedded": True, "standard": True,
                               "full": True, "custom": False}},
    ]}
    d.apply(manifest, graph)
    it = next(i for i in manifest["items"] if i["id"] == "opc_cu_3189")
    assert it["depends_on"] == ["MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER"]  # BASE_TYPES dropped
    assert "depends_on_op" not in it or it["depends_on_op"] is None
    assert it["profile_defaults"]["embedded"] is True and it["profile_defaults"]["nano"] is False
    assert it["profile_defaults"]["full"] is True  # preserved, not derived
    assert it["profile_defaults"]["custom"] is False  # preserved, not derived


def test_apply_skips_items_without_cu_name_mapping():
    graph = _full_graph()
    untouched = {"id": "opc_facet_only", "kind": "facet",
                 "kconfig_symbol": "MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER",
                 "opc_reference": {"profile_id": "1219"}}
    no_cu_name = {"id": "opc_cu_no_name", "kind": "conformance_unit",
                  "kconfig_symbol": "MUC_OPCUA_CU_UNMAPPED",
                  "opc_reference": {"cu_id": "9999"}, "depends_on": ["SOMETHING"]}
    manifest = {"items": [untouched, dict(no_cu_name)]}
    d.apply(manifest, graph)
    assert manifest["items"][0] == untouched  # kind != conformance_unit: untouched
    assert manifest["items"][1]["depends_on"] == ["SOMETHING"]  # no cu_name: untouched


def test_apply_preserves_hand_authored_values_when_cu_name_absent_from_graph():
    # cu_name IS present but does not appear anywhere in the graph's child_cus
    # (e.g. a merged/combined manifest CU referencing a legacy facet the graph
    # doesn't model). We have no authoritative data for it, so apply() must
    # leave its pre-existing depends_on/profile_defaults untouched rather than
    # deriving all-false profile_defaults.
    graph = _full_graph()
    hand_authored = {
        "id": "service_browse", "kind": "conformance_unit",
        "kconfig_symbol": "MUC_OPCUA_CU_VIEW_BASIC_TRANSLATEBROWSEPATH",
        "opc_reference": {"cu_name": "View Basic / TranslateBrowsePath"},
        "depends_on": [],
        "profile_defaults": {"nano": True, "micro": True, "embedded": True, "standard": True,
                              "full": True, "custom": True},
    }
    manifest = {"items": [dict(hand_authored)]}
    d.apply(manifest, graph)
    it = manifest["items"][0]
    assert it["depends_on"] == []
    assert it["profile_defaults"] == {
        "nano": True, "micro": True, "embedded": True, "standard": True,
        "full": True, "custom": True,
    }
