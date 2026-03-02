cmake_minimum_required(VERSION 3.16)
project(vhdl-grammar NONE)

set(EBNF2TIKZ "${CMAKE_CURRENT_SOURCE_DIR}/../experimental/build/ebnf2tikz"
    CACHE FILEPATH "Path to ebnf2tikz binary")

find_program(PDFLATEX pdflatex)
if(NOT PDFLATEX)
  message(FATAL_ERROR "pdflatex not found")
endif()

add_custom_target(pdf
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/build_pdf.sh ${EBNF2TIKZ}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "Building VHDL grammar railroad diagram PDF"
)
