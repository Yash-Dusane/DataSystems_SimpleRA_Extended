import sys
import random
import os

def generate_graph(name, num_nodes, num_edges, is_directed):
    # Setup filenames
    suffix = "D" if is_directed else "U"
    node_file = f"data/{name}_Nodes_{suffix}.csv"
    edge_file = f"data/{name}_Edges_{suffix}.csv"
    
    os.makedirs("data", exist_ok=True)

    print(f"Generating {name} ({num_nodes} nodes, {num_edges} edges)...")

    # 1. Generate Nodes
    # Format: NodeID, A1, A2
    with open(node_file, "w") as f:
        f.write("NodeID,A1,A2\n") # Header
        for i in range(num_nodes):
            # 50% chance for attributes to be 0 or 1
            a1 = random.randint(0, 1)
            a2 = random.randint(0, 1)
            f.write(f"{i},{a1},{a2}\n")

    # 2. Generate Edges
    # Format: Src, Dest, Weight, B1, B2
    with open(edge_file, "w") as f:
        f.write("Src_NodeID,Dest_NodeID,Weight,B1,B2\n") # Header
        
        generated_edges = set()
        count = 0
        
        while count < num_edges:
            src = random.randint(0, num_nodes - 1)
            dest = random.randint(0, num_nodes - 1)
            
            if src == dest: continue # No self-loops for simplicity
            
            # For undirected, store canonical form to avoid dupes in generation
            edge_key = (src, dest) if is_directed else tuple(sorted((src, dest)))
            
            if edge_key not in generated_edges:
                generated_edges.add(edge_key)
                w = random.randint(1, 100)
                b1 = random.randint(0, 1)
                b2 = random.randint(0, 1)
                f.write(f"{src},{dest},{w},{b1},{b2}\n")
                count += 1
                
                if count % 100000 == 0:
                    print(f"  Generated {count} edges...")

    print(f"Done! Files created:\n  - {node_file}\n  - {edge_file}")

if __name__ == "__main__":
    if len(sys.argv) < 5:
        print("Usage: python3 generate_graph.py <NAME> <NODES> <EDGES> <1=Direct/0=Undirect>")
        print("Example: python3 generate_graph.py BIG 10000 100000 1")
        sys.exit(1)
    
    name = sys.argv[1]
    nodes = int(sys.argv[2])
    edges = int(sys.argv[3])
    directed = bool(int(sys.argv[4]))
    
    generate_graph(name, nodes, edges, directed)