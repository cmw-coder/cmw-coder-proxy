include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake)
CPMAddPackage(
        NAME cpp-tree-sitter
        GIT_REPOSITORY https://github.com/nsumner/cpp-tree-sitter.git
        GIT_TAG v0.0.3
)
add_grammar_from_repo(tree-sitter-c
        https://github.com/tree-sitter/tree-sitter-c.git
        0.20.7
)
