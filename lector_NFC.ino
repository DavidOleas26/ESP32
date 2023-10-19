  // incluir las librerias para la lectura de NFC 
  // #include <Wire.h>
  #include <PN532_I2C.h>
  #include <PN532.h>
  #include <NfcAdapter.h>
  #include <Arduino.h>
  PN532_I2C pn532i2c(Wire);
  PN532 nfc(pn532i2c);

  //Variables para control rele 
   const int control = 18;

  // Variables control flujometro
  const int sensorPin = 33;
  const int measureInterval = 500;
  volatile int pulseConter; 
  const float factorK = 7.663;//8.363

  //Variables calculo volumen
  float volumen=0;
  long dt=0; //variación de tiempo por cada bucle
  long t0=0; //millis() del bucle anterior

  // variables para el control del tiempo de espera
  unsigned long previousMillis = 0;  // Almacena el tiempo anterior
  const long interval = 5000;        // Intervalo de tiempo en milisegundos (5 segundos)
  unsigned long tiempoInicioTemporizador = 0; // Variable para almacenar el tiempo de inicio del temporizador
  
  //Funcion para contar los pulsos sel flujometro 
  void ISRCountPulse()
  {
    pulseConter++;
  }

  //Funcion para obtener la frecuancia del flujometro
  float GetFrequency()
  {
    //t0=millis();
    pulseConter = 0;
    unsigned long startTime = millis();  // Registra el tiempo de inicio
    while (millis() - startTime < measureInterval) {
      // Timepo de espera para contar el flujo 
    }
    noInterrupts();  // Deshabilita las interrupciones
    unsigned long elapsedTime = millis() - startTime;  // Calcula el tiempo transcurrido
    interrupts();  // Habilita las interrupciones nuevamente
    return (float)pulseConter * 1000 / elapsedTime;
  }

  float getvolume (float frequency)
  {
    float caudal = frequency/factorK;
    t0=millis()-dt; //calculamos la variación de tiempo
    volumen=volumen+(caudal/60)*(t0); // volumen(L)=caudal(L/s)*tiempo(s) (dt/1000)
//    t0=millis();

//    volumen += caudal / 60 * (millis() - t0) ;/// 1000.0;
//    t0 = millis();
    return volumen;
  }

// Funcion para imprimir la direccion de la tarjeta NFC  
  void imprimirValores (uint8_t uid[], uint8_t uidLength )
  { 
    Serial.println("Found a card!");
    //Serial.print("UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
    //Serial.print("UID Value: ");
    for (uint8_t i=0; i < uidLength; i++) 
    {
      Serial.print(" ");Serial.print(uid[i], DEC); 
    }
    Serial.println("");
  }
 
  void setup() 
  {
    // iniciar velocidad a 115200 
    Serial.begin(115200);

    // Declaramos el pin de control del rele con un pin de salida OUTPUT 
    pinMode(control, OUTPUT);
    digitalWrite(control, HIGH);

    //Asignamos el pin del flujometro como interrupcion
    attachInterrupt(digitalPinToInterrupt(sensorPin), ISRCountPulse, RISING);
  
    //Validacion para verificar que le modulo NFC este concetado y funcionando correctamente 
    nfc.begin();
    uint32_t versiondata = nfc.getFirmwareVersion(); // obtenemo el firmware del modulo lector de NFC
    if (! versiondata) {
      Serial.print("Didn't find PN53x board"); 
      while (1); // halt
  }
  
    // Set the max number of retry attempts to read from a card This prevents us from waiting forever for a card, which is the default behaviour of the PN532.
    nfc.setPassiveActivationRetries(0xFF);
  
    // configure board to read RFID tags
    nfc.SAMConfig();
    Serial.println("Waiting for an ISO14443A card");

    //medicion de tiempo para calcular volumen 
    //t0 = millis();
  }

  void loop() {
      
    //Variable para el control de la frecuencia del flujometro
    float frequency = 0;

    //variables banderas estado flujometro
    boolean estadoflujo;

    //variables cambio de estado switch
    int estado = 0;

    //variables para guardar la dirrecion de las tarjetas NFC 
    boolean success;
    uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
    uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
    // Wait for an ISO14443A type cards (Mifare, etc.). When one is found, 'uid' will be populated with the UID, and uidLength will indicate, if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
    success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength);
    //funcion para imprimir la direccion de las tarjetas

    if(success){
      imprimirValores(uid,uidLength); 
      //Activar el rele cuando se haya leido una tarjeta NFC 
      digitalWrite(control, LOW);
      Serial.println("Rele activado"); 
      
    }else{
      // PN532 probably timed out waiting for a card
      Serial.println("Timed out waiting for a card");      
    }
    
//    frequency = GetFrequency();
//    Serial.print(frequency); Serial.println(" HZ"); 
    //lazo  de control de estados de la maquina
    while (success) 
    {
      Serial.println("Loop While"); 
      // Realizar una lectura inicial del flujometro
      frequency = GetFrequency(); 
      frequency=0;
//      Serial.print(frequency); Serial.println(" HZ"); 
//      volumen = getvolume(frequency);
//      Serial.print(volumen); Serial.println(" L");
      
      //Switch case para manejar los estados de la maquina
      switch (estado) {
      case 0:

          Serial.println("CASE 0");
          dt = millis();
          frequency = GetFrequency();
//          dt = millis();
          volumen = getvolume(frequency);
//          Serial.print(frequency); Serial.println(" HZ"); 
            
          if(frequency !=0){
             estadoflujo= true;
//             Serial.println("frecuencia diferente de cero CASE 0");    
          }else{
             estadoflujo= false;
//             Serial.println("frecuencia igual a cero, CASE 0");
             estado= 1;
          }
          while(estadoflujo==true){
             dt = millis();
             frequency = GetFrequency();
//             dt = millis();
             volumen = getvolume(frequency);
//             Serial.println(" estadoflujo==true CASE 0");  
             if(frequency !=0){
                estadoflujo= true;
//                Serial.println("frecuencia diferente de cero estado flujo verdadero, CASE 0");    
             }else{
                estadoflujo= false;
//                Serial.println("frecuencia igual a cero while estado flujo verdadero, CASE 0");
                estado= 1;
             }                    
                Serial.print(frequency); Serial.println(" HZ"); 
                Serial.print(volumen); Serial.println(" L"); 
             }         
          break;

      case 1:

            Serial.println("CASE 1");
            while(estadoflujo==false){
            
//                Serial.println("WHILE estado flujo == false, CASE 1");
                dt=millis();
                frequency = GetFrequency();
//                dt = millis();
                volumen = getvolume(frequency);
//                Serial.print(frequency); Serial.println(" HZ"); 
                  
                if(frequency ==0){
                  
//                    Serial.println("caso 1 dentro del while y del if frecuencia 0 ");
                    if (tiempoInicioTemporizador == 0) {
                        // Inicia el temporizador cuando ingresamos por primera vez al caso 1
                        tiempoInicioTemporizador = millis();
                    }   
                    // Calcular el tiempo transcurrido desde que ingresamos al caso 1
                    unsigned long tiempoTranscurrido = millis() - tiempoInicioTemporizador;
 
                    if (tiempoTranscurrido >= interval) {
//                        Serial.println("Temporizador de 5 segundos alcanzado. Reiniciando temporizador.");
                        tiempoInicioTemporizador = 0; // Reiniciar el temporizador
                        estado=2; 
                        estadoflujo=true;   
                    }
                }else{
                    estadoflujo= true;
//                    Serial.println("frecuencia diferente a cero while estado flujo verdadero");
                    estado= 0;
                    tiempoInicioTemporizador = 0;  // Reiniciar el temporizador 
                }                    
                Serial.print(frequency); Serial.println(" HZ");  
                Serial.print(volumen); Serial.println(" L");
             }  

        break;
        
      case 2:
           Serial.println("caso2");
           digitalWrite(control,HIGH);
           Serial.println("Rele desactivado");
           volumen = 0; 
           frequency=0;   
           success=false;        
        break;
    }
    }
    frequency = GetFrequency();
    frequency = 0;   
//    Serial.print(frequency); Serial.println(" HZ"); 
  }
