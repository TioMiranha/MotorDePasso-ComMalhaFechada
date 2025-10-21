// No início do arquivo, após os includes
#include "../functions/motion.h"
/*

void iniciar_home(void)
{
    if (home_start())
    {
        printf("🏠 Comando de home enviado\n");
    }
    else
    {
        printf("❌ Falha ao iniciar home\n");
    }
}

void parar_home(void)
{
    home_stop();
}

bool sistema_referenciado(void)
{
    return home_is_complete();
}

bool home_em_andamento(void)
{
    return home_is_homing();
}

// Obter posição absoluta atual (após home)
int32_t obter_posicao_absoluta(void)
{
    return encoder_get_position();
}

// Função para verificar estado atual (útil para debug)
void imprimir_estado_home(void)
{
    const char *estados[] = {
        "HOME_IDLE", "HOME_FAST_SEARCH", "HOME_BACK_OFF",
        "HOME_SLOW_SEARCH", "HOME_COMPLETE", "HOME_ERROR"};

    printf("🏠 Estado Home: %s | Referenciado: %s | Em andamento: %s | Posição: %d\n",
           estados[home_control.state],
           home_control.home_found ? "SIM" : "NÃO",
           home_control.homing_in_progress ? "SIM" : "NÃO",
           encoder_get_position());
}

void inicializar_sistema(void)
{
    // Suas inicializações existentes...
    TimerInitPulse();
    InitMotionControl(0); // Inicializa apenas o eixo 0

    // Inicializar sistema de home (inclui encoder)
    home_init();

    // Criar tarefa do home
    xTaskCreate(home_task, "Home Task", 4096, NULL, 3, NULL);

    printf("✅ Sistema inicializado com encoder e função home\n");
}

// Exemplo de uso completo:
void exemplo_uso_completo(void)
{
    // 1. Iniciar home
    printf("🎯 Iniciando sequência de home...\n");
    iniciar_home();

    // 2. Aguardar home completar
    while (!sistema_referenciado() && home_em_andamento())
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        imprimir_estado_home(); // Mostra progresso
    }

    // 3. Verificar resultado
    if (sistema_referenciado())
    {
        printf("🎉 Sistema referenciado com sucesso!\n");
        printf("📊 Posição absoluta atual: %d\n", obter_posicao_absoluta());

        // 4. Agora pode usar movimentos absolutos
        printf("➡️  Movendo para posição 5000...\n");
        MotorControlAbs(0, 5000, 1000, 1500, 1);

        // Aguardar movimento completar
        while (!MotionComplete(0))
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            printf("📊 Posição atual: %d\n", obter_posicao_absoluta());
        }

        printf("✅ Chegou na posição 5000\n");
    }
    else
    {
        printf("❌ Falha no referenciamento. Verifique:\n");
        printf("   - Fim de curso conectado e funcionando\n");
        printf("   - Encoder conectado nos pinos corretos\n");
        printf("   - Motor conseguindo se mover\n");
    }
}

// Comando para parar home se necessário
void comando_emergencia(void)
{
    parar_home();
}

// Função para testar o encoder sem home
void testar_encoder(void)
{
    printf("🔧 Testando encoder...\n");

    for (int i = 0; i < 10; i++)
    {
        printf("📊 Posição do encoder: %d\n", obter_posicao_absoluta());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void teste_completo_sistema(void) {
    // 1. Testar encoder
    testar_encoder();
    
    // 2. Executar home
    exemplo_uso_completo();
    
    // 3. Testar movimentos absolutos
    printf("🧪 Testando movimentos absolutos...\n");
    mover_para_posicao(1000);
    mover_para_posicao(-1000);
    mover_para_posicao(0);
}

void mover_para_posicao(int32_t posicao) {
    printf("➡️  Movendo para posição %d...\n", posicao);
    MotorControlAbs(0, posicao, 1000, 1500, 1);
    
    while (!MotionComplete(0)) {
        vTaskDelay(pdMS_TO_TICKS(100));
        printf("📊 Posição atual: %d\n", obter_posicao_absoluta());
    }
    
    printf("✅ Chegou na posição %d\n", posicao);
}*/