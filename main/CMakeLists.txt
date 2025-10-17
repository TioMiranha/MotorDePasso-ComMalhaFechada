idf_component_register(SRCS "bicaTest.c" "components/aceleracao.c" "components/amortecedor.c" "components/direcao.c"
    "components/initializer.c" "components/motor.c" "components/movimento.c" "components/rampaAceleracao.c" "components/rampaDesaceleracao.c"
    INCLUDE_DIRS "." "include"
    REQUIRES driver
)
