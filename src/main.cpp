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

const char *ssid = "iPhone (7)";
const char *password = "12345678";
const char *mqtt_server = "broker.emqx.io";

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

const bool USE_MOCK_SENSORS = true;
unsigned long mockStep = 0;

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

void updateMockSensorReadings()
{
  if (!USE_MOCK_SENSORS)
  {
    return;
  }

  unsigned long step = mockStep++;

  xSemaphoreTake(mutexStatus, pdMS_TO_TICKS(100));
  status = true;
  xSemaphoreGive(mutexStatus);

  xSemaphoreTake(mutexDistance, pdMS_TO_TICKS(100));
  distanceCm = 15.5f + (step % 10) * 2.25f;
  xSemaphoreGive(mutexDistance);

  xSemaphoreTake(mutexServoAngle, pdMS_TO_TICKS(100));
  servoAngle = (step * 30) % 181;
  xSemaphoreGive(mutexServoAngle);

  xSemaphoreTake(mutexPresenca, pdMS_TO_TICKS(100));
  presenca = (step % 2 == 0);
  xSemaphoreGive(mutexPresenca);

  xSemaphoreTake(mutexLuminosidade, pdMS_TO_TICKS(100));
  luminosidade = 20 + (step % 5) * 15;
  xSemaphoreGive(mutexLuminosidade);

  xSemaphoreTake(mutexTemperatura, pdMS_TO_TICKS(100));
  temperatura = 24.0f + (step % 6) * 0.7f;
  xSemaphoreGive(mutexTemperatura);

  xSemaphoreTake(mutexAlarme, pdMS_TO_TICKS(100));
  alarme = (step % 3 == 0);
  xSemaphoreGive(mutexAlarme);

  xSemaphoreTake(mutexServo, pdMS_TO_TICKS(100));
  servo = (step % 2 == 0);
  xSemaphoreGive(mutexServo);

  xSemaphoreTake(mutexArmar, pdMS_TO_TICKS(100));
  armar = (step % 4 < 2);
  xSemaphoreGive(mutexArmar);

  xSemaphoreTake(mutexBuzzer, pdMS_TO_TICKS(100));
  buzzer = (step % 5 == 0);
  xSemaphoreGive(mutexBuzzer);
}

WiFiClient espClient;
PubSubClient client(espClient);

String getUniqueClientId()
{
  uint64_t chipId = ESP.getEfuseMac();
  char clientId[50];
  snprintf(clientId, sizeof(clientId), "esp32_%04X%08X", (uint16_t)(chipId >> 32), (uint32_t)chipId);
  return String(clientId);
}

void reconnect()
{
  if (!client.connected())
  {
    static unsigned long lastReconnectAttempt = 0;
    unsigned long now = millis();

    if (now - lastReconnectAttempt > 5000)
    {
      lastReconnectAttempt = now;
      String clientId = getUniqueClientId();
      Serial.print("[MQTT] Tentando conexao com ID: ");
      Serial.println(clientId);

      if (client.connect(clientId.c_str()))
      {
        Serial.println("[MQTT] Conectado com sucesso!");
        client.subscribe(topico_servo);
        client.subscribe(topico_armar);
        client.subscribe(topico_buzzer);
        client.subscribe(topico_config);
        Serial.println("[MQTT] Inscrito nos topicos de comando");
      }
      else
      {
        Serial.print("[MQTT] Falha - Codigo: ");
        Serial.println(client.state());
      }
    }
  }
}

void conectaWifi()
{
  Serial.println("[WiFi] Conectando a rede: " + String(ssid));
  WiFi.begin(ssid, password);
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20)
  {
    delay(500);
    Serial.print(".");
    tentativas++;
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\n[WiFi] Conectado com sucesso! IP: " + WiFi.localIP().toString());
  }
  else
  {
    Serial.println("\n[WiFi] Falha na conexao!");
  }
}

void publicar()
{
  if (!client.connected())
  {
    Serial.println("[PUBLICAR] Cliente nao conectado, pulando publicacao");
    return;
  }

  updateMockSensorReadings();

  Serial.println("[PUBLICAR] Enviando dados...");
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
  client.publish(topico_presenca, presencaLocal ? "detectada" : "vazia");
  client.publish(topico_luminosidade, std::to_string(luminosidadeLocal).c_str());
  client.publish(topico_temperatura, std::to_string(temperaturaLocal).c_str());
  client.publish(topico_alarme, alarmeLocal ? "ativado" : "desativado");
  Serial.println("[PUBLICAR] Dados enviados com sucesso!");
}

void ldrTask(void *pvParameters)
{
  if (USE_MOCK_SENSORS)
  {
    for (;;)
    {
      updateMockSensorReadings();
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }

  int rawValue = analogRead(LDR_PIN);
  int lightPercent = map(rawValue, 0, 4095, 0, 100);
  xSemaphoreTake(mutexLuminosidade, pdMS_TO_TICKS(100));
  luminosidade = lightPercent;
  xSemaphoreGive(mutexLuminosidade);
  vTaskDelay(pdMS_TO_TICKS(5000));
}
void detectPresence()
{
  xSemaphoreTake(mutexPresenca, pdMS_TO_TICKS(100));
  presenca = true;
  xSemaphoreGive(mutexPresenca);
}

void detectInvasionTask(void *pvParameters)
{
  for (;;)
  { // Tasks precisam de um loop infinito externo
    int lightLocal;

    // 1. Busca o valor atualizado da luminosidade
    if (xSemaphoreTake(mutexLuminosidade, pdMS_TO_TICKS(10)))
    {
      lightLocal = luminosidade;
      xSemaphoreGive(mutexLuminosidade);
    }

    bool presencaLocal;

    if (xSemaphoreTake(mutexPresenca, pdMS_TO_TICKS(10)))
    {
      presencaLocal = presenca;
      xSemaphoreGive(mutexPresenca);
    }

    // 2. Verifica a condição de invasão
    if (lightLocal > 60 && presencaLocal)
    {
      // Alerta visual/sonoro (substituí delay por vTaskDelay)
      gpio_set_level(LED_PIN, 1);
      gpio_set_level(BUZZER_PIN, 1);
      vTaskDelay(pdMS_TO_TICKS(500));

      gpio_set_level(LED_PIN, 0);
      gpio_set_level(BUZZER_PIN, 0);
      vTaskDelay(pdMS_TO_TICKS(500));

      // 3. Atualiza o status de presença via Mutex
      if (xSemaphoreTake(mutexPresenca, pdMS_TO_TICKS(10)))
      {
        presenca = false;
        xSemaphoreGive(mutexPresenca);
      }
    }
    else
    {
      // Pequena espera se não houver invasão para não estressar a CPU
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}

void setup()
{
  Serial.begin(115200);

  gpio_set_direction(LDR_PIN, GPIO_MODE_INPUT);
  gpio_set_direction(PIR_PIN, GPIO_MODE_INPUT);
  gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);
  gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

  // Inicializando cada Mutex
  mutexStatus = xSemaphoreCreateMutex();
  mutexDistance = xSemaphoreCreateMutex();
  mutexServoAngle = xSemaphoreCreateMutex();
  mutexPresenca = xSemaphoreCreateMutex();
  mutexLuminosidade = xSemaphoreCreateMutex();
  mutexTemperatura = xSemaphoreCreateMutex();
  mutexAlarme = xSemaphoreCreateMutex();
  mutexServo = xSemaphoreCreateMutex();
  mutexArmar = xSemaphoreCreateMutex();
  mutexBuzzer = xSemaphoreCreateMutex();

  // Verificação simples
  if (mutexStatus == NULL)
  {
    Serial.println("Erro ao criar semáforos");
  }
  conectaWifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback([](char *topic, byte *payload, unsigned int length)
                     {
    Serial.print("[MQTT] Mensagem em: ");
    Serial.print(topic);
    Serial.print(" = ");
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println(); });

  attachInterrupt(digitalPinToInterrupt(PIR_PIN), detectPresence, RISING);

  xTaskCreatePinnedToCore(
      ldrTask,      // Function to run
      "LDR_Reader", // Task name
      2048,         // Stack size (bytes)
      NULL,         // Parameters
      1,            // Priority
      NULL,         // Task handle
      1             // Core 1
  );
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  static unsigned long pooling = 0;
  if (millis() - pooling > 10000)
  {
    pooling = millis();
    publicar();
  }
}
