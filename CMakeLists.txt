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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.yy
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.yy
  COMMENT "Generating main parser from parser.yy"
)

# --- Main lexer (flex) ---
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lexer.cc
         ${CMAKE_CURRENT_BINARY_DIR}/lexer.hh
  COMMAND ${FLEX_EXECUTABLE}
    --header-file=${CMAKE_CURRENT_BINARY_DIR}/lexer.hh
    -o ${CMAKE_CURRENT_BINARY_DIR}/lexer.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lexer.ll
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/lexer.ll
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/annot_parser.yy
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/annot_parser.yy
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/annot_lexer.ll
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/annot_lexer.ll
          ${CMAKE_CURRENT_BINARY_DIR}/annot_parser.hh
  COMMENT "Generating annotation lexer from annot_lexer.ll"
)

# --- Executable ---
add_executable(ebnf2tikz
  src/main.cc
  src/driver.cc
  src/util.cc
  src/nodesizes.cc
  src/diagnostics.cc
  src/ast.cc
  src/ast_dump.cc
  src/resolver.cc
  src/ast_optimizer.cc
  src/ast_layout.cc
  src/ast_tikzwriter.cc
  ${CMAKE_CURRENT_BINARY_DIR}/parser.cc
  ${CMAKE_CURRENT_BINARY_DIR}/lexer.cc
  ${CMAKE_CURRENT_BINARY_DIR}/annot_parser.cc
  ${CMAKE_CURRENT_BINARY_DIR}/annot_lexer.cc
)

target_include_directories(ebnf2tikz PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_options(ebnf2tikz PRIVATE -Wall -g)

# --- LaTeX package extraction ---
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/LaTeX/ebnf2tikz.sty
  COMMAND latex ebnf2tikz.ins
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/LaTeX/ebnf2tikz.dtx
          ${CMAKE_CURRENT_SOURCE_DIR}/LaTeX/ebnf2tikz.ins
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/LaTeX
  COMMENT "Extracting ebnf2tikz.sty from ebnf2tikz.dtx"
)
add_custom_target(sty DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/LaTeX/ebnf2tikz.sty)

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
  DEPENDS ebnf2tikz sty
  COMMENT "Building TikZ PDF regression test"
)

add_custom_target(check-sty
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/LaTeX/tests/run_tests.sh
    ${CMAKE_CURRENT_BINARY_DIR}/ebnf2tikz
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/LaTeX/tests
  DEPENDS ebnf2tikz sty
  COMMENT "Running LaTeX package tests"
)

add_custom_target(all_targets ALL DEPENDS ebnf2tikz check-tikz)

# --- Documentation target ---
find_package(Doxygen)
if(DOXYGEN_FOUND)
  find_package(LATEX COMPONENTS PDFLATEX MAKEINDEX)
  add_custom_target(doc
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
    COMMAND ${CMAKE_MAKE_PROGRAM} -C ${CMAKE_CURRENT_SOURCE_DIR}/documentation/latex
    COMMAND ${CMAKE_COMMAND} -E copy
      ${CMAKE_CURRENT_SOURCE_DIR}/documentation/latex/refman.pdf
      ${CMAKE_CURRENT_SOURCE_DIR}/documentation/ebnf2tikz.pdf
    COMMAND ${CMAKE_COMMAND} -E rm -rf
      ${CMAKE_CURRENT_SOURCE_DIR}/documentation/latex
    COMMAND ${CMAKE_COMMAND}
      -DOUTFILE=${CMAKE_CURRENT_SOURCE_DIR}/documentation/index.html
      -P ${CMAKE_CURRENT_SOURCE_DIR}/write_index_redirect.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating API documentation in documentation/"
  )
endif()

# --- Augment built-in 'clean' target ---
# CMake's clean removes build products (binary, .o in CMakeFiles/, generated
# flex/bison files).  Add stale top-level .o files from the old Makefile build.
file(GLOB _stale_objects "${CMAKE_CURRENT_SOURCE_DIR}/*.o")
set_directory_properties(PROPERTIES ADDITIONAL_CLEAN_FILES "${_stale_objects}")

# --- realclean target ---
add_custom_target(realclean
  COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target clean
  # Stale object files and bison/flex generated files in project root
  COMMAND bash -c "rm -f *.o parser.cc parser.hh location.hh lexer.cc lexer.hh annot_parser.cc annot_parser.hh annot_location.hh annot_lexer.cc annot_lexer.hh"
  # Unit test artifacts
  COMMAND bash -c "rm -f unit_tests/*.pdf unit_tests/*.aux unit_tests/*.log unit_tests/*.out unit_tests/*.toc unit_tests/*.synctex.gz unit_tests/bnfnodes.dat unit_tests/all_tests.tex unit_tests/all_input.ebnf unit_tests/testlist.tex"
  # Top-level generated test files
  COMMAND ${CMAKE_COMMAND} -E rm -f all_input.ebnf all_tests.tex
  # Doxygen output
  COMMAND ${CMAKE_COMMAND} -E rm -rf documentation/html documentation/latex
  COMMAND ${CMAKE_COMMAND} -E rm -f documentation/ebnf2tikz.pdf documentation/index.html
  # CMake-generated Doxygen config
  COMMAND ${CMAKE_COMMAND} -E rm -f CMakeDoxyfile.in CMakeDoxygenDefaults.cmake
  # LaTeX package build artifacts (keep .dtx, .ins, and tests/)
  COMMAND bash -c "rm -f LaTeX/ebnf2tikz.sty LaTeX/*.aux LaTeX/*.glo LaTeX/*.hd LaTeX/*.idx LaTeX/*.log LaTeX/*.out LaTeX/*.pdf LaTeX/*.toc"
  # LaTeX test artifacts
  COMMAND bash -c "rm -f LaTeX/tests/*.pdf LaTeX/tests/*.aux LaTeX/tests/*.log LaTeX/tests/*.out LaTeX/tests/bnfnodes.dat LaTeX/tests/*-diagrams.tex LaTeX/tests/*_wrapper.tex"
  # Manual build artifacts (keep manual.tex)
  COMMAND bash -c "rm -f manual/*.aux manual/*.log manual/*.out manual/*.toc manual/*.pdf"
  # CMake products
  COMMAND ${CMAKE_COMMAND} -E rm -f
    CMakeCache.txt cmake_install.cmake Makefile .depend
  COMMAND ${CMAKE_COMMAND} -E rm -rf CMakeFiles
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Removing all build artifacts, PDFs, LaTeX intermediates, and CMake products"
)
