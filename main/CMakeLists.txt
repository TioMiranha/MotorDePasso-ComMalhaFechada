idf_component_register(SRCS "bicaTest.c" "components/velocidade.c" "components/amortecedor.c" "components/direcao.c"
    "components/initializer.c" "components/motor.c" "components/movimento.c" "components/rampaAceleracao.c" "components/rampaDesaceleracao.c"
    "GLOBAL_VARS/vars.c" "components/home.c" "components/motion.c" 
    INCLUDE_DIRS "." "include"
    REQUIRES driver
)
