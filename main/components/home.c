#include "../functions/home.h"
#include <stdio.h>

// Variáveis globais
home_control_t home_control = {
    .state = HOME_IDLE,
    .home_found = false,
    .home_position = 0,
    .start_time = 0,
    .homing_in_progress = false
};

SemaphoreHandle_t xMutexHome = NULL;

// Inicialização do sistema de home
void home_init(void) {
    // Criar mutex
    if (xMutexHome == NULL) {
        xMutexHome = xSemaphoreCreateMutex();
    }
    
    // Configurar GPIO do fim de curso
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << HOME_SWITCH_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,    // Assumindo fim de curso ativo em LOW
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    printf("✅ Sistema de Home inicializado - Fim de curso: GPIO %d\n", HOME_SWITCH_GPIO);
}

// Verifica se o fim de curso está ativado
bool home_switch_activated(void) {
    return gpio_get_level(HOME_SWITCH_GPIO) == 0; // Ajuste conforme seu hardware
}

void doMotion(u8 axis_num,u8t dir) {
	float tempAccel;
	float tempMaxSpeed;
	float t;
	int32_t distToTarget;
	int32_t accelDist;

	distToTarget = abs(axis[axis_num].targetLocation - axis[axis_num].currentLocation); // figure out the distance we will travel on this move

	tempAccel =axis[axis_num].moveAccelSteps; // * stepsInch; // in steps per sec
	tempAccel = tempAccel / (ENGINE_RATE * ENGINE_RATE);
	tempAccel = tempAccel * SPEED_OFFSET;
	axis[axis_num].accel = (int32_t) tempAccel;

	tempMaxSpeed = axis[axis_num].moveMaxSpeedSteps;
	tempMaxSpeed = (tempMaxSpeed / ENGINE_RATE) * SPEED_OFFSET;
	axis[axis_num].maxSpeed = (int32_t) tempMaxSpeed;

	//determine Accel distance
	//     d = 1/2 a t^2
	t = (float) axis[axis_num].moveMaxSpeedSteps / (float) axis[axis_num].moveAccelSteps;
	accelDist = (axis[axis_num].moveAccelSteps / 2) * t * t;

	// determine the direction
	if (axis[axis_num].targetLocation > axis[axis_num].currentLocation) {
		axis[axis_num].motionDirection = DIRECTION_FORWARD;

	    if(!axis[axis_num].flg_zrn){
			gpio_set_level(_SAIDAS_Y[dir],1);// determina direção horario
			SET_Y(dir);// determina direção horario anti-horaro
	    }


		axis[axis_num].decelLocation = axis[axis_num].currentLocation + axis[axis_num].decelLocation;


		if (accelDist >= distToTarget / 2)
			axis[axis_num].decelLocation = axis[axis_num].targetLocation - distToTarget / 2;
		else
			axis[axis_num].decelLocation = axis[axis_num].targetLocation - accelDist;

	} else {
		axis[axis_num].motionDirection = DIRECTION_REVERSE;

         if(!axis[axis_num].flg_zrn){
    		 gpio_set_level(_SAIDAS_Y[dir],0);// determina direção  ANTI-horario
    	     CLR_Y(dir);// determina direção horario anti-horaro

         }


		axis[axis_num].decelLocation = axis[axis_num].currentLocation - axis[axis_num].decelLocation;

		if (accelDist >= distToTarget / 2)
			axis[axis_num].decelLocation = axis[axis_num].targetLocation + distToTarget / 2;
		else
			axis[axis_num].decelLocation = axis[axis_num].targetLocation + accelDist;

	}

	esp_rom_delay_us(4);

    if(axis_num == 0)	init_rmt_motor0();
    if(axis_num == 1)	init_rmt_motor1();

	axis[axis_num].bDecel = false;
	axis[axis_num].currentSpeed = 0; // initialize this
	axis[axis_num].bEnableMotion = true; // all set time to move
	axis[axis_num].ativo =true;
	HabilitaTimer();
}

int32_t MotorControlAbs(u8 axis_num ,int32_t position, u32 ramp, u32 rpm, u8t  dir ) {
	    u32 rampa;
	if (!axis[axis_num].bEnableMotion) {
		if(ramp <= 100 )ramp = 10;// se rampa for zero add 100 limite mimino de rampa
		if(ramp > 10000 )ramp = 10000;// se rampa for zero add 100 limite mimino de rampa
		rampa =(u32) Scale(ramp, 100,10000, 10000, 100);
		axis[axis_num].moveAccelSteps = rampa; //rampa
		if(rpm > LIMITE_MAX_FREQ  )rpm = LIMITE_MAX_FREQ ; // limita frequencia proteção
		if(rpm < 1 )rpm = 10;
		axis[axis_num].moveMaxSpeedSteps = rpm; // velocidade;
		axis[axis_num].targetLocation = position ;
	//	if(dir > 8 )dir = 8;//  definição para o maximo numero de eixos 4 eixos
		doMotion(axis_num, dir );
	}
	return axis[axis_num].currentLocation;

} 

int32_t MotorControlRel(u8 axis_num ,int32_t position, u32 ramp, u32 rpm, u8t dir ) {

	  u32 rampa;

	if (!axis[axis_num].bEnableMotion) {
		if(ramp <= 100 )ramp = 10;// se rampa for zero add 100 limite mimino de rampa
		if(ramp >= 10000 )ramp = 10000;// se rampa for zero add 100 limite mimino de rampa
		rampa =(u32) Scale(ramp, 100,10000, 10000, 100);
		axis[axis_num].moveAccelSteps = rampa; //rampa
		if(rpm > LIMITE_MAX_FREQ  )rpm = LIMITE_MAX_FREQ ;// limita frequencia maxima proteção
		if(rpm < 1 )rpm = 10;// limita frequencia minima proteção
		axis[axis_num].moveMaxSpeedSteps = rpm; // velocidade;
		axis[axis_num].targetLocation = position;
		axis[axis_num].currentLocation = 0;// MODO RELAITO A POISÇÃO ATUAL É ZERADA
		//if(dir > 8 )dir = 8;// definição para o maximo numero de eixos 4 eixos
		doMotion(axis_num,dir);
	}
	return axis[axis_num].currentLocation;
} 

// Inicia sequência de home
bool home_start(void) {
    if (xSemaphoreTake(xMutexHome, portMAX_DELAY)) {
        if (home_control.homing_in_progress) {
            xSemaphoreGive(xMutexHome);
            printf("⚠️  Home já está em andamento!\n");
            return false;
        }
        
        home_control.state = HOME_FAST_SEARCH;
        home_control.start_time = xTaskGetTickCount();
        home_control.homing_in_progress = true;
        home_control.home_found = false;
        
        printf("🏠 INICIANDO SEQUÊNCIA DE HOME\n");
        xSemaphoreGive(xMutexHome);
        return true;
    }
    return false;
}

// Para a sequência de home
void home_stop(void) {
    if (xSemaphoreTake(xMutexHome, portMAX_DELAY)) {
        home_control.state = HOME_IDLE;
        home_control.homing_in_progress = false;
        MotorStop(0); // Para o motor no eixo 0
        xSemaphoreGive(xMutexHome);
        printf("🛑 Home interrompido\n");
    }
}

// Verifica se home está completo
bool home_is_complete(void) {
    bool complete;
    if (xSemaphoreTake(xMutexHome, portMAX_DELAY)) {
        complete = (home_control.state == HOME_COMPLETE);
        xSemaphoreGive(xMutexHome);
    }
    return complete;
}

// Verifica se home está em andamento
bool home_is_homing(void) {
    bool homing;
    if (xSemaphoreTake(xMutexHome, portMAX_DELAY)) {
        homing = home_control.homing_in_progress;
        xSemaphoreGive(xMutexHome);
    }
    return homing;
}

// Obtém posição de home
int32_t home_get_position(void) {
    int32_t position;
    if (xSemaphoreTake(xMutexHome, portMAX_DELAY)) {
        position = home_control.home_position;
        xSemaphoreGive(xMutexHome);
    }
    return position;
}

// Tarefa principal do home
void home_task(void *pvParameters) {
    printf("🔧 Tarefa de Home iniciada\n");
    
    for (;;) {
        if (home_control.homing_in_progress) {
            switch (home_control.state) {
                
                case HOME_FAST_SEARCH: {
                    printf("🔍 FASE 1: Busca rápida do home\n");
                    
                    // Mover na direção negativa para buscar home
                    // Usando eixo 0 fixo e direção 1 (ajuste conforme seu hardware)
                    MotorControlRel(0, -1000000, HOMING_ACCEL, HOMING_FAST_SPEED, 1);
                    
                    // Aguardar até encontrar fim de curso ou timeout
                    uint32_t current_time = xTaskGetTickCount();
                    while (home_control.state == HOME_FAST_SEARCH && 
                           (current_time - home_control.start_time) * portTICK_PERIOD_MS < HOMING_TIMEOUT_MS) {
                        
                        if (home_switch_activated()) {
                            printf("🎯 Fim de curso encontrado na busca rápida!\n");
                            MotorStop(0);
                            vTaskDelay(pdMS_TO_TICKS(100));
                            home_control.state = HOME_BACK_OFF;
                            break;
                        }
                        
                        if (MotionComplete(0)) {
                            printf("❌ Busca rápida completada sem encontrar home\n");
                            home_control.state = HOME_ERROR;
                            break;
                        }
                        
                        vTaskDelay(pdMS_TO_TICKS(10));
                        current_time = xTaskGetTickCount();
                    }
                    
                    if (home_control.state == HOME_FAST_SEARCH) {
                        printf("❌ Timeout na busca rápida\n");
                        home_control.state = HOME_ERROR;
                    }
                    break;
                }
                
                case HOME_BACK_OFF: {
                    printf("🔄 FASE 2: Recuando do fim de curso\n");
                    
                    // Recuar na direção positiva
                    MotorControlRel(0, 2000, HOMING_ACCEL, HOMING_SLOW_SPEED, 1);
                    
                    // Aguardar movimento completar ou sair do fim de curso
                    uint32_t backoff_start = xTaskGetTickCount();
                    while (home_control.state == HOME_BACK_OFF && 
                           (xTaskGetTickCount() - backoff_start) * portTICK_PERIOD_MS < 5000) {
                        
                        if (!home_switch_activated()) {
                            printf("✅ Saiu do fim de curso\n");
                            MotorStop(0);
                            vTaskDelay(pdMS_TO_TICKS(100));
                            home_control.state = HOME_SLOW_SEARCH;
                            break;
                        }
                        
                        if (MotionComplete(0)) {
                            home_control.state = HOME_SLOW_SEARCH;
                            break;
                        }
                        
                        vTaskDelay(pdMS_TO_TICKS(10));
                    }
                    
                    if (home_control.state == HOME_BACK_OFF) {
                        home_control.state = HOME_SLOW_SEARCH;
                    }
                    break;
                }
                
                case HOME_SLOW_SEARCH: {
                    printf("🎯 FASE 3: Busca lenta final\n");
                    
                    // Mover lentamente para encontrar home com precisão
                    MotorControlRel(0, -1000, HOMING_ACCEL, HOMING_SLOW_SPEED / 2, 1);
                    
                    uint32_t slow_start = xTaskGetTickCount();
                    while (home_control.state == HOME_SLOW_SEARCH && 
                           (xTaskGetTickCount() - slow_start) * portTICK_PERIOD_MS < 5000) {
                        
                        if (home_switch_activated()) {
                            printf("🎯 Home encontrado com precisão!\n");
                            MotorStop(0);
                            vTaskDelay(pdMS_TO_TICKS(100));
                            
                            // ZERAR POSIÇÃO ABSOLUTA
                            // Usando sua função existente para zerar a posição
                            if (xSemaphoreTake(xMutexHome, portMAX_DELAY)) {
                                axis[0].currentLocation = 0;  // Zera a posição do motor
                                home_control.home_position = 0;
                                home_control.home_found = true;
                                home_control.state = HOME_COMPLETE;
                                xSemaphoreGive(xMutexHome);
                            }
                            
                            printf("🎉 HOME CONCLUÍDO! Posição absoluta: 0\n");
                            break;
                        }
                        
                        if (MotionComplete(0)) {
                            printf("❌ Busca lenta completada sem encontrar home\n");
                            home_control.state = HOME_ERROR;
                            break;
                        }
                        
                        vTaskDelay(pdMS_TO_TICKS(10));
                    }
                    
                    if (home_control.state == HOME_SLOW_SEARCH) {
                        printf("❌ Timeout na busca lenta\n");
                        home_control.state = HOME_ERROR;
                    }
                    break;
                }
                
                case HOME_COMPLETE: {
                    home_control.homing_in_progress = false;
                    printf("✅ Sequência de home finalizada com sucesso\n");
                    break;
                }
                
                case HOME_ERROR: {
                    MotorStop(0);
                    home_control.homing_in_progress = false;
                    printf("❌ Erro na sequência de home\n");
                    break;
                }
                
                default:
                    break;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(50)); // Executar a 20Hz
    }
}