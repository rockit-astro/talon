cmake_minimum_required (VERSION 2.6)
project (byacc)

set(BYACC_SRC closure.c error.c lalr.c lr0.c main.c mkpar.c output.c reader.c
skeleton.c symtab.c verbose.c warshall.c)


add_executable(byacc ${BYACC_SRC})

install (TARGETS byacc DESTINATION ${CORE_CSIMC_DIR}/byacc)