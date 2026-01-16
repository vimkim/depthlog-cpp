build:
    cmake -S example -B build-example
    cmake --build build-example
    ln -sf build-example/compile_commands.json compile_commands.json

run:
    build-example/example_proj

build-run: build run

depthlog-tree:
    python3 depthlog_tree.py app.log
    python3 depthlog_tree.py app.log --show-msg
    python3 depthlog_tree.py app.log --only-tid 3547698
