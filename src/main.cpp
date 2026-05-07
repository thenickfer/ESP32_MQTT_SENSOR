#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <string>

#define MQTT_ID "esp1_pub"
#define topico_status "nickfer/status"
#define topico_ultrassom "nickfer/ultrassom"
#define topico_presenca "nickfer/presenca"
#define topico_luminosidade "nickfer/luminosidade"
#define topico_temperatura "nickfer/temperatura"
#define topico_alarme "nickfer/alarme"

#define topico_servo "nickfer/comando/servo"
#define topico_armar "nickfer/comando/armar"
#define topico_buzzer "nickfer/comando/buzzer"
#define topico_config "nickfer/comando/config"

#define SOUND_SPEED 0.034

const gpio_num_t LDR_PIN = GPIO_NUM_35;
const gpio_num_t PIR_PIN = GPIO_NUM_32;
const gpio_num_t BUZZER_PIN = GPIO_NUM_33;
const gpio_num_t LED_PIN = GPIO_NUM_25;
const gpio_num_t SERVO_PIN = GPIO_NUM_12;

const char* ssid = "iPhone (7)";
const char* password = "12345678";
const char* mqtt_server = "broker.emqx.io";

bool status;
float distanceCm;
int servoAngle;
bool presenca;
int luminosidade;
float temperatura;
bool alarme;

bool servo;
bool armar;
bool buzzer;

SemaphoreHandle_t mutexStatus;
SemaphoreHandle_t mutexDistance;
SemaphoreHandle_t mutexServoAngle;
SemaphoreHandle_t mutexPresenca;
SemaphoreHandle_t mutexLuminosidade;
SemaphoreHandle_t mutexTemperatura;
SemaphoreHandle_t mutexAlarme;

SemaphoreHandle_t mutexServo;
SemaphoreHandle_t mutexArmar;
SemaphoreHandle_t mutexBuzzer;

WiFiClient espClient;
PubSubClient client(espClient);

void reconnect() {
    while (!client.connected()) {
        Serial.print("Tentando conexao MQTT...");
        if (client.connect("ESP32Client")) {
            Serial.println("conectado");
            client.subscribe(topico_servo);
            client.subscribe(topico_armar);
            client.subscribe(topico_buzzer);
            client.subscribe(topico_config);
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            delay(5000);
        }
    }
}

void conectaWifi()
{
  Serial.println("Conectando a rede wifi!");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.println("Conectando a rede wifi....");
  }
  Serial.println("Conectado a rede wifi com sucesso!!!");
}

void publicar(){

  bool statusLocal;
  float distanceCmLocal;
  int servoAngleLocal;
  bool presencaLocal;
  int luminosidadeLocal;
  float temperaturaLocal;
  bool alarmeLocal;

  xSemaphoreTake(mutexStatus, pdMS_TO_TICKS(100));
  statusLocal = status;
  xSemaphoreGive(mutexStatus);
  xSemaphoreTake(mutexDistance, pdMS_TO_TICKS(100));
  distanceCmLocal = distanceCm;
  xSemaphoreGive(mutexDistance);
  xSemaphoreTake(mutexServoAngle, pdMS_TO_TICKS(100));
  servoAngleLocal = servoAngle;
  xSemaphoreGive(mutexServoAngle);
  xSemaphoreTake(mutexPresenca, pdMS_TO_TICKS(100));
  presencaLocal = presenca;
  xSemaphoreGive(mutexPresenca);
  xSemaphoreTake(mutexLuminosidade, pdMS_TO_TICKS(100));
  luminosidadeLocal = luminosidade;
  xSemaphoreGive(mutexLuminosidade);
  xSemaphoreTake(mutexTemperatura, pdMS_TO_TICKS(100));
  temperaturaLocal = temperatura;
  xSemaphoreGive(mutexTemperatura);
  xSemaphoreTake(mutexAlarme, pdMS_TO_TICKS(100));
  alarmeLocal = alarme;
  xSemaphoreGive(mutexAlarme);

  client.publish(topico_status, (status ? "online" : "offline"));
  std::string resultado = std::to_string(distanceCmLocal) + ";" + std::to_string(servoAngleLocal);
  client.publish(topico_ultrassom, resultado.c_str());
  client.publish(topico_presenca, presencaLocal ? "detectada" : "vazia" );
  client.publish(topico_luminosidade, std::to_string(luminosidadeLocal).c_str() );
  client.publish(topico_temperatura, std::to_string(temperaturaLocal).c_str());
  client.publish(topico_presenca, alarmeLocal ? "ativado" : "desativado" );
}

void ldrTask(void *pvParameters) {
  int rawValue = analogRead(LDR_PIN);
  int lightPercent = map(rawValue, 0, 4095, 0, 100);
  xSemaphoreTake(mutexLuminosidade, pdMS_TO_TICKS(100));
  luminosidade = lightPercent;
  xSemaphoreGive(mutexLuminosidade);
  vTaskDelay(pdMS_TO_TICKS(5000));
}
void detectPresence() {
  xSemaphoreTake(mutexPresenca, pdMS_TO_TICKS(100));
  presenca = true;
  xSemaphoreGive(mutexPresenca);
}

void detectInvasionTask(void *pvParameters) {
  for (;;) { // Tasks precisam de um loop infinito externo
    int lightLocal;

    // 1. Busca o valor atualizado da luminosidade
    if (xSemaphoreTake(mutexLuminosidade, pdMS_TO_TICKS(10))) {
      lightLocal = luminosidade;
      xSemaphoreGive(mutexLuminosidade);
    }

    bool presencaLocal;

    if (xSemaphoreTake(mutexPresenca, pdMS_TO_TICKS(10))) {
      presencaLocal = presenca;
      xSemaphoreGive(mutexPresenca);
    }

    // 2. Verifica a condição de invasão
    if (lightLocal > 60 && presencaLocal) {
      // Alerta visual/sonoro (substituí delay por vTaskDelay)
      gpio_set_level(LED_PIN, 1);
      gpio_set_level(BUZZER_PIN, 1);
      vTaskDelay(pdMS_TO_TICKS(500)); 
      
      gpio_set_level(LED_PIN, 0);
      gpio_set_level(BUZZER_PIN, 0);
      vTaskDelay(pdMS_TO_TICKS(500));

      // 3. Atualiza o status de presença via Mutex
      if (xSemaphoreTake(mutexPresenca, pdMS_TO_TICKS(10))) {
        presenca = false;
        xSemaphoreGive(mutexPresenca);
      }
    } else {
      // Pequena espera se não houver invasão para não estressar a CPU
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}

void setup() {
  Serial.begin(115200);

  gpio_set_direction(LDR_PIN, GPIO_MODE_INPUT);
  gpio_set_direction(PIR_PIN, GPIO_MODE_INPUT);
  gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);
  gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

  // Inicializando cada Mutex
  mutexStatus       = xSemaphoreCreateMutex();
  mutexDistance     = xSemaphoreCreateMutex();
  mutexServoAngle   = xSemaphoreCreateMutex();
  mutexPresenca     = xSemaphoreCreateMutex();
  mutexLuminosidade = xSemaphoreCreateMutex();
  mutexTemperatura  = xSemaphoreCreateMutex();
  mutexAlarme       = xSemaphoreCreateMutex();
  mutexServo        = xSemaphoreCreateMutex();
  mutexArmar        = xSemaphoreCreateMutex();
  mutexBuzzer       = xSemaphoreCreateMutex();

  // Verificação simples
  if (mutexStatus == NULL) { Serial.println("Erro ao criar semáforos"); }
  conectaWifi();
  client.setServer(mqtt_server, 1883);

  attachInterrupt(digitalPinToInterrupt(PIR_PIN), detectPresence, RISING);

  xTaskCreatePinnedToCore(
    ldrTask,        // Function to run
    "LDR_Reader",   // Task name
    2048,           // Stack size (bytes)
    NULL,           // Parameters
    1,              // Priority
    NULL,           // Task handle
    1               // Core 1
  );

}

void loop() {
  if (!client.connected()) {
        reconnect();
    }
  client.loop();

  static unsigned long pooling = 0;
  if(millis() - pooling > 10000) {
    pooling = millis();
    publicar();
  }


}
