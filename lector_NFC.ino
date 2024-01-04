// incluir las librerias para la lectura de NFC
#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <NfcAdapter.h>
#include <Arduino.h>
//librerias para conexion con server
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

//Variables para conexion con el servidor
const char *ssid = "NETLIFE-Andres Balarezo";  //NETLIFE-Andres Balarezo, ROYAL HUB
const char *password = "1003410675";           //1003410675, 11_royalhub2023
const char *apiUrl = "https://apitest.proatek.com/public/api/maquinas/beer/activar";
const char *apiUrlSensor = "https://apitest.proatek.com/public/api/sensor/beer/escanear";
const char *apiUrlVolumen = "https://apitest.proatek.com/public/api/sensor/maquina/venta";

//objeto de la libreria NFC
PN532_I2C pn532i2c(Wire);
PN532 nfc(pn532i2c);

//Puerto de salida para control rele
const int control = 18;

// Variables control flujometro
const int sensorPin = 33;
const unsigned long measureInterval = 500UL;
volatile int pulseConter;
const float factorK = 7.663;  //8.363

//Variables calculo volumen
float volumen = 0;
unsigned long t0 = 0;  //millis() del bucle anterior
unsigned long dt = 0;

// variables para el control del tiempo de espera
unsigned long previousMillis = 0;            // Almacena el tiempo anterior
const unsigned long interval = 5000UL;       // Intervalo de tiempo en milisegundos (5 segundos)
unsigned long tiempoInicioTemporizador = 0;  // Variable para almacenar el tiempo de inicio del temporizador

//Funcion para contar los pulsos sel flujometro
void IRAM_ATTR CountPulse() {
  pulseConter++;
}

//Funcion para obtener la frecuancia del flujometro
float GetFrequency() {
  pulseConter = 0;
  unsigned long startTime = millis();  // Registra el tiempo de inicio
  while (millis() - startTime < measureInterval) {
    // Timepo de espera para contar el flujo
  }
  noInterrupts();                                    // Deshabilita las interrupciones
  unsigned long elapsedTime = millis() - startTime;  // Calcula el tiempo transcurrido
  interrupts();                                      // Habilita las interrupciones nuevamente
  return (float)pulseConter * 1000 / elapsedTime;    //return (float)pulseConter * 1000 / elapsedTime;
}

float getvolume(float frequency) {
  float caudal = frequency / factorK;
  t0 = millis() - dt;                                   //calculamos la variación de tiempo
  volumen = volumen + (((caudal / 60) * (t0)) / 1000);  // volumen(L)=caudal(L/s)*tiempo(s) (dt/1000)
  return volumen;
}

// Funcion para imprimir la direccion de la tarjeta NFC
void imprimirValores(uint8_t uid[], uint8_t uidLength) {
  Serial.println("Found a card!");
  for (uint8_t i = 0; i < uidLength; i++) {
    Serial.print("");
    Serial.print(uid[i]);
  }
  Serial.println("");
}

void ConnectWiFi() {
  //Inicializa el objeto wifi y conecta a internet
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WIFI");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Conectando a WiFi...");
  }
  Serial.println("Conectado a la red WiFi");
}

void setup() {
  // iniciar velocidad a 115200
  Serial.begin(115200);

  //Asignamos el pin del flujometro como interrupcion
  pinMode(sensorPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(sensorPin), CountPulse, RISING);

  // Declaramos el pin de control del rele con un pin de salida OUTPUT
  pinMode(control, OUTPUT);
  digitalWrite(control, HIGH);

  //Funcion para conectar al internet
  ConnectWiFi();

  // Crea un objeto JSON
  StaticJsonDocument<32> jsonDocument;
  jsonDocument["id_maquina"] = 1;
  jsonDocument["estado"] = 1;

  // Convierte el objeto JSON a una cadena
  String postData;
  serializeJson(jsonDocument, postData);

  HTTPClient http;
  http.begin(apiUrl);
  http.addHeader("Content-Type", "application/json");  // Usar application/json para JSON

  int httpCode = http.POST(postData);
  String payload = http.getString();

  if (httpCode > 0) {

    http.end();
    Serial.println("Código de respuesta: " + String(httpCode));  //String(httpCode)

    if (httpCode == 201) {

      //Validacion para verificar que le modulo NFC este concetado y funcionando correctamente
      nfc.begin();
      uint32_t versiondata = nfc.getFirmwareVersion();  // obtenemo el firmware del modulo lector de NFC
      if (!versiondata) {
        Serial.print("Didn't find PN53x board");
        // Crea un objeto JSON
        StaticJsonDocument<32> jsonDocument1;
        jsonDocument1["id_maquina"] = 1;
        jsonDocument1["estado"] = 3;

        // Convierte el objeto JSON a una cadena
        String postData1;
        serializeJson(jsonDocument1, postData1);

        //HTTPClient http;
        http.begin(apiUrl);
        http.addHeader("Content-Type", "application/json");  // Usar application/json para JSON

        int httpCode = http.POST(postData1);
        String payload = http.getString();
        http.end();
        Serial.println("Código de respuesta: " + String(httpCode));
        Serial.println("Respuesta del servidor: " + payload);
        while (1)
          ;  // halt
      }

      // Set the max number of retry attempts to read from a card This prevents us from waiting forever for a card, which is the default behaviour of the PN532.
      nfc.setPassiveActivationRetries(0xFF);

      // configure board to read RFID tags
      nfc.SAMConfig();
      Serial.println("Waiting for an ISO14443A card");
    }
  } else {
    http.end();
    Serial.println("Código de respuesta: " + String(httpCode));
    Serial.println("Respuesta del servidor: " + payload);
    Serial.println("Error en la solicitud HTTP");
  }
}

void loop() {

  //Variable para el control de la frecuencia del flujometro
  float frequency = 0;

  //variables banderas estado flujometro
  boolean estadoflujo;
  boolean ingresar = false;

  //variables cambio de estado switch
  int estado = 0;

  //variables para guardar la dirrecion de las tarjetas NFC
  boolean success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  // Wait for an ISO14443A type cards (Mifare, etc.). When one is found, 'uid' will be populated with the UID, and uidLength will indicate, if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength);
  //funcion para imprimir la direccion de las tarjetas

  if (success) {
    imprimirValores(uid, uidLength);
    //Enviar id_maquina y codigo_sensor al back para procesar
    String cadenaResultado = "";
    for (int i = 0; i < uidLength; i++) {
      cadenaResultado += String(uid[i]);
    }
    // Crea un objeto JSON
    StaticJsonDocument<200> jsonDocument2;
    jsonDocument2["id_maquina"] = 1;
    jsonDocument2["codigo_sensor"] = cadenaResultado;

    // Convierte el objeto JSON a una cadena
    String postData;
    serializeJson(jsonDocument2, postData);
    Serial.println(postData);

    // Realiza la solicitud HTTP POST
    HTTPClient http;
    http.begin(apiUrlSensor);
    http.addHeader("Content-Type", "application/json");  // Usar application/json para JSON

    int httpCode = http.POST(postData);

    //Maneja la respuesta del servidor
    if (httpCode > 0) {
      String response = http.getString();
      http.end();
      Serial.println(response);
      Serial.println("Código de respuesta: " + String(httpCode));

      switch (httpCode) {
        case 202:
          ingresar = true;
          break;
        case 203:
          ingresar = true;
          break;
        default:
          ingresar = false;
      }

      //Activar el rele cuando se haya leido una tarjeta NFC
      digitalWrite(control, LOW);
      Serial.println("Rele activado");
      Serial.println(ingresar);
    } else {
      String payload = http.getString();
      http.end();
      Serial.println("Código de respuesta: " + String(httpCode));
      Serial.println("Respuesta del servidor: " + payload);
    }

  } else {
    // PN532 probably timed out waiting for a card
    Serial.println("Timed out waiting for a card");
  }

  //lazo  de control de estados de la maquina
  while (ingresar) {
    Serial.println("Loop While");
    // Realizar una lectura inicial del flujometro
    frequency = GetFrequency();
    frequency = 0;

    //Switch case para manejar los estados de la maquina
    switch (estado) {

      case 0:
        Serial.println("CASE 0");
        dt = millis();
        frequency = GetFrequency();
        volumen = getvolume(frequency);

        if (frequency != 0) {
          estadoflujo = true;
          //Serial.println("frecuencia diferente de cero CASE 0");
        } else {
          estadoflujo = false;
          //Serial.println("frecuencia igual a cero, CASE 0");
          estado = 1;
        }
        while (estadoflujo == true) {
          dt = millis();
          frequency = GetFrequency();
          //dt = millis();
          volumen = getvolume(frequency);
          //Serial.println(" estadoflujo==true CASE 0");
          if (frequency != 0) {
            estadoflujo = true;
            //Serial.println("frecuencia diferente de cero estado flujo verdadero, CASE 0");
          } else {
            estadoflujo = false;
            //Serial.println("frecuencia igual a cero while estado flujo verdadero, CASE 0");
            estado = 1;
          }
          Serial.print(frequency); Serial.println(" HZ");
          Serial.print(volumen); Serial.println(" L");
        }
        break;

      case 1:
        Serial.println("CASE 1");
        while (estadoflujo == false) {
          //Serial.println("WHILE estado flujo == false, CASE 1");
          dt = millis();
          frequency = GetFrequency();
          //dt = millis();
          volumen = getvolume(frequency);
          //Serial.print(frequency); Serial.println(" HZ");

          if (frequency == 0) {

            //Serial.println("caso 1 dentro del while y del if frecuencia 0 ");
            if (tiempoInicioTemporizador == 0) {
              // Inicia el temporizador cuando ingresamos por primera vez al caso 1
              tiempoInicioTemporizador = millis();
            }
            // Calcular el tiempo transcurrido desde que ingresamos al caso 1
            unsigned long tiempoTranscurrido = millis() - tiempoInicioTemporizador;

            if (tiempoTranscurrido >= interval) {
              //Serial.println("Temporizador de 5 segundos alcanzado. Reiniciando temporizador.");
              tiempoInicioTemporizador = 0;  // Reiniciar el temporizador
              estado = 2;
              estadoflujo = true;
            }
          } else {
            estadoflujo = true;
            //Serial.println("frecuencia diferente a cero while estado flujo verdadero");
            estado = 0;
            tiempoInicioTemporizador = 0;  // Reiniciar el temporizador
          }
          Serial.print(frequency); Serial.println(" HZ");
          Serial.print(volumen); Serial.println(" L");
        }

        break;

      case 2:
        Serial.println("caso2");
        digitalWrite(control, HIGH);
        Serial.println("Rele desactivado");

        // Enviar id_maquina, volumuen total, id sensor
        DynamicJsonDocument jsonDocument3(200);
        String cadenaResultado = "";
        for (int i = 0; i < uidLength; i++) {
          cadenaResultado += String(uid[i]);
        }
        jsonDocument3["id_beer"] = cadenaResultado;
        jsonDocument3["total"] = volumen;
        jsonDocument3["id_maquina"] = 1;
        // Convierte el objeto JSON a una cadena
        String postData1;
        serializeJson(jsonDocument3, postData1);
        Serial.println(postData1);
        // Realiza la solicitud HTTP POST
        HTTPClient http;
        http.begin(apiUrlVolumen);
        http.addHeader("Content-Type", "application/json");  // Usar application/json para JSON

        int httpCode = http.POST(postData1);
        String payload = http.getString();
        Serial.println("Código de respuesta: " + String(httpCode));
        Serial.println("Respuesta del servidor: " + payload);

        volumen = 0;
        frequency = 0;
        ingresar = false;
        break;
    }
  }
  frequency = GetFrequency();
  frequency = 0;
  //Serial.print(frequency); Serial.println(" HZ");
}
