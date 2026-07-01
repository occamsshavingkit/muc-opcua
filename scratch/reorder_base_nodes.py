# scratch/reorder_base_nodes.py
import re

with open('src/address_space/base_nodes.c', 'r') as f:
    content = f.read()

# We want to parse s_base_nodes[] block.
# Let's find the content between 'static const mu_node_t s_base_nodes[] = {' and '};'
start_marker = 'static const mu_node_t s_base_nodes[] = {'
end_marker = '};\n\nstatic const mu_address_space_t s_base_space'

start_idx = content.find(start_marker)
end_idx = content.find(end_marker)

if start_idx == -1 or end_idx == -1:
    print("Could not find s_base_nodes array limits")
    exit(1)

array_content = content[start_idx + len(start_marker):end_idx]

# Let's parse individual node initializers.
# A node initializer starts with optional preprocessor directives, then '{{0, MU_NODEID_NUMERIC, {id}},'
# and ends with '},' or '}, \n' or preprocessor directives.
# Since we have lines in base_nodes.c, let's split by lines and parse carefully.

lines = array_content.split('\n')
nodes = []
current_node_lines = []
current_cond = []

# We trace active preprocessor conditions
for line in lines:
    stripped = line.strip()
    if stripped.startswith('#if'):
        # If we have a pending node, save it first? No, preprocessor directives shouldn't split a node,
        # but in our file, they are between nodes or inside node references.
        # Wait, the only preprocessor directives at node level are:
        # '#if MUC_OPCUA_BASE_TYPE_SYSTEM'
        # '#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD'
        # '#endif'
        if 'MUC_OPCUA_BASE_TYPE_SYSTEM' in stripped:
            current_cond.append('MUC_OPCUA_BASE_TYPE_SYSTEM')
            continue
        elif 'MUC_OPCUA_SUBSCRIPTIONS_STANDARD' in stripped:
            current_cond.append('MUC_OPCUA_SUBSCRIPTIONS_STANDARD')
            continue
    elif stripped.startswith('#endif'):
        if current_cond:
            current_cond.pop()
        continue
    elif stripped.startswith('#else'):
        # Not expected at node level, but let's check
        continue
        
    if not stripped:
        if current_node_lines:
            current_node_lines.append(line)
        continue

    # Detect start of a new node by matching '{{0, MU_NODEID_NUMERIC, {id}},'
    # Wait, node id can have namespace index other than 0? All base nodes are ns=0.
    match = re.match(r'^\s*\{\{0,\s*MU_NODEID_NUMERIC,\s*\{(\d+)\}\},', stripped)
    if match:
        if current_node_lines:
            # save previous node
            node_text = '\n'.join(current_node_lines)
            # Find the ID of the previous node
            prev_match = re.search(r'\{\{0,\s*MU_NODEID_NUMERIC,\s*\{(\d+)\}\},', node_text)
            prev_id = int(prev_match.group(1))
            nodes.append((prev_id, list(current_node_lines), list(nodes[-1][3] if nodes else []))) # we will assign cond later
            current_node_lines = []
            
    current_node_lines.append(line)

if current_node_lines:
    node_text = '\n'.join(current_node_lines)
    prev_match = re.search(r'\{\{0,\s*MU_NODEID_NUMERIC,\s*\{(\d+)\}\},', node_text)
    prev_id = int(prev_match.group(1))
    nodes.append((prev_id, current_node_lines, list(current_cond)))

# Now, we need to associate each node with its condition at the time of parsing.
# Let's fix the conditions. We can re-parse using a state machine that tracks line numbers.
# Let's write a robust parser.
nodes_debug = []
print(f"Roughly parsed {len(nodes)} nodes.")
