# scratch/sort_base_nodes_braces.py
import re

with open('src/address_space/base_nodes.c', 'r') as f:
    content = f.read()

start_marker = 'static const mu_node_t s_base_nodes[] = {'
end_marker = '};\n\nstatic const mu_address_space_t s_base_space'

start_idx = content.find(start_marker)
end_idx = content.find(end_marker)

if start_idx == -1 or end_idx == -1:
    print("Could not find s_base_nodes array limits")
    exit(1)

array_content = content[start_idx + len(start_marker):end_idx]

lines = array_content.split('\n')
nodes = []
current_node_lines = []
current_conds = []
node_active_conds = []
brace_count = 0

for line in lines:
    stripped = line.strip()
    if not stripped:
        if current_node_lines:
            current_node_lines.append(line)
        continue

    # If brace_count is 0, any preprocessor directive is outer!
    if brace_count == 0:
        if stripped.startswith('#if'):
            current_conds.append(line)
            continue
        elif stripped.startswith('#endif'):
            if current_conds:
                current_conds.pop()
            continue
        elif stripped.startswith('#else'):
            continue

    # Detect node start
    if brace_count == 0 and stripped.startswith('{{0,') and 'MU_NODEID_NUMERIC' in stripped:
        if current_node_lines:
            node_text = '\n'.join(current_node_lines)
            match = re.search(r'\{\{0,\s*MU_NODEID_NUMERIC,\s*\{(\d+)\}\},', node_text)
            if match:
                node_id = int(match.group(1))
                nodes.append({
                    'id': node_id,
                    'text': node_text,
                    'conds': list(node_active_conds)
                })
            current_node_lines = []
        node_active_conds = list(current_conds)

    current_node_lines.append(line)

    # Update brace count
    for char in line:
        if char == '{':
            brace_count += 1
        elif char == '}':
            brace_count -= 1

if current_node_lines:
    node_text = '\n'.join(current_node_lines)
    match = re.search(r'\{\{0,\s*MU_NODEID_NUMERIC,\s*\{(\d+)\}\},', node_text)
    if match:
        node_id = int(match.group(1))
        nodes.append({
            'id': node_id,
            'text': node_text,
            'conds': list(node_active_conds)
        })

print(f"Parsed {len(nodes)} nodes successfully.")

# Sort nodes by ID
sorted_nodes = sorted(nodes, key=lambda x: x['id'])

new_array_lines = []
for node in sorted_nodes:
    if node['conds']:
        for cond in node['conds']:
            new_array_lines.append(cond)
        new_array_lines.append(node['text'])
        for _ in node['conds']:
            new_array_lines.append('#endif')
    else:
        new_array_lines.append(node['text'])

new_array_content = '\n'.join(new_array_lines)

# Replace in content
new_content = content[:start_idx + len(start_marker)] + '\n' + new_array_content + '\n' + content[end_idx:]

with open('src/address_space/base_nodes.c', 'w') as f:
    f.write(new_content)

print("Successfully reordered base_nodes.c!")
