import sys
import random
import os
import heapq
import math

# ================= CONFIGURATION =================
BLOCK_SIZE_KB = 1  # 1KB per block
# Average bytes per row (Conservative estimates for CSV text data)
NODE_ROW_BYTES = 12  # e.g., "100,1,1\n"
EDGE_ROW_BYTES = 25  # e.g., "100,200,50,1,1\n"

# Derived Capacities
NODES_PER_BLOCK = (BLOCK_SIZE_KB * 1024) // NODE_ROW_BYTES
EDGES_PER_BLOCK = (BLOCK_SIZE_KB * 1024) // EDGE_ROW_BYTES

class GraphOracle:
  def __init__(self, name, directed):
    self.name = name
    self.directed = directed
    self.nodes = {} # id -> tuple of node boolean attrs (a1,a2,...)
    self.edges = {} # (u, v) -> weight (canonical for undirected)
    self.adj = {}   # u -> [(v, w, b1, b2, ...), ...]
    self.rev_adj = {} # v -> [u, ...] for directed in-degree
    self.edge_attrs = {} # (u,v) -> tuple(b1,b2,...)

  def add_node(self, nid, *attrs):
    self.nodes[nid] = tuple(attrs)
    if nid not in self.adj: self.adj[nid] = []
    if nid not in self.rev_adj: self.rev_adj[nid] = []

  def add_edge(self, u, v, w, *battrs):
    self.adj[u].append((v, w) + tuple(battrs))
    self.edge_attrs[(u,v)] = tuple(battrs)
    if self.directed:
      self.rev_adj[v].append(u)
    else:
      self.adj[v].append((u, w) + tuple(battrs))
      self.edge_attrs[(v,u)] = tuple(battrs)

  def get_degree(self, nid):
    if nid not in self.nodes: return "Node does not exist"
    if not self.directed:
      return len(self.adj[nid])
    else:
      return len(self.adj[nid]) + len(self.rev_adj[nid])

  # conditions: list of dicts describing node/edge constraints
  # each condition dict:
  #  { 'target': 'N'|'E', 'type': 'ATTR_EQ'|'ATTR_UNIFORM'|'ANY_EQ'|'ANY_UNIFORM',
  #    'attr_index': 0-based index or None, 'value': 0/1 or None }
  def get_shortest_path(self, src, dst, conditions=None):
    if src not in self.nodes or dst not in self.nodes:
      return "Node does not exist"

    # quick helpers
    node_attr_count = len(next(iter(self.nodes.values())))
    edge_attr_count = len(next(iter(self.edge_attrs.values()))) if self.edge_attrs else 0

    # split conditions by target
    node_conds = [c for c in (conditions or []) if c['target'] == 'N']
    edge_conds = [c for c in (conditions or []) if c['target'] == 'E']

    # Validate condition attribute indexes (defensive)
    for c in node_conds:
      if c['type'].startswith('ATTR') and (c['attr_index'] is None or c['attr_index'] >= node_attr_count):
        return "FALSE"  # impossible condition given node attr count
    for c in edge_conds:
      if c['type'].startswith('ATTR') and (c['attr_index'] is None or c['attr_index'] >= edge_attr_count):
        return "FALSE"

    # State representation:
    #  node_uniform_vals: tuple of length node_attr_count where:
    #    - value 0/1 means that attribute has been fixed to that value across path so far
    #    - value -1 means not involved or not yet fixed/irrelevant
    #  node_any_candidates: frozenset of (attr_index, value) pairs still possible for ANY constraints
    # similarly for edges: edge_uniform_vals, edge_any_candidates
    #
    # We'll only track fields relevant to present conditions to avoid unnecessary state bloat.

    # helpers to create initial state from source node
    def init_node_state_for_source(src_node_attrs):
      # node_uniform_vals starts as list with -1
      node_uniform_vals = [-1] * node_attr_count
      # for each ATTR_UNIFORM for a specific index: set to the src value
      for c in node_conds:
        if c['type'] == 'ATTR_UNIFORM':
          ai = c['attr_index']
          node_uniform_vals[ai] = src_node_attrs[ai]
        if c['type'] == 'ATTR_EQ':
          # must match immediately
          ai = c['attr_index']
          if src_node_attrs[ai] != c['value']:
            return None  # source fails equality
        if c['type'] == 'ANY_EQ':
          # ANY_EQ: candidate attributes are those whose src value == required
          pass
        if c['type'] == 'ANY_UNIFORM':
          pass

      node_any_candidates = set()
      # initialize any-type node candidates based on source node
      any_eq_present = any(c['type'] == 'ANY_EQ' for c in node_conds)
      any_uniform_present = any(c['type'] == 'ANY_UNIFORM' for c in node_conds)

      if any_eq_present:
        val = next(c['value'] for c in node_conds if c['type'] == 'ANY_EQ')
        for ai in range(node_attr_count):
          if src_node_attrs[ai] == val:
            node_any_candidates.add((ai, val))
        if not node_any_candidates:
          return None

      if any_uniform_present:
        # any_uniform allows either 0 or 1 but attribute index must be same across path.
        for ai in range(node_attr_count):
          node_any_candidates.add((ai, src_node_attrs[ai]))

      return (tuple(node_uniform_vals), frozenset(node_any_candidates))

    # edge initial state (no edges yet)
    def init_edge_state_for_source():
      edge_uniform_vals = [-1] * edge_attr_count
      edge_any_candidates = set()
      # nothing to filter yet; but if there's an ATTR_EQ edge condition it applies only when an edge is seen
      return (tuple(edge_uniform_vals), frozenset(edge_any_candidates))

    init_node_state = init_node_state_for_source(self.nodes[src])
    if init_node_state is None:
      # source fails node-equality conditions
      return "FALSE"
    init_edge_state = init_edge_state_for_source()

    # Dijkstra over extended state
    # state key: (node, node_uniform_vals, node_any_cands, edge_uniform_vals, edge_any_cands)
    start_key = (src, init_node_state[0], init_node_state[1], init_edge_state[0], init_edge_state[1])
    pq = [(0, start_key)]
    dist = {start_key: 0}

    while pq:
      d, key = heapq.heappop(pq)
      if d != dist.get(key, None): continue
      u, node_uniform_vals, node_any_cands, edge_uniform_vals, edge_any_cands = key
      if u == dst:
        # final validation: ensure ANY constraints still have at least one candidate
        # and ATTR_UNIFORM constraints (node_uniform_vals) are okay (they always will be by construction)
        node_any_ok = True
        edge_any_ok = True
        if any(c['type'] == 'ANY_EQ' for c in node_conds) or any(c['type'] == 'ANY_UNIFORM' for c in node_conds):
          node_any_ok = len(node_any_cands) > 0
        if any(c['type'] == 'ANY_EQ' for c in edge_conds) or any(c['type'] == 'ANY_UNIFORM' for c in edge_conds):
          # a path must contain at least one edge for edge ANY/uniform to be meaningful;
          # but if there were 0 edges (src==dst) then it's a path of length 0: it's valid only if edge constraints don't require edges
          # We'll consider edge_any_ok true only if edge_any_cands non-empty or no edge ANY constraints exist.
          edge_any_ok = len(edge_any_cands) > 0 or not (any(c['type'].startswith('ANY') for c in edge_conds))
        if node_any_ok and edge_any_ok:
          return f"TRUE {d}"
        else:
          # can't accept this path; keep searching
          pass

      # explore neighbors
      for entry in self.adj[u]:
        # entry: (v, w, *edge_attrs)
        v = entry[0]
        w = entry[1]
        edge_attrs = tuple(entry[2:]) if len(entry) > 2 else tuple()
        vnode_attrs = self.nodes[v]

        # --- local checks & state updates for this edge and the destination node ---
        # 1) edge-level ATTR_EQ: must hold for this edge
        bad = False
        for c in edge_conds:
          if c['type'] == 'ATTR_EQ':
            ai = c['attr_index']
            if edge_attrs[ai] != c['value']:
              bad = True
              break
        if bad: continue

        # 2) node-level ATTR_EQ: destination node must satisfy
        for c in node_conds:
          if c['type'] == 'ATTR_EQ':
            ai = c['attr_index']
            if vnode_attrs[ai] != c['value']:
              bad = True
              break
        if bad: continue

        # 3) Update node_uniform_vals: check consistency for ATTR_UNIFORM
        new_node_uniform = list(node_uniform_vals)
        for c in node_conds:
          if c['type'] == 'ATTR_UNIFORM':
            ai = c['attr_index']
            prev = new_node_uniform[ai]
            if prev == -1:
              new_node_uniform[ai] = vnode_attrs[ai]
            else:
              if prev != vnode_attrs[ai]:
                bad = True
                break
        if bad: continue

        # 4) Update node_any_candidates
        new_node_any = set(node_any_cands)
        # If node_any_cands is empty but node ANY conditions present, initialize from source was done earlier.
        for c in node_conds:
          if c['type'] == 'ANY_EQ':
            val = c['value']
            # intersect candidates: keep only those (ai,val) where vnode_attrs[ai]==val
            new_node_any = set(filter(lambda x: vnode_attrs[x[0]] == val, new_node_any))
          if c['type'] == 'ANY_UNIFORM':
            # keep candidate attributes whose value at v matches their recorded value
            if not new_node_any:
              # first edge/node after source: initialize possible attributes with v's values (but source init already added)
              for ai in range(node_attr_count):
                new_node_any.add((ai, vnode_attrs[ai]))
            else:
              new_node_any = set(filter(lambda x: vnode_attrs[x[0]] == x[1], new_node_any))
        # If ANY conditions exist but new_node_any becomes empty => invalid path prefix
        if (any(c['type'].startswith('ANY') for c in node_conds)) and not new_node_any:
          continue

        # 5) Update edge_uniform_vals for ATTR_UNIFORM (edges)
        new_edge_uniform = list(edge_uniform_vals)
        for c in edge_conds:
          if c['type'] == 'ATTR_UNIFORM':
            ai = c['attr_index']
            prev = new_edge_uniform[ai]
            if prev == -1:
              new_edge_uniform[ai] = edge_attrs[ai]
            else:
              if prev != edge_attrs[ai]:
                bad = True
                break
        if bad: continue

        # 6) Update edge_any_candidates
        new_edge_any = set(edge_any_cands)
        for c in edge_conds:
          if c['type'] == 'ANY_EQ':
            val = c['value']
            if not new_edge_any:
              # initialize using this first edge
              for ai in range(edge_attr_count):
                if edge_attrs[ai] == val:
                  new_edge_any.add((ai, val))
            else:
              new_edge_any = set(filter(lambda x: x[1] == val and edge_attrs[x[0]] == val, new_edge_any))
          if c['type'] == 'ANY_UNIFORM':
            if not new_edge_any:
              for ai in range(edge_attr_count):
                new_edge_any.add((ai, edge_attrs[ai]))
            else:
              new_edge_any = set(filter(lambda x: edge_attrs[x[0]] == x[1], new_edge_any))
        if (any(c['type'].startswith('ANY') for c in edge_conds)) and not new_edge_any:
          continue

        # build next state
        next_key = (v, tuple(new_node_uniform), frozenset(new_node_any), tuple(new_edge_uniform), frozenset(new_edge_any))
        nd = d + w
        if nd < dist.get(next_key, float('inf')):
          dist[next_key] = nd
          heapq.heappush(pq, (nd, next_key))

    return "FALSE"

# ================= GENERATOR =================

def generate_data(name, num_nodes, num_edges, is_directed):
  suffix = "D" if is_directed else "U"
  os.makedirs("data", exist_ok=True)

  oracle = GraphOracle(name, is_directed)

  # We'll use two node attrs (A1,A2) and two edge attrs (B1,B2) like your original
  NODE_ATTRS = 5
  EDGE_ATTRS = 5

  # 1. Nodes
  with open(f"data/{name}_Nodes_{suffix}.csv", "w") as f:
    f.write("NodeID," + ",".join(f"A{i+1}" for i in range(NODE_ATTRS)) + "\n")
    for i in range(num_nodes):
      a = [random.randint(0,1) for _ in range(NODE_ATTRS)]
      oracle.add_node(i, *a)
      f.write(f"{i}," + ",".join(str(x) for x in a) + "\n")

  # 2. Edges
  generated = set()
  with open(f"data/{name}_Edges_{suffix}.csv", "w") as f:
    f.write("Src_NodeID,Dest_NodeID,Weight," + ",".join(f"B{i+1}" for i in range(EDGE_ATTRS)) + "\n")
    count = 0
    attempts = 0
    max_attempts = num_edges * 10 + 1000
    while count < num_edges and attempts < max_attempts:
      attempts += 1
      u, v = random.randint(0, num_nodes-1), random.randint(0, num_nodes-1)
      if u == v: continue

      key = (u,v) if is_directed else tuple(sorted((u,v)))
      if key in generated: continue
      generated.add(key)

      w = random.randint(1, 100)
      b = [random.randint(0,1) for _ in range(EDGE_ATTRS)]
      oracle.add_edge(u, v, w, *b)
      f.write(f"{u},{v},{w}," + ",".join(str(x) for x in b) + "\n")
      count += 1

  return oracle

# ================= TEST SUITE WRITER =================

def write_test_suite(oracle_d, oracle_u):
  sql_file = open("test_suite.sql", "w")
  ans_file = open("test_expected_answers.txt", "w")

  def log(cmd):
    sql_file.write(cmd + "\n")

  def expect(desc, cmd, answer, block_math):
    ans_file.write(f"--- {desc} ---\n")
    ans_file.write(f"CMD: {cmd}\n")
    ans_file.write(f"EXP: {answer}\n")
    ans_file.write(f"BLOCKS (Min): {block_math}\n\n")

  # --- 1. LOAD ERRORS ---
  ans_file.write("=== SECTION 1: SYNTACTIC & SEMANTIC ERRORS ===\n")

  # Syntactic
  bad_cmds = [
    "LOAD", "LOAD GRAPH", "LOAD GRAPH A", "LOAD GRAPH A B C",
    "EXPORT", "EXPORT GRAPH", "DEGREE", "DEGREE A",
    "PATH A B", "RES <- PATH A", "RES <- PATH A B", "PRINT", "PRINT GRAPH"
  ]
  for cmd in bad_cmds:
    log(cmd)
    expect("Syntactic Error", cmd, "SYNTAX ERROR", "0")

  # Semantic
  log(f"LOAD GRAPH {oracle_d.name} D") # Valid Load for setup
  log(f"LOAD GRAPH {oracle_u.name} U") # Valid Load for setup

  sem_cmds = [
    f"LOAD GRAPH {oracle_d.name} D", # Already exists
    "LOAD GRAPH FAKE D",             # File missing
    "EXPORT GRAPH FAKE",             # Graph missing
    "DEGREE FAKE 0",
    "DEGREE BIG -1",                 # Invalid Int (Syntactic/Semantic edge)
    "RES <- PATH FAKE 0 1",
    "PRINT GRAPH FAKE"
  ]
  for cmd in sem_cmds:
    log(cmd)
    expect("Semantic Error", cmd, "SEMANTIC ERROR or Error", "0")

  # --- 2. FUNCTIONAL TESTS ---
  ans_file.write("\n=== SECTION 2: FUNCTIONAL TESTS ===\n")

  # DEGREE (Directed)
  d_blocks = math.ceil(len(oracle_d.edge_attrs) / EDGES_PER_BLOCK)

  for i in range(15):
    node = random.randint(0, len(oracle_d.nodes)-1)
    cmd = f"DEGREE {oracle_d.name} {node}"
    ans = oracle_d.get_degree(node)

    math_str = f"2 + {d_blocks} (Index Read + Target Block + Full Scan for In-Degree)"

    log(cmd)
    expect(f"Degree Directed Node {node}", cmd, ans, math_str)

  # DEGREE (Undirected)
  u_blocks = math.ceil(len(oracle_u.edge_attrs) / EDGES_PER_BLOCK)

  for i in range(15):
    node = random.randint(0, len(oracle_u.nodes)-1)
    cmd = f"DEGREE {oracle_u.name} {node}"
    ans = oracle_u.get_degree(node)
    log(cmd)
    expect(f"Degree Undirected Node {node}", cmd, ans, "2 (Index + Data Block jump)")

  # --- 3. PATH TESTS (Varied WHERE clauses) ---
  ans_file.write("\n=== SECTION 3: PATH TESTS (Varied WHERE clauses) ===\n")

  # helper to build condition objects and SQL strings
  def cond_attr_node(idx, eq=None):
    # idx is 0-based attr index; maps to A1..An
    if eq is None:
      return ({'target':'N','type':'ATTR_UNIFORM','attr_index':idx,'value':None}, f"A{idx+1}(N)")
    else:
      return ({'target':'N','type':'ATTR_EQ','attr_index':idx,'value':eq}, f"A{idx+1}(N) == {eq}")

  def cond_attr_edge(idx, eq=None):
    if eq is None:
      return ({'target':'E','type':'ATTR_UNIFORM','attr_index':idx,'value':None}, f"B{idx+1}(E)")
    else:
      return ({'target':'E','type':'ATTR_EQ','attr_index':idx,'value':eq}, f"B{idx+1}(E) == {eq}")

  def cond_any_node(eq=None):
    if eq is None:
      return ({'target':'N','type':'ANY_UNIFORM','attr_index':None,'value':None}, "ANY(N)")
    else:
      return ({'target':'N','type':'ANY_EQ','attr_index':None,'value':eq}, f"ANY(N) == {eq}")

  def cond_any_edge(eq=None):
    if eq is None:
      return ({'target':'E','type':'ANY_UNIFORM','attr_index':None,'value':None}, "ANY(E)")
    else:
      return ({'target':'E','type':'ANY_EQ','attr_index':None,'value':eq}, f"ANY(E) == {eq}")

  # We'll create a variety of WHERE clauses:
  where_templates = []

  # Single node equality conditions
  where_templates.append([cond_attr_node(0, 1)])  # A1(N) == 1
  where_templates.append([cond_attr_node(1, 1)])  # A2(N) == 1

  # Single edge attr uniform (no ==) and equality
  where_templates.append([cond_attr_edge(1)])     # B2(E)
  where_templates.append([cond_attr_edge(0, 1)])  # B1(E) == 1

  # Combined node and edge constraints
  where_templates.append([cond_attr_node(0,1), cond_attr_edge(1)])  # A1(N)==1 AND B2(E)
  where_templates.append([cond_attr_node(1), cond_any_edge(1)])      # A2(N) AND ANY(E) == 1

  # ANY node / ANY edge tests
  where_templates.append([cond_any_node(1)])   # ANY(N) == 1
  where_templates.append([cond_any_edge()])    # ANY(E)

  # Two node-uniform attrs
  where_templates.append([cond_attr_node(0), cond_attr_node(1)])  # A1(N) AND A2(N) uniform

  # empty WHERE (no conditions)
  where_templates.append([])

  # generate tests
  # Use node range based on actual oracle size (fixes earlier bug)
  def pick_nodes(oracle):
    n = len(oracle.nodes)
    return random.randint(0, n-1), random.randint(0, n-1)

  # create 30 varied path tests for directed graph
  for i in range(30):
    u, v = pick_nodes(oracle_d)
    # choose a where template
    templ = random.choice(where_templates)
    cond_objs = [t[0] for t in templ]
    where_str_parts = [t[1] for t in templ]
    where_clause = ""
    if where_str_parts:
      where_clause = " WHERE " + " AND ".join(where_str_parts)

    cmd = f"RES <- PATH {oracle_d.name} {u} {v}{where_clause}"
    # The oracle now understands condition objects (list)
    ans = oracle_d.get_shortest_path(u, v, cond_objs)
    # Block math approximate:
    math_str = "Depends on Path Length (Approx: Visited_Nodes * 2)"
    log(cmd)
    expect(f"Path {u}->{v} with conditions {where_str_parts}", cmd, ans, math_str)

  # create 30 varied path tests for undirected graph (TEST_U)
  for i in range(30):
    u, v = pick_nodes(oracle_u)
    templ = random.choice(where_templates)
    cond_objs = [t[0] for t in templ]
    where_str_parts = [t[1] for t in templ]
    where_clause = ""
    if where_str_parts:
      where_clause = " WHERE " + " AND ".join(where_str_parts)

    cmd = f"RES <- PATH {oracle_u.name} {u} {v}{where_clause}"
    ans = oracle_u.get_shortest_path(u, v, cond_objs)
    math_str = "Depends on Path Length (Approx: Visited_Nodes * 2)"
    log(cmd)
    expect(f"Path {u}->{v} (undirected) with conditions {where_str_parts}", cmd, ans, math_str)

  # create a few path tests that are expected semantic/node errors
  # - Graph that doesn't exist
  cmd = f"RES <- PATH NO_GRAPH 0 1"
  log(cmd)
  expect("Path Nonexistent Graph", cmd, "SEMANTIC ERROR: Graph doesn't exist", "0")

  # - Node out of range
  bad_node = len(oracle_d.nodes) + 100
  cmd = f"RES <- PATH {oracle_d.name} {bad_node} 1"
  log(cmd)
  expect("Path Node Does Not Exist", cmd, "Node does not exist", "0")

  # --- EXPORT & PRINT ---
  log(f"EXPORT GRAPH {oracle_d.name}")
  expect("Export Directed", f"EXPORT GRAPH {oracle_d.name}", "Success", f"{d_blocks} (Sequential Scan)")

  log(f"PRINT GRAPH {oracle_d.name}")
  expect("Print Graph", f"PRINT GRAPH {oracle_d.name}", "Dump of Nodes & Edges", f"{d_blocks} (Seq Scan)")

  # --- 4. STRESS TESTS ---
  ans_file.write("\n=== SECTION 4: STRESS TESTS (High Volume) ===\n")

  stress_node_d = len(oracle_d.nodes) - 5
  log(f"DEGREE {oracle_d.name} {stress_node_d}")
  expect("Stress Degree Directed", f"DEGREE {oracle_d.name} {stress_node_d}", oracle_d.get_degree(stress_node_d), f"2 + {d_blocks}")

  stress_node_u = len(oracle_u.nodes) - 5
  log(f"DEGREE {oracle_u.name} {stress_node_u}")
  expect("Stress Degree Undirected", f"DEGREE {oracle_u.name} {stress_node_u}", oracle_u.get_degree(stress_node_u), "2 (Index + Data Block jump)")

  print("Test Generation Complete.")
  print("1. Run './server' inside src/")
  print("2. Paste content of 'test_suite.sql' into server")
  print("3. Compare output with 'test_expected_answers.txt'")

  sql_file.close()
  ans_file.close()

if __name__ == "__main__":
  # Generate small graph for functional testing (you can crank these for stress)
  print("Generating Graph 'TEST_D'...")
  oracle_d = generate_data("TEST_D", 10000, 250000, True)

  print("Generating Graph 'TEST_U'...")
  oracle_u = generate_data("TEST_U", 10000, 250000, False)

  write_test_suite(oracle_d, oracle_u)
