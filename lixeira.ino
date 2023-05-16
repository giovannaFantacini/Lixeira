#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <HCSR04.h>
#include "DHTesp.h"

LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 16, 2);

//Aqui estariam as definições do Blynk que foram excluidas por questões de segurança

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Wokwi-GUEST";
char pass[] = "";

BlynkTimer timer;

/* defines - LCD */
#define LCD_16X2_CLEAN_LINE                "                "
#define LCD_16X2_I2C_ADDRESS               0x27
#define LCD_16X2_COLS                      16 
#define LCD_16X2_ROWS                      2 

// Definindo as Portas das I/Os
#define PINO_TRIGGER 5
#define PINO_ECHO 18

/* tasks */
void task_lcd( void *pvParameters );
void task_sensor_hum( void *pvParameters );
void task_sensor_dist( void *pvParameters );


/* filas (queues) */
QueueHandle_t xQueue_ESTADO, xQueue_ALERTA;

/* semaforos utilizados */
SemaphoreHandle_t xSerial_semaphore;

DHTesp dhtSensor;

UltraSonicDistanceSensor distanceSensor(PINO_TRIGGER, PINO_ECHO); 

// Definindo os Estados,
enum Estado {
  A, // Estado "VAZIO"
  B, // Estado "EM USO"
  C, // Estado "CHEIO"
  D // Estado "EM ALERTA"
};

Estado estado;
  

void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();
  Blynk.begin(auth, ssid, pass);

  dhtSensor.setup(15, DHTesp::DHT22);
  
  while (!Serial) {
    ; /* Somente vai em frente quando a serial estiver pronta para funcionar */
  }


  /* Criação das filas (queues) */ 
  xQueue_ESTADO = xQueueCreate( 1, sizeof( Estado ) );
  xQueue_ALERTA = xQueueCreate( 1, sizeof( Estado ) );

  /* Criação dos semaforos */
  xSerial_semaphore = xSemaphoreCreateMutex();

  if (xSerial_semaphore == NULL)
  {
     Serial.println("Erro: nao e possivel criar o semaforo");
     while(1); /* Sem semaforo o funcionamento esta comprometido. Nada mais deve ser feito. */
  }
  
  /* Criação das tarefas */
  
  xTaskCreatePinnedToCore(
     task_sensor_hum              
    , "sensor umidade"  
    ,  5000                        
    ,  NULL                        
    ,  3                           
    ,  NULL
    ,  APP_CPU_NUM );                    

  xTaskCreatePinnedToCore(
      task_lcd 
      ,"LCD"
      , 5000
      , NULL
      , 2
      , NULL
      , APP_CPU_NUM
  );
  xTaskCreatePinnedToCore(
      task_sensor_dist
      ,"sensor distancia"
      , 5000
      , NULL
      , 2
      , NULL
      , APP_CPU_NUM
  );
}



void loop() {
  delay(250);
}



void task_sensor_hum( void *pvParameters )
{
    (void) pvParameters;
    int adc_read= 0;
    UBaseType_t uxHighWaterMark;
    TempAndHumidity  data;
    Estado estado;
    int alerta;

    Blynk.run(); 

    while(1)
    {   
        data = dhtSensor.getTempAndHumidity();
        Blynk.virtualWrite(V0, data.temperature); 
        Blynk.virtualWrite(V1, data.humidity); 

        if (data.humidity < 20.00 || data.humidity > 90.00){
          alerta = 1;
          xQueueOverwrite(xQueue_ALERTA, (void *)&alerta);
        }else{
          alerta = 0;
          xQueueOverwrite(xQueue_ALERTA, (void *)&alerta);
        }
        
        vTaskDelay( 1000 / portTICK_PERIOD_MS ); 

        xSemaphoreTake(xSerial_semaphore, portMAX_DELAY );

        uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
        Serial.print("task_sensor temp: " + String(data.temperature, 2) + "°C");
        Serial.println("Humidity: " + String(data.humidity, 1) + "%");
        xSemaphoreGive(xSerial_semaphore);
        
       
    }
}

void task_lcd( void *pvParameters )
{
    (void) pvParameters;
    TempAndHumidity  data_rcv;
    String msg;
    UBaseType_t uxHighWaterMark;
    Estado estado;
    int alerta;


    while(1)
    {        
       
        xQueueReceive(xQueue_ESTADO, (void *)&estado, portMAX_DELAY);
        xQueueReceive(xQueue_ALERTA, (void *)&alerta, portMAX_DELAY);

        if(alerta == 1){
           msg = "EM ALERTA";
        }else{
          if (estado == A){
            msg = "VAZIO";
          }else if(estado == B){
            msg = "EM USO";
          }else if(estado == C){
            msg = "CHEIO";
          }
        }
        
        lcd.clear();
        
        
        lcd.setCursor(1,0);
        lcd.print(msg); 

        xSemaphoreTake(xSerial_semaphore, portMAX_DELAY );
        
        uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
        Serial.println("estado ");
        Serial.println(estado);
        Serial.println("---");
        xSemaphoreGive(xSerial_semaphore);
    }  
}

void task_sensor_dist( void *pvParameters )
{
    (void) pvParameters;
    int adc_read= 0;
    UBaseType_t uxHighWaterMark;
    float distancia;
    Estado estado;

    Blynk.run(); 

    while(1)
    {   
        
        distancia = distanceSensor.measureDistanceCm();

       
        if (distancia > 9.50 and distancia < 13.0) {
          estado = A;
        } else if (distancia < 9.5 and distancia > 4.2) {
          estado = B;
        } else if (distancia < 4.2) {
          estado = C;
        }
  
        xQueueOverwrite(xQueue_ESTADO, (void *)&estado);
        Blynk.virtualWrite(V4, distancia);
        

        vTaskDelay( 1000 / portTICK_PERIOD_MS ); 

        xSemaphoreTake(xSerial_semaphore, portMAX_DELAY );

        uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
        Serial.print("distancia: " + String(distancia, 2) + "cm");
        xSemaphoreGive(xSerial_semaphore);
        
       
    }
}
