/**
 * Este é um exemplo de cliente BLE (Bluetooth Low Energy) para ESP32.
 * Ele procura por dispositivos BLE que oferecem um serviço específico e, 
 * quando encontrado, conecta-se a esse dispositivo. 
 * 
 * Autor: LSA
 * Atualizado por: LSA
 */

#include <arpa/inet.h>
#include <memory.h>
#include "BLEDevice.h"
#include "esp32-hal-log.h"

// UUIDs para o serviço e características BLE
static BLEUUID serviceUUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
static BLEUUID rxCharUUID("6E400002-B5A3-F393-E0A9-E50E24DCCA9E");
static BLEUUID txCharUUID("6E400003-B5A3-F393-E0A9-E50E24DCCA9E");

// Variáveis de controle de conexão BLE
static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pTxCharacteristic;
static BLERemoteCharacteristic* pRxCharacteristic;
static BLEAdvertisedDevice* myDevice;

// Tamanho do array de leituras do sensor ECG
#define LEITURAS 7500
static float ecg[LEITURAS] = { 0 };
static size_t pos = 0;

// Callback para notificações da característica BLE
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
  uint32_t input;
  float voltage;

  // Processa os dados recebidos e os armazena no array ecg
  for (size_t i = 0; i < length / 4; i++) {
    uint32_t val;
    val = ntohl(((uint32_t*)pData)[i]);
    voltage = *(float*)&val;
    ecg[pos++] = voltage;
  }
}

// Callbacks do cliente BLE
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    // Configura o MTU e imprime no monitor serial
    pclient->setMTU(131);
    Serial.print("Client MTU ");
    Serial.println(pclient->getMTU());
  }

  void onDisconnect(BLEClient* pclient) {
    // Define a flag de conexão para falso quando desconectado
    connected = false;
    Serial.println("onDisconnect");
  }
};

// Função para conectar ao servidor BLE
bool connectToServer() {
  Serial.print("Forming a connection to ");
  Serial.println(myDevice->getAddress().toString().c_str());

  // Cria um cliente BLE
  BLEClient* pClient = BLEDevice::createClient();
  Serial.println(" - Created client");

  // Configura os callbacks do cliente BLE
  pClient->setClientCallbacks(new MyClientCallback());

  // Conecta ao servidor BLE remoto
  pClient->connect(myDevice);
  Serial.println(" - Connected to server");

  // Obtém uma referência para o serviço remoto
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our service");

  // Obtém referências para as características no serviço remoto
  pTxCharacteristic = pRemoteService->getCharacteristic(txCharUUID);
  if (pTxCharacteristic == nullptr) {
    Serial.print("Failed to find our TX characteristic UUID: ");
    Serial.println(txCharUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our TX characteristic");

  pRxCharacteristic = pRemoteService->getCharacteristic(rxCharUUID);
  if (pTxCharacteristic == nullptr) {
    Serial.print("Failed to find our RX characteristic UUID: ");
    Serial.println(rxCharUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our RX characteristic");

  // Registra a função de callback para notificações da característica TX
  if (pTxCharacteristic->canNotify())
    pTxCharacteristic->registerForNotify(notifyCallback);
  else
    Serial.println("Could not notify on Tx");

  // Define a flag de conexão para verdadeiro
  connected = true;
  return true;
}

// Callbacks para dispositivos BLE anunciados
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    // Verifica se o dispositivo anunciado oferece o serviço desejado
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      // Interrompe a varredura BLE e configura as flags para conexão e varredura
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;
    }
  }
};

void setup() {
  // Inicializa a comunicação serial
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");

  // Configuração do nível de log para verbose
  Serial.setDebugOutput(true);
  esp_log_level_set("*", ESP_LOG_VERBOSE);

  // Inicializa o dispositivo BLE com o nome "Sensor Central"
  BLEDevice::init("Sensor Central");
  BLEDevice::setMTU(131);  // 128 + 3 (header)

  // Configura o scanner BLE
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
}

void loop() {
  // Se a flag "doConnect" for verdadeira, conecta-se ao servidor BLE
  if (doConnect == true) {
    if (connectToServer()) {
      Serial.println("We are now connected to the BLE Server.");
    } else {
      Serial.println("We have failed to connect to the server; there is nothing more we will do.");
    }
    doConnect = false;
  }

  // Se estiver conectado a um servidor BLE, atualiza a característica com os dados do sensor ECG
  if (connected) {
    if (pos == LEITURAS) {
      // Quando a leitura do ECG está completa, imprime os valores no monitor serial
      pos = 0;
      for (int i = 0; i < LEITURAS; i++) {
        Serial.println(ecg[i], 6);
      }
    }
  } else if (doScan) {
    // Se não estiver conectado, reinicia a varredura BLE
    BLEDevice::getScan()->start(0);
  }
}
