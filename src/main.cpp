#include <Arduino.h>
#include <DHTesp.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <string>

#define MQTT_ID "esp1_pub"
#define topico_status "nickfer/status"
#define topico_ultrassom "nickfer/ultrassom"
#define topico_presenca "nickfer/presenca"
#define topico_luminosidade "nickfer/luminosidade"
#define topico_temperatura "nickfer/temperatura"
#define topico_umidade "nickfer/umidade"
#define topico_alarme "nickfer/alarme"

#define topico_servo "nickfer/comando/servo"
#define topico_armar "nickfer/comando/armar"
#define topico_buzzer "nickfer/comando/buzzer"
#define topico_config "nickfer/comando/config"

#define SOUND_SPEED 0.034
#define SERVO_MIN_US 500
#define SERVO_MAX_US 2400
#define SERVO_MIN_ANGLE 5
#define SERVO_MAX_ANGLE 175
#define SERVO_STEP_DEGREES 10
#define SERVO_STEP_DELAY_MS 1000
#define HC_SR04_TIMEOUT_US 30000

const gpio_num_t LDR_PIN = GPIO_NUM_35;
const gpio_num_t PIR_PIN = GPIO_NUM_32;
const gpio_num_t BUZZER_PIN = GPIO_NUM_33;
const gpio_num_t LED_PIN = GPIO_NUM_25;
const gpio_num_t SERVO_PIN = GPIO_NUM_12;
const gpio_num_t HC_SR04_TRIG_PIN = GPIO_NUM_18;
const gpio_num_t HC_SR04_ECHO_PIN = GPIO_NUM_19;
const gpio_num_t DHT22_PIN = GPIO_NUM_4;

const char *ssid = "iPhone (7)";
const char *password = "12345678";
const char *mqtt_server = "broker.emqx.io";

bool status;
float distanceCm;
int servoAngle;
bool presenca;
int luminosidade;
float temperatura;
float umidade;
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
SemaphoreHandle_t mutexUmidade;
SemaphoreHandle_t mutexAlarme;

SemaphoreHandle_t mutexServo;
SemaphoreHandle_t mutexArmar;
SemaphoreHandle_t mutexBuzzer;

DHTesp dht;
Servo servoMotor;

void writeServoAngle(int angle)
{
  angle = constrain(angle, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
  servoMotor.write(angle);
}

float readUltrasonicDistanceCm()
{
  digitalWrite(HC_SR04_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(HC_SR04_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(HC_SR04_TRIG_PIN, LOW);

  unsigned long duration = pulseIn(HC_SR04_ECHO_PIN, HIGH, HC_SR04_TIMEOUT_US);
  if (duration == 0)
  {
    return -1.0f;
  }
  float val = (duration * SOUND_SPEED) / 2.0f;
  Serial.printf("[ULTRASSOM] Leu %f\n", val);
  return val;
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

  Serial.println("[PUBLICAR] Enviando dados...");
  bool statusLocal;

  bool presencaLocal;
  int luminosidadeLocal;
  float temperaturaLocal;
  float umidadeLocal;
  bool alarmeLocal;

  xSemaphoreTake(mutexStatus, pdMS_TO_TICKS(100));
  statusLocal = status;
  xSemaphoreGive(mutexStatus);

  xSemaphoreTake(mutexPresenca, pdMS_TO_TICKS(100));
  presencaLocal = presenca;
  xSemaphoreGive(mutexPresenca);
  xSemaphoreTake(mutexLuminosidade, pdMS_TO_TICKS(100));
  luminosidadeLocal = luminosidade;
  xSemaphoreGive(mutexLuminosidade);
  xSemaphoreTake(mutexTemperatura, pdMS_TO_TICKS(100));
  temperaturaLocal = temperatura;
  xSemaphoreGive(mutexTemperatura);
  xSemaphoreTake(mutexUmidade, pdMS_TO_TICKS(100));
  umidadeLocal = umidade;
  xSemaphoreGive(mutexUmidade);
  xSemaphoreTake(mutexAlarme, pdMS_TO_TICKS(100));
  alarmeLocal = alarme;
  xSemaphoreGive(mutexAlarme);

  std::string statusJson = std::string("{\"status\":\"") + (statusLocal ? "online" : "offline") + "\"}";
  std::string presencaJson = std::string("{\"presenca\":\"") + (presencaLocal ? "detectada" : "vazia") + "\"}";
  std::string alarmeJson = std::string("{\"alarme\":\"") + (alarmeLocal ? "ativado" : "desativado") + "\"}";

  client.publish(topico_status, statusJson.c_str());
  client.publish(topico_presenca, presencaJson.c_str());
  client.publish(topico_luminosidade, std::to_string(luminosidadeLocal).c_str());
  client.publish(topico_temperatura, std::to_string(temperaturaLocal).c_str());
  client.publish(topico_umidade, std::to_string(umidadeLocal).c_str());
  client.publish(topico_alarme, alarmeJson.c_str());
  Serial.println("[PUBLICAR] Dados enviados com sucesso!");
}

void ldrTask(void *pvParameters)
{
  for (;;)
  {
    int rawValue = analogRead(LDR_PIN);
    int lightPercent = map(rawValue, 0, 4095, 100, 0);
    xSemaphoreTake(mutexLuminosidade, pdMS_TO_TICKS(100));
    luminosidade = lightPercent;
    xSemaphoreGive(mutexLuminosidade);
    Serial.printf("[LDR] raw=%d percent=%d\n", rawValue, lightPercent);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void pirTask(void *pvParameters)
{
  for (;;)
  {
    bool pirDetected = (digitalRead(PIR_PIN) == HIGH);
    xSemaphoreTake(mutexPresenca, pdMS_TO_TICKS(100));
    presenca = pirDetected;
    xSemaphoreGive(mutexPresenca);
    Serial.printf("[PIR] detected=%d\n", pirDetected ? 1 : 0);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void dhtTask(void *pvParameters)
{
  for (;;)
  {
    TempAndHumidity data = dht.getTempAndHumidity();

    if (!isnan(data.temperature))
    {
      xSemaphoreTake(mutexTemperatura, pdMS_TO_TICKS(100));
      temperatura = data.temperature;
      xSemaphoreGive(mutexTemperatura);
    }

    if (!isnan(data.humidity))
    {
      xSemaphoreTake(mutexUmidade, pdMS_TO_TICKS(100));
      umidade = data.humidity;
      xSemaphoreGive(mutexUmidade);
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}
void publish_ultrasound()
{
  float distanceCmLocal;
  int servoAngleLocal;

  xSemaphoreTake(mutexDistance, pdMS_TO_TICKS(100));
  distanceCmLocal = distanceCm;
  xSemaphoreGive(mutexDistance);
  xSemaphoreTake(mutexServoAngle, pdMS_TO_TICKS(100));
  servoAngleLocal = servoAngle;
  xSemaphoreGive(mutexServoAngle);

  std::string resultado = std::string("{\"distance_cm\":") + std::to_string(distanceCmLocal) +
                          ",\"servo_angle\":" + std::to_string(servoAngleLocal) + "}";
  client.publish(topico_ultrassom, resultado.c_str());
}

void servoUltrasonicTask(void *pvParameters)
{
  for (;;)
  {
    for (int angle = SERVO_MIN_ANGLE; angle <= SERVO_MAX_ANGLE; angle += SERVO_STEP_DEGREES)
    {
      writeServoAngle(angle);

      float measuredDistance = readUltrasonicDistanceCm();

      xSemaphoreTake(mutexServoAngle, pdMS_TO_TICKS(100));
      servoAngle = angle;
      xSemaphoreGive(mutexServoAngle);

      if (measuredDistance >= 0.0f)
      {
        xSemaphoreTake(mutexDistance, pdMS_TO_TICKS(100));
        distanceCm = measuredDistance;
        xSemaphoreGive(mutexDistance);
        Serial.printf("[SERVO] angle=%d distance=%.2fcm\n", angle, measuredDistance);
      }
      else
      {
        Serial.printf("[SERVO] angle=%d distance=timeout\n", angle);
      }

      publish_ultrasound();

      vTaskDelay(pdMS_TO_TICKS(SERVO_STEP_DELAY_MS));
    }

    for (int angle = SERVO_MAX_ANGLE; angle >= SERVO_MIN_ANGLE; angle -= SERVO_STEP_DEGREES)
    {
      writeServoAngle(angle);

      float measuredDistance = readUltrasonicDistanceCm();

      xSemaphoreTake(mutexServoAngle, pdMS_TO_TICKS(100));
      servoAngle = angle;
      xSemaphoreGive(mutexServoAngle);

      if (measuredDistance >= 0.0f)
      {
        xSemaphoreTake(mutexDistance, pdMS_TO_TICKS(100));
        distanceCm = measuredDistance;
        xSemaphoreGive(mutexDistance);
        Serial.printf("[SERVO] angle=%d distance=%.2fcm\n", angle, measuredDistance);
      }
      else
      {
        Serial.printf("[SERVO] angle=%d distance=timeout\n", angle);
      }

      publish_ultrasound();

      vTaskDelay(pdMS_TO_TICKS(SERVO_STEP_DELAY_MS));
    }
  }
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
      Serial.printf("[INVASION] light=%d presenca=%d\n", lightLocal, presencaLocal ? 1 : 0);
    }

    bool alarmActive = (lightLocal > 60 && presencaLocal);
    if (xSemaphoreTake(mutexAlarme, pdMS_TO_TICKS(10)))
    {
      alarme = alarmActive;
      xSemaphoreGive(mutexAlarme);
    }

    // 2. Verifica a condição de invasão
    if (alarmActive)
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

      if (xSemaphoreTake(mutexAlarme, pdMS_TO_TICKS(10)))
      {
        alarme = false;
        xSemaphoreGive(mutexAlarme);
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
  gpio_set_direction(HC_SR04_TRIG_PIN, GPIO_MODE_OUTPUT);
  gpio_set_direction(HC_SR04_ECHO_PIN, GPIO_MODE_INPUT);
  dht.setup(DHT22_PIN, DHTesp::DHT22);
  gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);
  gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
  servoMotor.setPeriodHertz(50);
  servoMotor.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);
  writeServoAngle(0);

  // Inicializando cada Mutex
  mutexStatus = xSemaphoreCreateMutex();
  mutexDistance = xSemaphoreCreateMutex();
  mutexServoAngle = xSemaphoreCreateMutex();
  mutexPresenca = xSemaphoreCreateMutex();
  mutexLuminosidade = xSemaphoreCreateMutex();
  mutexTemperatura = xSemaphoreCreateMutex();
  mutexUmidade = xSemaphoreCreateMutex();
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

  xSemaphoreTake(mutexPresenca, pdMS_TO_TICKS(100));
  presenca = (digitalRead(PIR_PIN) == HIGH);
  xSemaphoreGive(mutexPresenca);

  TempAndHumidity initialDhtReading = dht.getTempAndHumidity();
  if (!isnan(initialDhtReading.temperature))
  {
    xSemaphoreTake(mutexTemperatura, pdMS_TO_TICKS(100));
    temperatura = initialDhtReading.temperature;
    xSemaphoreGive(mutexTemperatura);
  }
  if (!isnan(initialDhtReading.humidity))
  {
    xSemaphoreTake(mutexUmidade, pdMS_TO_TICKS(100));
    umidade = initialDhtReading.humidity;
    xSemaphoreGive(mutexUmidade);
  }

  xTaskCreatePinnedToCore(
      ldrTask,      // Function to run
      "LDR_Reader", // Task name
      2048,         // Stack size (bytes)
      NULL,         // Parameters
      1,            // Priority
      NULL,         // Task handle
      1             // Core 1
  );

  xTaskCreatePinnedToCore(
      pirTask,
      "PIR_Reader",
      2048,
      NULL,
      1,
      NULL,
      1);

  xTaskCreatePinnedToCore(
      dhtTask,
      "DHT22_Reader",
      2048,
      NULL,
      1,
      NULL,
      1);

  xTaskCreatePinnedToCore(
      servoUltrasonicTask,
      "Servo_Ultrasonic",
      3072,
      NULL,
      1,
      NULL,
      1);

  xTaskCreatePinnedToCore(
      detectInvasionTask,
      "Detect_Invasion",
      2048,
      NULL,
      1,
      NULL,
      1);
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
