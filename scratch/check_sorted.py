# scratch/check_sorted.py
import re

with open('src/address_space/base_nodes.c', 'r') as f:
    content = f.read()

# Match the array content inside static const mu_node_t s_base_nodes[] = { ... };
array_match = re.search(r'static const mu_node_t s_base_nodes\[\] = \{(.*?)\};', content, re.DOTALL)
if not array_match:
    print("Could not find s_base_nodes")
    exit(1)

array_content = array_match.group(1)

# Find all initializers like {{ns, MU_NODEID_NUMERIC, {id}}, ...}
node_ids = re.findall(r'\{\{(\d+),\s*MU_NODEID_NUMERIC,\s*\{(\d+)\}\}', array_content)

print(f"Parsed {len(node_ids)} nodes.")
sorted_ids = sorted(node_ids, key=lambda x: (int(x[0]), int(x[1])))
if node_ids == sorted_ids:
    print("YES, s_base_nodes is ALREADY sorted!")
else:
    print("NO, s_base_nodes is NOT sorted.")
    for i, (ns, nid) in enumerate(node_ids):
        print(f"{i}: ns={ns}, id={nid}")
