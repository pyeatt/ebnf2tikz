cmake_minimum_required(VERSION 3.16)
project(ebnf2tikz CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(BISON 3.7 REQUIRED)
find_package(FLEX REQUIRED)

# --- Main parser (bison) ---
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/parser.cc
         ${CMAKE_CURRENT_BINARY_DIR}/parser.hh
         ${CMAKE_CURRENT_BINARY_DIR}/location.hh
  COMMAND ${BISON_EXECUTABLE} -d --locations
    -o ${CMAKE_CURRENT_BINARY_DIR}/parser.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/parser.yy
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/parser.yy
  COMMENT "Generating main parser from parser.yy"
)

# --- Main lexer (flex) ---
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lexer.cc
         ${CMAKE_CURRENT_BINARY_DIR}/lexer.hh
  COMMAND ${FLEX_EXECUTABLE}
    --header-file=${CMAKE_CURRENT_BINARY_DIR}/lexer.hh
    -o ${CMAKE_CURRENT_BINARY_DIR}/lexer.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/lexer.ll
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/lexer.ll
          ${CMAKE_CURRENT_BINARY_DIR}/parser.hh
  COMMENT "Generating main lexer from lexer.ll"
)

# --- Annotation parser (bison) ---
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/annot_parser.cc
         ${CMAKE_CURRENT_BINARY_DIR}/annot_parser.hh
         ${CMAKE_CURRENT_BINARY_DIR}/annot_location.hh
  COMMAND ${BISON_EXECUTABLE} -d --locations
    -o ${CMAKE_CURRENT_BINARY_DIR}/annot_parser.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/annot_parser.yy
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/annot_parser.yy
  COMMENT "Generating annotation parser from annot_parser.yy"
)

# --- Annotation lexer (flex with -P annot prefix) ---
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/annot_lexer.cc
         ${CMAKE_CURRENT_BINARY_DIR}/annot_lexer.hh
  COMMAND ${FLEX_EXECUTABLE}
    --header-file=${CMAKE_CURRENT_BINARY_DIR}/annot_lexer.hh
    -P annot
    -o ${CMAKE_CURRENT_BINARY_DIR}/annot_lexer.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/annot_lexer.ll
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/annot_lexer.ll
          ${CMAKE_CURRENT_BINARY_DIR}/annot_parser.hh
  COMMENT "Generating annotation lexer from annot_lexer.ll"
)

# --- Executable ---
add_executable(ebnf2tikz
  main.cc
  driver.cc
  util.cc
  nodesizes.cc
  diagnostics.cc
  ast.cc
  ast_dump.cc
  resolver.cc
  ast_optimizer.cc
  ast_layout.cc
  ast_tikzwriter.cc
  ${CMAKE_CURRENT_BINARY_DIR}/parser.cc
  ${CMAKE_CURRENT_BINARY_DIR}/lexer.cc
  ${CMAKE_CURRENT_BINARY_DIR}/annot_parser.cc
  ${CMAKE_CURRENT_BINARY_DIR}/annot_lexer.cc
)

target_include_directories(ebnf2tikz PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_options(ebnf2tikz PRIVATE -Wall -g)

# --- Test targets ---
add_custom_target(check
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/run_tests.sh
    ${CMAKE_CURRENT_BINARY_DIR}/ebnf2tikz
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/unit_tests
  DEPENDS ebnf2tikz
  COMMENT "Running AST-dump regression tests"
)

add_custom_target(check-tikz
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/unit_tests/build_tikz_tests.sh
    ${CMAKE_CURRENT_BINARY_DIR}/ebnf2tikz
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/unit_tests
  DEPENDS ebnf2tikz
  COMMENT "Building TikZ PDF regression test"
)

add_custom_target(all_targets ALL DEPENDS ebnf2tikz check-tikz)

# --- Augment built-in 'clean' target ---
# CMake's clean removes build products (binary, .o in CMakeFiles/, generated
# flex/bison files).  Add stale top-level .o files from the old Makefile build.
file(GLOB _stale_objects "${CMAKE_CURRENT_SOURCE_DIR}/*.o")
set_directory_properties(PROPERTIES ADDITIONAL_CLEAN_FILES "${_stale_objects}")

# --- realclean target ---
add_custom_target(realclean
  COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target clean
  COMMAND bash -c "rm -f *.o"
  COMMAND bash -c "rm -f unit_tests/*.pdf unit_tests/*.aux unit_tests/*.log unit_tests/*.out unit_tests/*.toc unit_tests/*.synctex.gz unit_tests/bnfnodes.dat unit_tests/all_tests.tex unit_tests/all_input.ebnf unit_tests/testlist.tex"
  COMMAND ${CMAKE_COMMAND} -E rm -f
    CMakeCache.txt cmake_install.cmake Makefile .depend
  COMMAND ${CMAKE_COMMAND} -E rm -rf CMakeFiles
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Removing all build artifacts, PDFs, LaTeX intermediates, and CMake products"
)
