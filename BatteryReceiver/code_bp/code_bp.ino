#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>

const char* ssid = "HONOR X8b"; // СЮДА НАЗВАНИЕ СВОЕЙ СЕТИ
const char* password = "tttttttt"; // СЮДА ПАРОЛЬ ОТ ЭТОЙ СЕТИ
const char* mqtt_server = "bigfollower347.cloud.shiftr.io";
const char* mqtt_username = "bigfollower347";   // Логин Shiftr.io
const char* mqtt_password = "S78zHwadaRjtzqjN";  // Пароль Shiftr.io

WiFiClient espClient;
PubSubClient client(espClient);

#define D5 14
#include <array>

const int IR_SENSOR_PIN = D5;

unsigned long obstacleTimeBattery[] = {0, 0}; // Время, в течение которого обнаружено препятствие для каждого типа батареек 
unsigned long lastDetectionTime = 0; // Время последнего обнаружения 

struct BatteryTimeRange { 
    unsigned long min_time; 
    unsigned long max_time; 
}; 

const std::array<BatteryTimeRange, 4> OBSTACLE_TIME_THRESHOLD = {{ 
    {5, 50}, //>АА
    {51, 92}, //АА
    {93, 140}, //ААА
    {141, 500}, //<ААА  
}}; 

uint32_t t_begin = 0;
uint32_t t_end = 0;
uint32_t t_buf;
bool state = 0;
bool needSave = 0;

int value1 = 0; 
int value2 = 0; 
int value3 = 0; 
int value4 = 0; 

unsigned long lastChangeTime = 0; // Время последнего изменения состояния
const unsigned long sleepThreshold = 5 * 60 * 1000; // 5 минут в миллисекундах

int previousValue1 = 0; 
int previousValue2 = 0; 
int previousValue3 = 0; 
int previousValue4 = 0;

void pin_high() {
  uint32_t t = millis();
  if (t - t_end > 100) t_begin = t;
  needSave = 0;
  lastChangeTime = millis(); // Обновление времени последнего изменения
}

void pin_low() {
  t_end = millis();
  needSave = 1;
  lastChangeTime = millis(); // Обновление времени последнего изменения
}

void save() {
  uint32_t t = t_end - t_begin;
  Serial.println(t);
  for (int i = 0; i < 4; i++) {
    if (t >= OBSTACLE_TIME_THRESHOLD[i].min_time &&  
        t <= OBSTACLE_TIME_THRESHOLD[i].max_time) { 
      // Действия для i-го типа батарейки 
      if (i == 0) { 
          value1++; 
          EEPROM.begin(16);
          EEPROM.put(0, value1); // Сохранение value1 в EEPROM
          EEPROM.commit();
          EEPROM.end();
      } else if (i == 1) { 
          value2++; 
          EEPROM.begin(16);
          EEPROM.put(4, value2); // Сохранение value2 в EEPROM
          EEPROM.commit();
          EEPROM.end();
      } else if (i == 2) { 
          value3++; 
          EEPROM.begin(16);
          EEPROM.put(8, value3); // Сохранение value3 в EEPROM
          EEPROM.commit();
          EEPROM.end();
      } else if (i == 3) { 
          value4++; 
          EEPROM.begin(16);
          EEPROM.put(12, value4); // Сохранение value4 в EEPROM
          EEPROM.commit();
          EEPROM.end();
      }
      break;
    } 
  }
  needSave = 0;
}

void goToDeepSleep() {
  Serial.println("Переход в режим глубокого сна на 5 минут");
  ESP.deepSleep(5 * 60 * 1000000); // Сон на 5 минут
}

void resetValues() {
  EEPROM.begin(16);

  value1 = 0;
  value2 = 0;
  value3 = 0;
  value4 = 0;

  EEPROM.put(0, value1);
  EEPROM.put(4, value2);
  EEPROM.put(8, value3);
  EEPROM.put(12, value4);
  EEPROM.commit();
  EEPROM.end(); // Завершение работы с EEPROM

  Serial.println("Значения обнулены и сохранены в EEPROM");
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Сообщение получено [");
  Serial.print(topic);
  Serial.print("]: ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  String incoming = "";
  for (unsigned int i = 0; i < length; i++) {
    incoming += (char)payload[i];
  }

  // Если сообщение "reset", обнуляем значения и очищаем EEPROM
  if (incoming == "reset") {
    resetValues();
    incoming = "";
  }
}

void setup() {
  Serial.begin(9600);
  
  EEPROM.begin(16); // Инициализация EEPROM с 16 байтами пространства

  // Чтение начальных значений из EEPROM
  EEPROM.get(0, value1);
  EEPROM.get(4, value2);
  EEPROM.get(8, value3);
  EEPROM.get(12, value4);
  EEPROM.end(); // Завершение работы с EEPROM после чтения данных
  
  // Подключение к Wi-Fi
  setup_wifi();
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback); // Установка callback-функции для обработки входящих сообщений

  t_begin = 0;
  t_end = 0;
  state = 0;
  needSave = 0;
  lastChangeTime = millis(); // Инициализация времени последнего изменения

  previousValue1 = value1; 
  previousValue2 = value2; 
  previousValue3 = value3; 
  previousValue4 = value4;
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266Client", mqtt_username, mqtt_password)) {
      Serial.println("connected");
      client.subscribe("batteryTopic"); // Подписка на топик
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect(); // Переподключаемся, если не подключены
  }
  client.loop(); // Обработка входящих сообщений
  
  unsigned long now = millis();
  // Отправка сообщения каждые 5 секунд
  static unsigned long lastMsg = 0;
  if (now - lastMsg > 5000) { // Увеличил интервал отправки до 5 секунд
    lastMsg = now;
    String msg = String(value1) + "," + String(value2) + "," + String(value3) + "," + String(value4);
    Serial.println(msg);
    client.publish("batteryTopic", msg.c_str());
  }

  if (needSave && millis() - t_end > 500) save();

  if (digitalRead(IR_SENSOR_PIN) != state) {
    state = !state;
    if (state) pin_low();
    else pin_high();
  }

  // Проверка на бездействие и переход в режим глубокого сна
  if (now - lastChangeTime > sleepThreshold) {
    // Если значения счетчиков не изменились за последние 5 минут, переход в глубокий сон
    if (value1 == previousValue1 && value2 == previousValue2 && value3 == previousValue3 && value4 == previousValue4) {
      goToDeepSleep();
    } else {
      // Обновляем предыдущие значения и время последнего изменения
      previousValue1 = value1;
      previousValue2 = value2;
      previousValue3 = value3;
      previousValue4 = value4;
      lastChangeTime = now;
    }
  }
}
