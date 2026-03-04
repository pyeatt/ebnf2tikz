cmake_minimum_required(VERSION 3.16)
project(ebnf2tikz CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(BISON 3.7 REQUIRED)
find_package(FLEX REQUIRED)

set(GENDIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(RUN_QUIET bash ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_quiet.sh)

# --- Main parser (bison) ---
add_custom_command(
  OUTPUT ${GENDIR}/parser.cc
         ${GENDIR}/parser.hh
         ${GENDIR}/location.hh
  COMMAND ${BISON_EXECUTABLE} -d --locations
    -o ${GENDIR}/parser.cc
    ${GENDIR}/parser.yy
  DEPENDS ${GENDIR}/parser.yy
  COMMENT "Generating main parser from parser.yy"
)

# --- Main lexer (flex) ---
add_custom_command(
  OUTPUT ${GENDIR}/lexer.cc
         ${GENDIR}/lexer.hh
  COMMAND ${FLEX_EXECUTABLE}
    --header-file=${GENDIR}/lexer.hh
    -o ${GENDIR}/lexer.cc
    ${GENDIR}/lexer.ll
  DEPENDS ${GENDIR}/lexer.ll
          ${GENDIR}/parser.hh
  COMMENT "Generating main lexer from lexer.ll"
)

# --- Annotation parser (bison) ---
add_custom_command(
  OUTPUT ${GENDIR}/annot_parser.cc
         ${GENDIR}/annot_parser.hh
         ${GENDIR}/annot_location.hh
  COMMAND ${BISON_EXECUTABLE} -d --locations
    -o ${GENDIR}/annot_parser.cc
    ${GENDIR}/annot_parser.yy
  DEPENDS ${GENDIR}/annot_parser.yy
  COMMENT "Generating annotation parser from annot_parser.yy"
)

# --- Annotation lexer (flex with -P annot prefix) ---
add_custom_command(
  OUTPUT ${GENDIR}/annot_lexer.cc
         ${GENDIR}/annot_lexer.hh
  COMMAND ${FLEX_EXECUTABLE}
    --header-file=${GENDIR}/annot_lexer.hh
    -P annot
    -o ${GENDIR}/annot_lexer.cc
    ${GENDIR}/annot_lexer.ll
  DEPENDS ${GENDIR}/annot_lexer.ll
          ${GENDIR}/annot_parser.hh
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
  ${GENDIR}/parser.cc
  ${GENDIR}/lexer.cc
  ${GENDIR}/annot_parser.cc
  ${GENDIR}/annot_lexer.cc
)

target_include_directories(ebnf2tikz PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_options(ebnf2tikz PRIVATE -Wall -Wextra -g)

# --- LaTeX package extraction ---
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/ebnf2tikz.sty
  COMMAND ${RUN_QUIET} latex ebnf2tikz.ins
  COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_SOURCE_DIR}/LaTeX/ebnf2tikz.sty
    ${CMAKE_CURRENT_SOURCE_DIR}/ebnf2tikz.sty
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/LaTeX/ebnf2tikz.dtx
          ${CMAKE_CURRENT_SOURCE_DIR}/LaTeX/ebnf2tikz.ins
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/LaTeX
  COMMENT "Extracting ebnf2tikz.sty from ebnf2tikz.dtx"
)
add_custom_target(sty DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ebnf2tikz.sty)

# --- LaTeX package documentation ---
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/ebnf2tikz.pdf
  COMMAND ${RUN_QUIET} pdflatex ebnf2tikz.dtx
  COMMAND ${RUN_QUIET} pdflatex ebnf2tikz.dtx
  COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_SOURCE_DIR}/LaTeX/ebnf2tikz.pdf
    ${CMAKE_CURRENT_SOURCE_DIR}/ebnf2tikz.pdf
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/LaTeX/ebnf2tikz.dtx
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/LaTeX
  COMMENT "Building ebnf2tikz package documentation"
)
add_custom_target(sty-doc DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ebnf2tikz.pdf)

# --- User manual ---
# The manual uses \ebnffile which invokes ebnf2tikz via shell-escape.
# We put the build directory on PATH so pdflatex can find the binary,
# and set TEXINPUTS so it can find ebnf2tikz.sty.
set(MANUAL_ENV ${CMAKE_COMMAND} -E env
  "PATH=${CMAKE_CURRENT_BINARY_DIR}:$ENV{PATH}"
  "TEXINPUTS=${CMAKE_CURRENT_SOURCE_DIR}/LaTeX::")
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/manual.pdf
  # Clean stale artifacts to prevent \newsavebox redefinition errors
  COMMAND ${CMAKE_COMMAND} -E rm -f
    manual.aux manual.toc manual.out manual.log
    manual-rail.ebnf manual-rail.tex bnfnodes.dat
  # Three passes: pdflatex runs ebnf2tikz via shell-escape, measures
  # nodes, and feeds sizes back through bnfnodes.dat
  COMMAND ${MANUAL_ENV} ${RUN_QUIET} pdflatex -shell-escape -interaction=nonstopmode manual
  COMMAND ${MANUAL_ENV} ${RUN_QUIET} pdflatex -shell-escape -interaction=nonstopmode manual
  COMMAND ${MANUAL_ENV} ${RUN_QUIET} pdflatex -shell-escape -interaction=nonstopmode manual
  COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_SOURCE_DIR}/manual/manual.pdf
    ${CMAKE_CURRENT_SOURCE_DIR}/manual.pdf
  DEPENDS ebnf2tikz
          ${CMAKE_CURRENT_SOURCE_DIR}/ebnf2tikz.sty
          ${CMAKE_CURRENT_SOURCE_DIR}/manual/manual.tex
          ${CMAKE_CURRENT_SOURCE_DIR}/manual/manual-examples.ebnf
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/manual
  COMMENT "Building user manual"
)
add_custom_target(manual DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/manual.pdf)

# --- Test targets ---
add_custom_target(check
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/unit_tests/run_tests.sh
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

add_custom_target(all_targets ALL DEPENDS ebnf2tikz check-tikz sty-doc manual)

# --- Documentation target ---
find_package(Doxygen)
if(DOXYGEN_FOUND)
  find_package(LATEX COMPONENTS PDFLATEX MAKEINDEX)
  add_custom_target(doc
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/documentation/Doxyfile
    COMMAND ${RUN_QUIET} ${CMAKE_MAKE_PROGRAM} -C ${CMAKE_CURRENT_SOURCE_DIR}/documentation/latex
    COMMAND ${CMAKE_COMMAND} -E copy
      ${CMAKE_CURRENT_SOURCE_DIR}/documentation/latex/refman.pdf
      ${CMAKE_CURRENT_SOURCE_DIR}/documentation/ebnf2tikz.pdf
    COMMAND ${CMAKE_COMMAND} -E rm -rf
      ${CMAKE_CURRENT_SOURCE_DIR}/documentation/latex
    COMMAND ${CMAKE_COMMAND}
      -DOUTFILE=${CMAKE_CURRENT_SOURCE_DIR}/documentation/index.html
      -P ${CMAKE_CURRENT_SOURCE_DIR}/documentation/write_index_redirect.cmake
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
  # Stale object files
  COMMAND bash -c "rm -f *.o"
  # Bison/Flex generated files in src/
  COMMAND bash -c "rm -f src/parser.cc src/parser.hh src/location.hh src/lexer.cc src/lexer.hh src/annot_parser.cc src/annot_parser.hh src/annot_location.hh src/annot_lexer.cc src/annot_lexer.hh"
  # Unit test artifacts
  COMMAND bash -c "rm -f unit_tests/*.pdf unit_tests/*.aux unit_tests/*.log unit_tests/*.out unit_tests/*.toc unit_tests/*.synctex.gz unit_tests/bnfnodes.dat unit_tests/all_tests.tex unit_tests/all_input.ebnf unit_tests/testlist.tex"
  # Top-level generated files
  COMMAND ${CMAKE_COMMAND} -E rm -f all_input.ebnf all_tests.tex ebnf2tikz.sty ebnf2tikz.pdf manual.pdf
  # Doxygen output
  COMMAND ${CMAKE_COMMAND} -E rm -rf documentation/html documentation/latex
  COMMAND ${CMAKE_COMMAND} -E rm -f documentation/ebnf2tikz.pdf documentation/index.html
  # CMake-generated Doxygen config
  COMMAND ${CMAKE_COMMAND} -E rm -f CMakeDoxyfile.in CMakeDoxygenDefaults.cmake
  # LaTeX package build artifacts (keep .dtx, .ins, and tests/)
  COMMAND bash -c "rm -f LaTeX/ebnf2tikz.sty LaTeX/*.aux LaTeX/*.glo LaTeX/*.hd LaTeX/*.idx LaTeX/*.log LaTeX/*.out LaTeX/*.pdf LaTeX/*.toc"
  # LaTeX test artifacts
  COMMAND bash -c "rm -f LaTeX/tests/*.pdf LaTeX/tests/*.aux LaTeX/tests/*.log LaTeX/tests/*.out LaTeX/tests/bnfnodes.dat LaTeX/tests/*-diagrams.tex LaTeX/tests/*_wrapper.tex"
  # Manual build artifacts (keep manual.tex and manual-examples.ebnf)
  COMMAND bash -c "rm -f manual/*.aux manual/*.log manual/*.out manual/*.toc manual/*.pdf manual/*-rail.ebnf manual/*-rail.tex manual/bnfnodes.dat"
  # CMake products
  COMMAND ${CMAKE_COMMAND} -E rm -f
    CMakeCache.txt cmake_install.cmake Makefile .depend
  COMMAND ${CMAKE_COMMAND} -E rm -rf CMakeFiles
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Removing all build artifacts, PDFs, LaTeX intermediates, and CMake products"
)
