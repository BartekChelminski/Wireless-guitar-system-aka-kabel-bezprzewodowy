idf_component_register(
    SRCS
        "src/lc3.c"
        "src/attdet.c"
        "src/bits.c"
        "src/bwdet.c"
        "src/energy.c"
        "src/ltpf.c"
        "src/mdct.c"
        "src/plc.c"
        "src/sns.c"
        "src/spec.c"
        "src/tables.c"
        "src/tns.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
)

# -O3 dla wydajności kodeka na Xtensa LX7 FPU
# -ffast-math bezpieczne na Xtensa LX7 (IEEE 754 z hardware FPU)
target_compile_options(${COMPONENT_LIB} PRIVATE -O3 -ffast-math)
