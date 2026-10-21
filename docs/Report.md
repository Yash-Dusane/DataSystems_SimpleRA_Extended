# SimpleRA Phase 1 - Report


## Team 19 :

- <span style="color:#f5f5f5">Harpreet Singh - 2022101048</span>
- <span style="color:white">Kunal Angadi - 2022111005</span>
- <span style="color:black">Yash Nitin Dusane - 2022102078</span>

------------

## Overview
- SimpleRA Phase1 extends initial SimpleRA to add graph storage, graph catalog management, and graph-specific queries. 
- The new implementation supports directed/undirected graphs, graph load/print/export commands, degree computation, and a path operator with optional node/edge attribute filters.

#### Net Effect:
Phase1 introduces a complete graph subsystem layered on the existing table engine:
- Adds graph-aware parsing, semantics, and execution paths.
- Introduces a dedicated graph catalog and graph data model.
- Adds graph-specific commands and algorithms (load/export/print/degree/path).
- Integrates graph tables into the existing table catalog for unified storage and buffer management.

--------------------------

## Detailed Implementation per Command (logic, page design, block access, error handling)

### LOAD GRAPH
**Logic** (see [../src/executors/load_graph.cpp](../src/executors/load_graph.cpp)):
- Parse `LOAD GRAPH <name> <D/U>`, set `parsedQuery.graphName` and `parsedQuery.isDirected`.
- Validate node and edge files exist (`<name>_Nodes_D/U.csv`, `<name>_Edges_D/U.csv`).
- Create a `Graph`, load node table first, then create a temp edge file.
- For undirected graphs, duplicate edges in reverse direction and write to temp file; for directed graphs, copy as-is.
- Sort edges by source node using shell `sort` to make source-based scans efficient.
- Load edge table, register both tables in `tableCatalogue`, insert graph into `graphCatalogue`.

**Page design**:
- Nodes and edges are stored as standard `Table` pages, each page containing multiple rows with the first column acting as the sort key.
- Edge table is sorted by source node to support block-level range scans in `PATH` and `DEGREE`.

**Block access**:
- Resets `GLOBAL_BLOCK_ACCESS_COUNT` before loading.
- Sorting and table loading are the dominant block operations; sorted order reduces subsequent accesses during graph queries.

**Error handling**:
- Syntax error if command format or `D/U` flag is invalid.
- Semantic error if the graph already exists or the required CSV files are missing.
- Load failures return with descriptive errors for nodes or edges.

### EXPORT GRAPH
**Logic** (see [../src/executors/export_graph.cpp](../src/executors/export_graph.cpp)):
- Validate graph exists, fetch it from `graphCatalogue`.
- Export node table directly.
- For directed graphs, export edges as-is; for undirected graphs, output only rows with `src <= dst` to avoid duplicates.

**Page design**:
- Uses existing table pages and a `Cursor` scan to iterate rows. No change to page layout.

**Block access**:
- `GLOBAL_BLOCK_ACCESS_COUNT` is reset and reported after export.
- Sequential cursor scan ensures mostly sequential page reads.

**Error handling**:
- Semantic error if graph is absent.

### PRINT GRAPH
**Logic** (see [../src/executors/print_graph.cpp](../src/executors/print_graph.cpp)):
- Validate graph exists.
- Print node count, edge count, and `D/U` flag.
- Print node rows and edge rows up to `PRINT_COUNT`.

**Page design**:
- Relies on `Cursor` traversal of table pages. No special page modifications.

**Block access**:
- Resets and reports `GLOBAL_BLOCK_ACCESS_COUNT` for the print operation.

**Error handling**:
- Semantic error if graph is absent.

### DEGREE
**Logic** (see [../src/executors/degree.cpp](../src/executors/degree.cpp)):
- Validate graph exists and node id is numeric.
- Locate node existence using a binary-search over pages (by first row key) and a local scan within the block.
- For undirected graphs: count edges where `src == node` (edges duplicated at load time, so this yields total degree).
- For directed graphs: count out-degree via source-sorted edge scan plus in-degree via full edge scan.

**Page design**:
- Edge pages are sorted by source id, so out-degree counting can start near the source block and stop early when `src` exceeds the target.

**Block access**:
- Uses `Page` access to find the starting block, then sequential scan from that block.
- Directed in-degree requires a full scan of the edge table.

**Error handling**:
- Syntax error on malformed input.
- Semantic error if graph is absent.
- Prints “Node does not exist” if the node id isn’t present in node table.

### PATH
**Logic** (see [../src/executors/path.cpp](../src/executors/path.cpp)):
- Parse `PATH` with optional `WHERE` constraints on node or edge attributes (node/edge selector via `(N)`/`(E)` markers).
- Expand constraints into concrete filters, then for each configuration:
	- Filter nodes by attribute conditions to produce a valid set.
	- Run Dijkstra-style traversal using the edge table, scanning only edges with `src == current` using the sorted edge pages.
- If a path is found, create a result graph containing only path nodes and edges and load it as a new graph.

**Page design**:
- The edge table is sorted by source, enabling efficient block-local scans per source node.
- Node and edge attributes are stored as row columns and checked via column indices.

**Block access**:
- Resets and reports `GLOBAL_BLOCK_ACCESS_COUNT`.
- Efficient access comes from block-local scans using `getStartBlock` on the sorted edge table.
- Additional scans are needed to validate node existence and to output the resulting subgraph.

**Error handling**:
- Syntax errors for malformed PATH grammar.
- Semantic error if graph is absent.
- Prints “Node does not exist” if source or destination node is missing.

---

## Page Design Justification 
**Theoretical basis**: A page (block) is the fundamental unit of disk I/O. In a row‑oriented DBMS, each page stores a fixed number of tuples; operations are evaluated by reading/writing pages, not individual rows. A buffer manager brings pages into memory, and cursors iterate over rows within a page. This model supports sequential scans, block‑level locality, and predictable I/O costs.

**Initial page design**:
- Tables are **row‑oriented** and **page‑based**: each table is partitioned into fixed‑size pages based on `BLOCK_SIZE` and `maxRowsPerBlock`.
- Each page is a separate file under `../data/temp/` named `<Table>_Page<index>`; pages are loaded into memory by the `Page` class and scanned via `Cursor`.
- The page stores `rowCount` rows as a `vector<vector<int>>`, providing simple, direct row access within a block.
- This design aims to store tuples contiguously per page and scan pages sequentially.

**Graph extension design**:
- The graph is represented using the same **two‑table** model: a node table and an edge table, each following the original page layout.
- The edge table is **sorted by source node** after loading, which optimizes adjacency‑style access while still using the existing page abstraction.

**Why the combined design is appropriate and efficient**:
- **Compatibility with the engine**: Reusing the original page format avoids any changes to buffer management, serialization, or cursor logic.
- **Uniform I/O accounting**: Graph operations report block accesses using the same page‑based cost model as relational queries.
- **Efficient adjacency scans**: Sorting edges by source means `PATH` and `DEGREE` can read only the relevant blocks and stop early, reducing I/O.
- **Scalability**: Page‑based scans allow large graphs to be processed incrementally without loading all data into memory.
- **Simplicity**: The row layout (id, src, dst, weight, attributes) keeps filtering logic identical to table predicates.
- **Undirected support without extra indexes**: Edge duplication for undirected graphs preserves symmetric traversal using the same source‑sorted access pattern.

Overall, the page design stays faithful to the original theoretical model (row‑oriented pages as the unit of I/O) while making a minimal, performance‑motivated extension (source‑sorted edges) that directly benefits graph algorithms.

---

## Assumptions

- Edge weights are stored as a column at index 2 for PATH and are integers.
- For undirected graphs, duplicating edges in reverse is acceptable and intended to make traversal symmetric.
- `PRINT_COUNT` may be large to allow full graph printing for debugging.
