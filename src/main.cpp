#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

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
  distanceCmLocal = distanceCm;
  xSemaphoreGive(mutexServoAngle);

  //client.publish
}

void setup() {
  Serial.begin(115200);

  gpio_set_direction(LDR_PIN, GPIO_MODE_INPUT);
  gpio_set_direction(PIR_PIN, GPIO_MODE_INPUT);

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
