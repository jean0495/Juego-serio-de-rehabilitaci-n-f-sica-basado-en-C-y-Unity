/*
 * Proyecto final - SIN string.h
 * Daniela Murillo - Jean Sebastian Gargano
 
 */ 
#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <util/delay.h>
#include <math.h>
#include "I2C.h"       // Para la comunicación con el MPU6050
#include "uu.h"        // Para USART0
#include <avr/sleep.h>
#define BAUD_RATE 9600
#define UBRR_VALUE ((F_CPU / (16UL * BAUD_RATE)) - 1)
#define CANAL_ADC 0

// Umbral para activación del EMG
#define UMBRAL_EMG 700  

// Configuración del modo sleep
#define TIEMPO_INACTIVIDAD_SEG 10     

// Dirección I2C del MPU6050
const int DIRECCION_MPU6050 = 0x68;

// Enum para direcciones 
typedef enum {
    DIR_CENTRO = 0,
    DIR_DERECHA = 1,
    DIR_IZQUIERDA = 2
} direccion_t;

// Variables para EMG
volatile uint16_t valor_emg = 0;
volatile uint8_t adc_listo = 0;

// Variables para MPU6050
volatile int16_t acelX, acelY, acelZ;
volatile float anguloY;
volatile uint8_t datos_mpu_listos = 0;

// Variables para la prioridad pa mostrar en el USART
volatile uint8_t emg_activo = 0;
volatile uint8_t enviar_datos_giro = 1;

// Variables para control del modo sleep
volatile uint8_t modo_sleep_activo = 0;
volatile uint8_t bandera_despertar = 0;
volatile uint32_t contador_inactividad = 0;
volatile uint8_t contador_segundos = 0;
volatile direccion_t direccion_anterior = DIR_CENTRO;

// Prototipos de funciones
void inicializar_adc(void);
void inicializar_mpu6050(void);
void leer_acelerometro(void);
void calcular_angulo_y(void);
direccion_t enviar_direccion_usart0(void);  
void configurar_timers(void);
void verificar_umbral_emg(void);
void entrar_modo_sleep(void);
void despertar_sistema(void);
void verificar_inactividad(direccion_t direccion_actual);  
void configurar_interrupcion_mpu6050(void);
void configurar_interrupcion_externa(void);
void configurar_timer_segundos(void);
void escribir_registro_mpu6050(uint8_t reg, uint8_t data);
void leer_registros_mpu6050(uint8_t reg, uint8_t* buffer, uint8_t cantidad);

//-------------------------------------------------------------------------------------------------------------------------------

int main(void) {
	
    USART0_init(UBRR_VALUE);
    inicializar_adc();
    configurar_timer_segundos();
    configurar_timers();
    
    // Inicializar I2C para MPU6050
    i2c_init();
    configurar_interrupcion_externa();
    
    // Mensajes iniciales
    USART0_send_string("=== Sistema EMG + Giroscopio con Modo Sleep ===\n");
    USART0_send_string("EMG tiene prioridad sobre giroscopio\n");
    USART0_send_string("Sleep tras 10 seg de inactividad\n");
    USART0_send_string("Sistema listo!\n\n");
    
    // Inicializar MPU6050
    inicializar_mpu6050();
    configurar_interrupcion_mpu6050();
    
    sei(); // Habilitar interrupciones globales
    
    while(1) {
        
        // Verificar si hay que despertar el sistema
        if (bandera_despertar) {
            despertar_sistema();
            bandera_despertar = 0;
        }
        
        // Si estamos en modo sleep, no ejecutar el resto del código
        if (modo_sleep_activo) {
            entrar_modo_sleep();
            continue;
        }
        
        if (contador_segundos) {
            contador_inactividad++;
            contador_segundos = 0;
            
            // Verificar si se alcanzó el tiempo de inactividad
            if (contador_inactividad >= TIEMPO_INACTIVIDAD_SEG) {
                USART0_send_string(">>> ENTRANDO EN MODO SLEEP <<<\n");
                modo_sleep_activo = 1;
                contador_inactividad = 0;
            }
        }
        
        // Verificar umbral de EMG cuando hay datos listos
        if (adc_listo) {
            verificar_umbral_emg();
            adc_listo = 0;
        }
        
        // Lógica de envío prioritario
        if (emg_activo) {
            
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "EMG:%u\n", valor_emg);
            USART0_send_string(buffer);
            
            contador_inactividad = 0;
            direccion_anterior = DIR_CENTRO;  
            
            _delay_ms(100);
        }
        else if (datos_mpu_listos && enviar_datos_giro) {
            // Si EMG no está activo, enviar datos del giroscopio
            leer_acelerometro();
            calcular_angulo_y();
            direccion_t dir_actual = enviar_direccion_usart0();  // ? Recibe enum
            verificar_inactividad(dir_actual);                   // ? Pasa enum
            
            datos_mpu_listos = 0;
            _delay_ms(50);
        }
    }
    
    return 0;
}

void verificar_inactividad(direccion_t direccion_actual) {
    if (direccion_actual != direccion_anterior) {
        // Dirección cambió, resetear timer
        contador_inactividad = 0;
        direccion_anterior = direccion_actual;  // ? Asignación simple
    }
}

void despertar_sistema(void) {
    
    ADCSRA |= (1 << ADEN);            // Habilitar ADC
    TCCR1B = (1 << CS10);             // Rehabilitar Timer1
    TCNT1 = 25536;
    TIMSK1 |= (1 << TOIE1);
    
    // CAMBIO: Rehabilitar Timer 3 en lugar de Timer 2
    TCCR3B = (1 << CS32);             // Prescaler 256 para Timer 3
    TCNT3 = 3036;                     // Valor para 1 segundo
    TIMSK3 |= (1 << TOIE3);           // Habilitar interrupción overflow Timer 3
    
    _delay_ms(100);
    
    modo_sleep_activo = 0;
    contador_inactividad = 0;
    direccion_anterior = DIR_CENTRO;  
    
    // Limpiar interrupción del MPU6050
    uint8_t estado_int;
    leer_registros_mpu6050(0x3A, &estado_int	, 1);
    
    USART0_send_string(">>> SISTEMA DESPERTADO <<<\n");
}

direccion_t enviar_direccion_usart0(void) {
    float umbral_centro = 15.0;
    direccion_t direccion_actual;
    
    if (anguloY >= -umbral_centro && anguloY <= umbral_centro) {
        direccion_actual = DIR_CENTRO;
        USART0_send_string("centro\n");
    } else if (anguloY > umbral_centro) {
        direccion_actual = DIR_DERECHA;
        USART0_send_string("derecha\n");
    } else {
        direccion_actual = DIR_IZQUIERDA;
        USART0_send_string("izquierda\n");
    }
    
    return direccion_actual;  // ? Retorna el enum
}

// Resto de funciones permanecen igual...
void configurar_interrupcion_externa(void) {
    DDRD &= ~(1 << PD2);
    PORTD |= (1 << PD2);
    EICRA |= (1 << ISC21);
    EICRA &= ~(1 << ISC20);
    EIMSK |= (1 << INT2);
}

void configurar_interrupcion_mpu6050(void) {
    escribir_registro_mpu6050(0x37, 0x80);
    escribir_registro_mpu6050(0x38, 0x40);
    escribir_registro_mpu6050(0x1F, 0x20);
    escribir_registro_mpu6050(0x20, 0x01);
    _delay_ms(10);
}

// CAMBIO: Timer 2 -> Timer 3 para contar segundos
void configurar_timer_segundos(void) {
    TCCR3A = 0;                       // Modo normal (no hay WGM para Timer 3)
    TCCR3B = (1 << CS32);             // Prescaler 256
    TCNT3 = 3036;                     // Valor inicial para 1 segundo
    TIMSK3 |= (1 << TOIE3);           // Habilitar interrupción overflow
}

void entrar_modo_sleep(void) {
    ADCSRA &= ~(1 << ADEN);
    TIMSK1 &= ~(1 << TOIE1);
    TCCR1B = 0;
    
    // CAMBIO: Deshabilitar Timer 3 en lugar de Timer 2
    TIMSK3 &= ~(1 << TOIE3);          // Deshabilitar interrupción Timer 3
    TCCR3B = 0;                       // Detener Timer 3
    
    SMCR = (1 << SM1) | (1 << SE);
    
    cli();
    sei();
    
    SMCR &= ~(1 << SE);
}

void verificar_umbral_emg(void) {
    static uint8_t contador_emg = 0;
    
    if (valor_emg > UMBRAL_EMG) {
        contador_emg++;
        if (contador_emg >= 3) {
            if (!emg_activo) {
                USART0_send_string(">>> EMG ACTIVADO <<<\n");
                emg_activo = 1;
                enviar_datos_giro = 0;
            }
            contador_emg = 3;
        }
    } else {
        if (contador_emg > 0) {
            contador_emg--;
        }
        
        if (contador_emg == 0 && emg_activo) {
            USART0_send_string(">>> EMG DESACTIVADO <<<\n");
            emg_activo = 0;
            enviar_datos_giro = 1;
        }
    }
}

void inicializar_adc(void) {
    ADMUX = (0 << REFS1) | (1 << REFS0) | (0 << ADLAR) | CANAL_ADC;
    ADCSRA = (1 << ADEN) | (1 << ADIE) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

void escribir_registro_mpu6050(uint8_t reg, uint8_t data) {
    i2c_start();
    i2c_write((DIRECCION_MPU6050 << 1) | 0);
    i2c_write(reg);
    i2c_write(data);
    i2c_stop();
}

void leer_registros_mpu6050(uint8_t reg, uint8_t* buffer, uint8_t cantidad) {
    i2c_start();
    i2c_write((DIRECCION_MPU6050 << 1) | 0);
    i2c_write(reg);
    
    i2c_start();
    i2c_write((DIRECCION_MPU6050 << 1) | 1);
    
    for (uint8_t i = 0; i < cantidad; i++) {
        if (i == cantidad - 1) {
            buffer[i] = i2c_read_nack();
        } else {
            buffer[i] = i2c_read_ack();
        }
    }
    
    i2c_stop();
}

void inicializar_mpu6050(void) {
    escribir_registro_mpu6050(0x6B, 0);
    escribir_registro_mpu6050(0x1C, 0x00);
    _delay_ms(100);
}

void configurar_timers(void) {
    TCCR1B = (1 << CS10);
    TCNT1 = 25536;
    TIMSK1 = (1 << TOIE1);
}

void leer_acelerometro(void) {
    uint8_t datos[6];
    
    leer_registros_mpu6050(0x3B, datos, 6);
    
    acelX = (int16_t)((datos[0] << 8) | datos[1]);
    acelY = (int16_t)((datos[2] << 8) | datos[3]);
    acelZ = (int16_t)((datos[4] << 8) | datos[5]);
}

void calcular_angulo_y(void) {
    if (acelX == 0 && acelY == 0 && acelZ == 0) {
        anguloY = 0.0;
        return;
    }
    
    float acelX_g = (float)acelX / 16384.0;
    float acelY_g = (float)acelY / 16384.0;
    float acelZ_g = (float)acelZ / 16384.0;
    
    float denominador = sqrt(acelY_g * acelY_g + acelZ_g * acelZ_g);
    
    if (denominador > 0.001) {
        anguloY = atan2(-acelX_g, denominador) * 180.0 / M_PI;
    } else {
        anguloY = 0.0;
    }
}

// CAMBIO: ISR Timer 2 -> Timer 3
ISR(TIMER3_OVF_vect) {
		TCNT3 = 3036;                     // Recargar valor para próximo segundo
		contador_segundos = 1;            // Marcar que pasó un segundo
}

ISR(INT2_vect) {
    bandera_despertar = 1;
}

ISR(ADC_vect) {
    valor_emg = ADC;
    adc_listo = 1;
}

ISR(TIMER1_OVF_vect) {
    TCNT1 = 25536;
    
    if (!modo_sleep_activo) {
        ADCSRA |= (1 << ADSC);
        datos_mpu_listos = 1;
    }
}