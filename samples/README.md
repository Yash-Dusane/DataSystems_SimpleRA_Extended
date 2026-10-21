# Sample Inputs + Expected Outputs

This folder contains ready-to-run `.ra` scripts for **Phase 1** and **Phase 2**, along with the expected outputs.

## How to run

Build:
`cd src && make clean && make`

Run a script:
`cd src && ./server < ../samples/phase2_basic.ra`

## Output matching

The server prints interactive prompts (`>`) and blank lines. The `*.expected.txt` files here store a **filtered** form:
- prompts removed
- blank lines removed

To generate filtered output for comparison:
`cd src && ./server < ../samples/phase2_basic.ra | python3 ../samples/filter_output.py`

