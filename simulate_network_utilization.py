import math

# single node routing table
def generate_local_routing_table_dense(node_count, verbose = False):
  half_node_count = node_count / 2
  max_shift = math.floor(math.log2(half_node_count)) # max possible 2^n in half_node_count
  routing_table = {}
  for node_idx in range(node_count):
    connected_indices = set()
    # self
    connected_indices.add(node_idx)
    for shift in range(0, max_shift + 1):
      connected_indices.add((node_idx + (1 << shift)) % node_count)
      connected_indices.add((node_idx - (1 << shift)) % node_count)
    routing_table[node_idx] = connected_indices
  if verbose:
    print(f"[INFO] Routing Table Generated:")
    print(routing_table)
  return routing_table

def next_hop(target, routing_table):
  return min(routing_table, key=lambda x: abs(x - target))

def _get_hop_count(source_node, target_node, routing_table):
  current_node = source_node
  hop_count = 0
  while(True):
    next_node = next_hop(target_node, routing_table[current_node])
    if next_node == current_node:
      break
    elif next_node == target_node:
      hop_count += 1
      break
    else:
      current_node = next_node
      hop_count += 1
  return hop_count

def get_hop_count(node_count, routing_table):
  source_node = 0
  target_node_lst = [i for i in range(node_count)]
  hop_count_lst = []
  for target_node in target_node_lst:
    count1 = _get_hop_count(source_node, target_node, routing_table)
    count2 = _get_hop_count(target_node, source_node, routing_table)
    hop_count_lst.append(count1 + count2)
  return hop_count_lst
  

log_node_count_lst = [i for i in range(0, 11)]
node_count_lst = [(1 << i) for i in log_node_count_lst]
# for node_count in node_count_lst:
# for node_count in [6]:
#   routing_table = generate_local_routing_table_dense(node_count, True)
#   hop_count_lst = get_hop_count(node_count, routing_table)
#   print('[INFO] Hop Count List')
#   print(hop_count_lst)
#   max_hop_count = max(hop_count_lst)
#   avg_hop_count = sum(hop_count_lst)/len(hop_count_lst)
#   print(f'[INFO] {max_hop_count=}, {avg_hop_count=}')
#   print(f'[INFO] Network Bandwidth Utilization: {avg_hop_count*512/4096/8:.4f}')

result = {'node_count':[], 'max_hop_count':[], 'avg_hop_count':[]}
# for node_count in range(1, 11):
for node_count in node_count_lst:
  routing_table = generate_local_routing_table_dense(node_count, False)
  hop_count_lst = get_hop_count(node_count, routing_table)
  max_hop_count = max(hop_count_lst)
  avg_hop_count = sum(hop_count_lst)/len(hop_count_lst)
  result['node_count'].append(node_count)
  result['max_hop_count'].append(max_hop_count)
  result['avg_hop_count'].append(avg_hop_count)
print(result)
  
