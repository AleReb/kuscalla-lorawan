/*
  Documentacion de proyecto kuscalla lorawan:
  desarrollado por  A.R. 2022.
  Configurar en lorawan el arduino como un modulo generico con lora version 1.0.2
  por ahora usar por defecto la codificacion de json
  function Decoder(bytes, port) {
  var result = "";
  for (var i = 0; i < bytes.length; i++) {
  result += String.fromCharCode(parseInt(bytes[i]));
  }
  return { payload: result, };
  }

  es muy importante configurar correctamente la region modem.begin(AU915_TTN) --- asi usara la mascara de subred para lorawan eso hace que
  la conexion sea casi indmediata, usamos lorawan mkrwan_v2 firmare. el divisor de voltaje es en 100k 100k sin embargo hay que configurar algo
  mas para generar el divisor de voltaje para 12 volts, se podria registrar el valor de la bateria solar y el valor de la bateria interna, se considera
  configurar algunos valores como la cantidad de veces que se guardo y la cantidad de veces que se envio asi se puede hacer una comparativa de los valores perdidos
  se hace un reboot al no poder recivir mensaje de recepcion de datos por 5 ocaciones concecutivas
  revisar esto: para mejorar: el factor de distancia y disminuir el tiempo en el aire del mensaje mientras mas corto sea menos posibilidades hay de que no llegue
  el mensaje:
  function decodeUplink(input) {
  var data = {};
  data.temp = ((input.bytes[0] << 8) + input.bytes[1]) / 100;
  data.hum = ((input.bytes[2] << 8) + input.bytes[3]) / 100;
  data.nivel = ((input.bytes[4] << 8) + input.bytes[5]) / 1000;
  data.envios = ((input.bytes[6] << 8) + input.bytes[7]);
  var warnings = [];

  return {
   data: data,
   warnings: warnings
  };
  }

  DataRate  Modulation  SF  BW  bit/s
  0   LoRa  12  125   250
  1   LoRa  11  125   440
  2   LoRa  10  125   980
  3   LoRa  9   125   1'760
  4   LoRa  8   125   3'125
  5   LoRa  7   125   5'470
  6   LoRa  7   250   11'000

  revisar codificacion  https://www.thethingsnetwork.org/forum/t/mkrwan-h-sending-a-byte-to-tts/55282
  se cambio el pin del chip select para dejar libre el pin del i2c externo
  usar constructor / PARA EL ABRIR ARCHIVOS
algunas cosas al tener para considerar con el la matriz de la pantalla 
      u8g2.setCursor(0, 14);
      u8g2.print("TEMP/HUM");
      u8g2.setCursor(0, 31);
      u8g2.print(String(bme.readTemperature()) + "\xb0" + "CE" + String(counterror));
      u8g2.setCursor(0, 48);
      u8g2.print(String(bme.readHumidity()) + "%HS" + String(countSDG));
      u8g2.setCursor(0, 64);
      u8g2.print(String(contador) + " enV" + "b:" + String(voltageB));
LA FUNCION LORA ES LA QUE MANDA EL MENSAJE

*/


#include "arduino_secrets.h"
// Please enter your sensitive data in the Secret tab or arduino_secrets.h
String appEui = SECRET_APP_EUI;
String appKey = SECRET_APP_KEY;

#include <MKRWAN_v2.h>
LoRaModem modem;
int contRetry = 0;
int Retrys = 10;  //veces de intentar conectaser
int counterror;   //contador de errores -2 transmite pero no entra en canales bloqueados si es asi al limitR vez se reinica
int limitR = 5;   // limite para reiniciar y reintentar
/////SD
#include <SPI.h>
//#include <SD.h>
#include "SdFat.h"
SdFat SD;
const int chipSelect = 7;
File dataFile;
///////////////////////// rtc
// Date and time functions using a DS3231 RTC connected via I2C and Wire lib
#include "RTClib.h"
RTC_DS3231 myRTCB;

String fecha;
String hora;
////////////////// variables sD
File uploadFile;
String mensajeSD = "Temperatura,Humedad,pression,corriente,columna de agua,promedio,bateria,Veces enviado,Conectado,hora,fecha";
String filename = "/RESP.csv";
bool firstrun = true;
bool Conectado = true;
bool SDok = false;
bool bme280ok = false;
bool ina219ok = false;
String ver = "1.0.0R";  ////////////////////////////////////////////////////////////////////////
bool debugL = false;
bool firstC = true;
//////control guardado SD
bool sending = false;
#define ledSDP 6  //PIN DEL LED INTERNO
bool savedSD = false;
bool ledSD = false;
int SDT = 300000;      //300000 5  minutos //600000 10 minutos*
int timeP = 30000;    //milis para pasar paginas
int timeP2 = 300000;  // 30000; 30 segundosmilis para apagar pantalla //300000 5  minutos
unsigned long countSDG = 0;
//////tiempos de mandados y reset
//mandar datos
int menT = 180000;  //300000 5  minutos// 180000 3  minutos  // 120000 2 minutos tiempo entre transmision de datos
//////
int failSafeReset = 3600000;  //la puesta en terreno nos muestra que mejor es que se reinicie cada 1 hora 3600000
//////////////////////////////////////////////////////////////////////////////////////////////////////////
////pantalla
//////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <U8g2lib.h>
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);  //chica
//U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE); //mas grande

////////////////////menus
int p = 2;          //paginas para pasar en pantalla
#define btnA 1      //BTON PIN
#define btnB 2      //BTON PIN
#define btnC 3      //BTON PIN
int LbtnA;          //ESTADOBTN
int LbtnB;          //ESTADOBTN
int LbtnC;          //ESTADOBTN
bool PoffS = true;  //apagar prender pantalla

///bme
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#define SEALEVELPRESSURE_HPA (1016.25)
Adafruit_BME280 bme;  // I2C

float tempInt;
float humInt;
float pressInt;
float altitud;
float depth;  //sensor de nivel 4-20 mah
int err;
unsigned long contador = 0;
bool enLoop = true;
/////////////////////////////////////////////////////////////
//ina corriente y nivel
#include <Wire.h>
#include <Adafruit_INA219.h>

Adafruit_INA219 ina219;
float shuntvoltage = 0;
float busvoltage = 0;
float current_mA = 0;
float loadvoltage = 0;
float power_mW = 0;

float CURRENT_INIT = 4.00;  // Current @ 0mm (uint: mA) par llegar a 0
float CURRENT_3M = 23.00;   // Replace ??? with the calibrated current value at 3m depth
float currentemp;

int RANGE = 3000;  // Depth measuring range 5000mm (for water)
bool RangeM = false;
bool menu = false;

const int numReadings = 20;
float promedioAgua;  //este promedio se reiniciara cada 24 horas/lecturas

int indexD = 0;               // El indice de la lectura actual
float readings[numReadings];  // Lecturas de la entrada analogica
float total = 0.0;            // Total
float average = 0.0;          // Promedio
float voltageB = 0;
bool rtcBool = true;
bool debugTest = false;  /////////////////////////////////////////////////////////////// debugg apaga la pantalla

#include <WDTZero.h>
WDTZero MyWatchDoggy;  // Define WatchDogTimer
///////////////////////////// led
#include <Adafruit_NeoPixel.h>
#define PIN 4        //PIN DEL LED RGB
#define NUMPIXELS 1  // Popular NeoPixel ring size
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
////////////////////////////////////////////////////////////////////////////////////////////////
//
//
////////////////////////////////////////////////////////////////////////////////////////////////
//Eprom clock
#define addressMem 0x57  // puede ser 50 o 57 o 54
int intVal;
int intVal2;
byte highByte, lowByte, highByte2, lowByte2, highByte3, lowByte3, highByteInt, lowByteInt, dataCount;
bool canR = false;
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int runs;  //variable para esperar el envio de datos cuando se llene la serie de corriente y sacar el promedio

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// setup
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
  pinMode(chipSelect, OUTPUT);  // Configurar el pin chipSelect como salida
  digitalWrite(LORA_RESET, LOW);
  SD.end();
  digitalWrite(chipSelect, HIGH);  // Desactivar manualmente el chip select
  Serial.begin(115200);
  Serial.println("Codigo kuscalla agua ver: " + String(ver));
  pixels.begin();  // INITIALIZE NeoPixel strip object (REQUIRED)
  pixels.setPixelColor(0, pixels.Color(255, 255, 255));
  pixels.show();

  Serial.println("revision de los componentes: ");
  Wire.begin();
  Wire.setClock(50000);  // Set I2C clock speed to 50kHz
  u8g2.begin();
  u8g2.clearBuffer();                  // clear the internal memory
  u8g2.setFont(u8g2_font_crox4hb_tr);  // choose a suitable font grande
  u8g2.drawStr(0, 14, "NodoN*");       // write something to the internal memory
  u8g2.drawStr(0, 30, "rev sens");     // write something to the internal memory
  u8g2.drawStr(0, 46, "rev SD");
  u8g2.sendBuffer();
  delay(1000);  // Wait for 1 second (1000 milliseconds)
  pinMode(ledSDP, OUTPUT);
  digitalWrite(ledSDP, HIGH);
  pinMode(btnA, INPUT_PULLUP);
  pinMode(btnB, INPUT_PULLUP);
  pinMode(btnC, INPUT_PULLUP);

  // Attempt to initialize the SD card
  int maxAttempts = 3;  // Limit the number of attempts to initialize the SD card
  int attemptCount = 0;

  while (!SD.begin(chipSelect) && attemptCount < maxAttempts) {
    Serial.println("Card failed, or not present. Retrying...");
    u8g2.drawStr(0, 46, "rev SD..");  // write something to the internal memory
    SDok = false;
    attemptCount++;
    if (attemptCount == 2) {
      u8g2.drawStr(0, 46, "rev SD...");  // write something to the internal memory
    }
    u8g2.sendBuffer();
    delay(1000);
  }

  if (attemptCount == maxAttempts) {
    Serial.println("Failed to initialize SD card after multiple attempts.");
    u8g2.drawStr(0, 45, "rev SD... NO");  // write something to the internal memory
    u8g2.sendBuffer();
  } else {
    SDok = true;
    u8g2.drawStr(0, 46, "rev SD.. OK");  // write something to the internal memory
    u8g2.sendBuffer();
    Serial.println("Card OK...");
  }

  Serial.flush();

  LbtnA = digitalRead(btnA);
  if (LbtnA == 0) {
    Retrys = 0;
    digitalWrite(ledSDP, LOW);
    delay(500);
    digitalWrite(ledSDP, HIGH);
     u8g2.drawStr(0, 64, "ret  0");  // write something to the internal memory
    u8g2.sendBuffer();
    Serial.println("Retrys reiniciados");
  }
  for (int thisReading = 0; thisReading < numReadings; thisReading++)
    readings[thisReading] = 0;
  //  // Initialize the INA219.
  if (!ina219.begin()) {
    Serial.println("Failed to find INA219 chip");
    //while (1) { delay(10); } //solo es
    delay(500);
  } else {
    Serial.println("INA219 chip OK");
    ina219ok = true;
    delay(500);
  }
  // Or to use a lower 16V, 400mA range (higher precision on volts and amps):
  ina219.setCalibration_16V_400mA();

  Serial.println("Measuring voltage and current with INA219 ...");


  delay(500);
  Serial.println("Prueba arduino wan lora TTN SD");
  bool status;
  // default settings
  status = bme.begin(0x76);  //76
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    delay(500);
  } else {
    Serial.println("valid BME280 sensor, OK");
    bme280ok = true;
    delay(500);
  }

  if (!myRTCB.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    rtcBool = false;
    // while (1) delay(10);
  }
  if (rtcBool == true) {
    delay(1000);
    readFromEEPROM();
    Serial.print(CURRENT_INIT, 2);  //Read the data and print to Serial port with 2 decimal places
    Serial.println();
    Serial.println("RTC ok y leida la EEPROM");
    delay(1000);
  }

  u8g2.clearBuffer();                  // clear the internal memory
  u8g2.setFont(u8g2_font_crox4hb_tr);  // choose a suitable font grande
  //u8g2.setFont(u8g2_font_pressstart2p_8u);

  u8g2.setCursor(0, 16);
  u8g2.print("Lora V: " + ver);
  u8g2.setFont(u8g2_font_t0_11_tf);
  if (SDok == true) {
    u8g2.drawStr(0, 40, "SD: OK");
  } else {
    u8g2.drawStr(0, 40, "SD: NO");
  }
  if (bme280ok == true) {
    u8g2.drawStr(0, 50, "BME: OK");
  } else {
    u8g2.drawStr(0, 50, "BME: NO");
  }
  if (ina219ok == true) {
    u8g2.drawStr(0, 60, "INA219: OK");
  } else {
    u8g2.drawStr(0, 60, "INA219: NO");
  }
  if (rtcBool == true) {
    u8g2.drawStr(70, 60, "rtc: OK");
  } else {
    u8g2.drawStr(70, 60, "rtc: NO");
  }
  u8g2.sendBuffer();
  // Set poll interval to 60 secs.

  if (!modem.begin(AU915_TTN)) {
    Serial.println("Failed to start module");
    u8g2.drawStr(0, 50, "FAIL MODEM");  // write something to the internal memory
    u8g2.sendBuffer();
    delay(1000);
  };
  u8g2.setFont(u8g2_font_crox4hb_tr);  // choose a suitable font grande
  Serial.println("Conectando...");
  u8g2.drawStr(0, 30, "conectando...");  // write something to the internal memory
  u8g2.sendBuffer();
  Serial.print("Your module version is: ");
  Serial.println(modem.version());
  Serial.print("Your device EUI is: ");
  Serial.println(modem.deviceEUI());
  Serial.print("Your device Data Rate is: ");
  Serial.println(modem.getDataRate());  //  modem.DataRate(xx);    // -> Returns OK! // -> Returns -1

  int connected = modem.joinOTAA(appEui, appKey);

  while (!connected) {
    Serial.println("Retry...");
    u8g2.clearBuffer();                // clear the internal memory
    u8g2.drawStr(0, 16, "TEST lora");  // write something to the internal memory
    u8g2.drawStr(0, 46, "retry: ");    // write something to the internal memory
    u8g2.setCursor(60, 47);
    u8g2.print(contRetry);
    u8g2.sendBuffer();
    if (!modem.joinOTAA(appEui, appKey)) {
      Serial.print("Fail counter: ");
      contRetry = contRetry + 1;
      u8g2.drawStr(0, 64, "FAIL!!");  // write something to the internal memory
      u8g2.sendBuffer();
      Serial.println(contRetry);
      u8g2.sendBuffer();
      if (contRetry > Retrys) {
        Conectado = false;
        break;
      }
    } else {
      Conectado = true;
      break;
    }
  };

  modem.minPollInterval(120);


  DateTime now = myRTCB.now();
  delay(500);
  //readFromEEPROM();
  delay(500);
  Serial.println("\nWDTZero : Setup HARD Watchdog at 16S interval");
  MyWatchDoggy.setup(WDT_HARDCYCLE16S);  // initializeWDT_HARDCYCLE16S refesh cycle on 16 sec interval
  digitalWrite(ledSDP, LOW);
  runs = numReadings;
  //////////////////////////////////////////////////////////////////////////////////////////
  delay(500);
}

//////////////////////////////////////////////////////////////////////////////////
//LOOP
/////////////////////////////////////////////////////////////////////////////////
void loop() {
  if (firstrun == true) {
    callpatdog();
    u8g2.clearBuffer();  // clear the internal memory
    u8g2.setCursor(0, 14);
    u8g2.print("leer SD");
    u8g2.setCursor(0, 30);
    u8g2.print("primero");
    u8g2.sendBuffer();
    // Attempt to initialize the SD card
    if (SDok == false) {
      // Desmontar la tarjeta SD
      SD.end();
      delay(3000);
      digitalWrite(chipSelect, HIGH);  // Desactivar manualmente el chip select
      u8g2.sendBuffer();
      int maxAttempts = 5;  // Limit the number of attempts to initialize the SD card
      int attemptCount = 0;
      digitalWrite(chipSelect, HIGH);  // Desactivar manualmente el chip select
      while (!SD.begin(chipSelect) && attemptCount < maxAttempts) {
        Serial.println("Card failed, or not present. Retrying...");
        u8g2.drawStr(0, 46, "rev SD..");  // write something to the internal memory
        SDok = false;
        attemptCount++;

        if (attemptCount == 2) {
          u8g2.drawStr(0, 46, "rev SD...");  // write something to the internal memory
        }
        u8g2.sendBuffer();
        callpatdog();
        delay(1000);
      }

      if (attemptCount == maxAttempts) {
        Serial.println("Failed to initialize SD card after multiple attempts.");
        u8g2.drawStr(0, 45, "rev SD... NO");  // write something to the internal memory
        u8g2.sendBuffer();
      } else {
        SDok = true;
        u8g2.drawStr(0, 46, "rev SD.. OK");  // write something to the internal memory
        u8g2.sendBuffer();
        Serial.println("Card OK...");
      }
    }
    if (SDok == true) {
      Serial.println("Tarjeta inicializada:");
      Serial.print("Filename: ");
      Serial.println(filename);

      dataFile = SD.open(filename, FILE_READ);  // Attempt to open the file for reading

      if (!dataFile) {                             // If the file does not open, it probably does not exist
        dataFile = SD.open(filename, FILE_WRITE);  // Open the file for writing
        if (dataFile) {
          dataFile.println(mensajeSD + "," + String(modem.deviceEUI()));
          Serial.println("Created new file: " + String(filename) + " and wrote: " + mensajeSD + "," + String(modem.deviceEUI()));
          dataFile.close();
          callpatdog();
          delay(1000);
        } else {
          Serial.println("Failed to create the file.");
        }
      } else {
        dataFile.close();  // If the file was opened for reading, close it here
        Serial.println("File already exists: " + String(filename));
      }
    }

    firstrun = false;
  }

  //////////////////////////////////////////////////////////////////////////////////
  // Sensores
  /////////////////////////////////////////////////////////////////////////////////////

  shuntvoltage = ina219.getShuntVoltage_mV();
  busvoltage = ina219.getBusVoltage_V();
  current_mA = ina219.getCurrent_mA();
  power_mW = ina219.getPower_mW();
  loadvoltage = busvoltage + (shuntvoltage / 1000);
  voltageB = loadvoltage;
  // Restamos la ultima lectura:
  total = total - readings[indexD];
  // Leemos del sensor:
  readings[indexD] = current_mA;
  // Añadimos la lectura al total:
  total = total + readings[indexD];
  // Avanzamos a la proxima posicion del array
  indexD = indexD + 1;
  // Si estamos en el final del array...
  if (indexD >= numReadings)
    indexD = 0;  // ...volvemos al inicio:

  // Calculamos el promedio:
  average = total / numReadings;
  // Lo mandamos a la PC como un valor ASCII

  // Calculate depth based on average current reading nueva formula
  depth = ((average - CURRENT_INIT) * RANGE) / (CURRENT_3M - CURRENT_INIT) / 1000;


  if (depth < 0) {
    depth = 0;
  }
  tempInt = bme.readTemperature();
  humInt = bme.readHumidity();
  pressInt = bme.readPressure() / 100.0F;
  altitud = bme.readAltitude(SEALEVELPRESSURE_HPA);
  //////////////////////////////////////////////////////////////////////////////////////
  //envio de datos
  /////////////////////////////////////////////////////////////////////////////////////

  if (Serial.available() > 0) {
    //This line is clearing the serial buffer<
    contador++;
    String msg = Serial.readStringUntil('\r');
    callpatdog();
    sending = true;

    Serial.println();
    Serial.print("Sending: " + msg + " - ");
    for (unsigned int i = 0; i < msg.length(); i++) {
      Serial.print(msg[i] >> 4, HEX);
      Serial.print(msg[i] & 0xF, HEX);
      Serial.print(" ");
    }
    Serial.println();
    modem.setPort(1);
    modem.beginPacket();
    modem.print(msg);

    err = modem.endPacket(false);
    Serial.print("Message state: ");
    Serial.println(err);

    switch (err) {
      case -2:
        Serial.print("Error channel: ");
        counterror++;
        Serial.println(counterror);
        u8g2.clearBuffer();  // clear the internal memory
        u8g2.setCursor(0, 14);
        u8g2.print("Error -2");
        u8g2.setCursor(0, 30);
        u8g2.print(String(counterror) + "veces fallando");
        u8g2.setCursor(0, 46);
        u8g2.print("a la " + String(limitR) + "vez reinicio");
        u8g2.setCursor(0, 64);
        u8g2.print(String(contador) + " enV" + "b:" + String(voltageB));
        u8g2.sendBuffer();
        callpatdog();
        sending = false;
        delay(3000);
        break;
      case -1:
        Serial.println("Timeout");
        callpatdog();
        sending = false;
        break;
      case 1:
        Serial.println("Message sent !");
        callpatdog();
        sending = false;
        break;
      default:
        Serial.print("Error while sending message with code: ");
        Serial.println(err);
        callpatdog();
        sending = false;
        break;
    }
    Serial.println();
    delay(1000);
    if (!modem.available()) {
      Serial.println("No downlink message received at this time.");
      callpatdog();
      sending = false;
    }
    char rcv[64];
    int i = 0;
    while (modem.available()) {
      rcv[i++] = (char)modem.read();
    }
    Serial.print("Received: ");
    for (unsigned int j = 0; j < i; j++) {
      Serial.print(rcv[j] >> 4, HEX);
      Serial.print(rcv[j] & 0xF, HEX);
      Serial.print(" ");
      callpatdog();
      sending = false;
    }
    Serial.println();
  }

  ////////////////////////////////////////////////////////////////////
  // botones
  ////////////////////////////////////////////////////////////////////
  LbtnA = digitalRead(btnA);
  LbtnB = digitalRead(btnB);
  LbtnC = digitalRead(btnC);
  if (LbtnA == 0) {
    p = p + 1;
    if (p == 7) p = 0;
    PoffS = true;
    digitalWrite(ledSDP, HIGH);
    delay(500);
    digitalWrite(ledSDP, LOW);
    if (menu == true) {
      //calLevel();
      if (canR == false) {
        u8g2.clearBuffer();  // clear the internal memory
        u8g2.setCursor(0, 14);
        u8g2.print("Error");
        u8g2.setCursor(0, 30);
        u8g2.print("no rtc o es");
        u8g2.sendBuffer();
        delay(5000);
      }
      callpatdog();
      if (average <= 0) average = 0;
      writeToEEPROM(average);  //////////////////////////////////////////////////////////////////////////////////////////// calibracion
    }
  }

  if (LbtnB == 0 && PoffS == true) {
    menu = !menu;
    p = p + 1;
    if (p >= 5) p = 0;
    Serial.println(menu);
    digitalWrite(ledSDP, HIGH);
    delay(500);
    digitalWrite(ledSDP, LOW);
  }
  ////////////////////////////////////////////////////////
  static unsigned long pmen2 = millis();  //every 30 seconds send men cmbio de visualizacion entre pantallas
  if (millis() - pmen2 >= timeP2 && debugTest == false) {
    pmen2 = millis();
    PoffS = false;
  }


  if (LbtnC == 0 && PoffS == true) {
    digitalWrite(ledSDP, HIGH);
    delay(500);
    digitalWrite(ledSDP, LOW);
    if (menu == true) {

      if (canR == false) {
        u8g2.clearBuffer();  // clear the internal memory
        u8g2.setCursor(0, 14);
        u8g2.print("Error");
        u8g2.setCursor(0, 30);
        u8g2.print("no rtc o es");
        u8g2.sendBuffer();
        delay(5000);
      }
      callpatdog();
      if (average <= 0) average = 0;
      writeToEEPROM2(average);  //////////////////////////////////////////////////////////////////////////////////////////// calibracion
    } else {
      Serial.print("Actualizado - Nivel de agua acumulado con nivel acutal: ");
      Serial.println(depth);
      Serial.print("Reiniciado - Contador de datos: ");
      Serial.println(1);
      // Actualizar la EEPROM con los nuevos valore
      u8g2.clearBuffer();  // clear the internal memory
      u8g2.setCursor(0, 14);
      u8g2.print("Reiniciando");
      u8g2.setCursor(0, 30);
      u8g2.print("NIVEL");
      u8g2.setCursor(0, 45);
      u8g2.print("PROM");
      u8g2.setCursor(0, 60);
      u8g2.print(depth);
      u8g2.sendBuffer();
      writeToEEPROMWaterLevel(depth);
      writeToEEPROMDataCount(1);
      delay(2000);
    }
  }

  if (debugTest == false) {
    if (PoffS == false) {
      u8g2.setPowerSave(1);
    } else {
      u8g2.setPowerSave(0);
    }
  }
  ////////////////////////////////////////////////////
  // paginas del display
  ////////////////////////////////////////////////////////
  static unsigned long pmen = millis();  //every 30 seconds send men cmbio de visualizacion entre pantallas
  if (millis() - pmen >= timeP) {
    pmen = millis();
    p = p + 1;
    if (p >= 3) p = 0;  //SIN BOTONES MUESTRO HASTA LA PAGINA 3

    Serial.print("Bus Voltage:   ");
    Serial.print(busvoltage);
    Serial.println(" V");
    Serial.print("Shunt Voltage: ");
    Serial.print(shuntvoltage);
    Serial.println(" mV");
    Serial.print("Load Voltage:  ");
    Serial.print(loadvoltage);
    Serial.println(" V");
    Serial.print(" Average = ");
    Serial.print(average);
    Serial.print(" mA   ");
    Serial.print(depth);
    Serial.println(" MT");
    // print out the value you read:
    Serial.print(" voltaje: ");
    Serial.println(voltageB, 4);
    Serial.print("En que pagina estoy: ");
    Serial.println(p);
  }
  if (menu == false) {  /////////////////////////////// para entrar a calibracion y seteo de nivel
    if (p == 0) {
      u8g2.clearBuffer();  // clear the internal memory
      u8g2.setCursor(0, 14);
      u8g2.print("TEMP/HUM");
      u8g2.setCursor(0, 31);
      u8g2.print(String(bme.readTemperature()) + "\xb0" + "CE" + String(counterror));
      u8g2.setCursor(0, 48);
      u8g2.print(String(bme.readHumidity()) + "%HS" + String(countSDG));
      u8g2.setCursor(0, 64);
      u8g2.print(String(contador) + " enV" + "b:" + String(voltageB));
      u8g2.sendBuffer();
    }

    if (p == 1) {
      u8g2.clearBuffer();  // clear the internal memory
      u8g2.setCursor(0, 14);
      u8g2.print("PRESI/MT");
      u8g2.setCursor(0, 31);
      u8g2.print(String(pressInt) + "PAC" + String(counterror));
      u8g2.setCursor(0, 48);
      u8g2.print(String(altitud) + "mtD" + String(countSDG));
      u8g2.setCursor(0, 64);
      u8g2.print(String(contador) + " enV" + "b:" + String(voltageB));
      u8g2.sendBuffer();
    }

    if (p == 2) {
      u8g2.clearBuffer();  // clear the internal memory
      u8g2.setCursor(0, 14);
      u8g2.print("NIVEL/MT");
      u8g2.setCursor(0, 31);
      u8g2.print(String(current_mA) + "mAC" + String(counterror));
      u8g2.setCursor(0, 48);
      u8g2.print(String(depth) + " MT");
      u8g2.setCursor(0, 64);
      u8g2.print(String(contador) + " enV" + "b:" + String(voltageB));
      u8g2.sendBuffer();
    }

    if (p == 3) {
      DateTime now = myRTCB.now();
      fecha = getFecha(now);
      hora = getHora(now);
      u8g2.clearBuffer();  // clear the internal memory
      u8g2.setCursor(0, 14);
      u8g2.print("F/HORA");
      u8g2.setCursor(0, 31);
      u8g2.print(String(fecha));
      u8g2.setCursor(0, 48);
      u8g2.print(String(hora));
      u8g2.setCursor(0, 64);
      u8g2.print(String(Retrys) + "R");
      u8g2.sendBuffer();
    }
    if (p == 4) {
      u8g2.clearBuffer();  // clear the internal memory
      u8g2.setCursor(0, 14);
      u8g2.print("ESTADO");
      u8g2.setCursor(0, 31);
      u8g2.print(String(countSDG) + "GSD");
      u8g2.setCursor(0, 48);
      u8g2.print(String(Conectado) + "L" + " N:" + String(filename));
      u8g2.setCursor(0, 64);
      u8g2.setFont(u8g2_font_t0_11_tf);
      String DEUI = String(modem.deviceEUI());
      DEUI.replace(":", "");
      u8g2.print("DEUI:" + String(DEUI));
      u8g2.setFont(u8g2_font_crox4hb_tr);  // choose a suitable font grande
      u8g2.sendBuffer();
    }
    if (p == 5) {
      u8g2.clearBuffer();  // clear the internal memory
      u8g2.setCursor(0, 14);
      u8g2.print("Prom/hora");
      u8g2.setCursor(0, 31);
      u8g2.print(String(promedioAgua) + "Prom");
      u8g2.setCursor(0, 48);
      u8g2.print(String(dataCount) + "Cont");
      u8g2.setCursor(0, 64);
      u8g2.setFont(u8g2_font_t0_11_tf);
      String DEUI = String(modem.deviceEUI());
      DEUI.replace(":", "");
      u8g2.print("DEUI:" + String(DEUI));
      u8g2.setFont(u8g2_font_crox4hb_tr);  // choose a suitable font grande
      u8g2.sendBuffer();
    }
    if (p == 6) {
      u8g2.clearBuffer();                  // clear the internal memory
      u8g2.setFont(u8g2_font_crox4hb_tr);  // choose a suitable font grande
      //u8g2.setFont(u8g2_font_pressstart2p_8u);

      u8g2.setCursor(0, 16);
      u8g2.print("Lora V: " + ver);
      u8g2.setFont(u8g2_font_t0_11_tf);
      if (SDok == true) {
        u8g2.drawStr(0, 40, "SD: OK");
      } else {
        u8g2.drawStr(0, 40, "SD: NO");
      }
      if (bme280ok == true) {
        u8g2.drawStr(0, 50, "BME: OK");
      } else {
        u8g2.drawStr(0, 50, "BME: NO");
      }
      if (ina219ok == true) {
        u8g2.drawStr(0, 60, "INA219: OK");
      } else {
        u8g2.drawStr(0, 60, "INA219: NO");
      }
      if (rtcBool == true) {
        u8g2.drawStr(70, 60, "rtc: OK");
      } else {
        u8g2.drawStr(70, 60, "rtc: NO");
      }
      u8g2.sendBuffer();
      u8g2.setFont(u8g2_font_crox4hb_tr);  // choose a suitable font grande
      u8g2.sendBuffer();
    }
  }
  if (menu == true) {
    u8g2.clearBuffer();  // clear the internal memory
    u8g2.setCursor(0, 14);
    u8g2.print("CALIBRACION");
    u8g2.setCursor(0, 31);
    u8g2.print(String(voltageB) + "V" + String(average) + "mA");
    u8g2.setCursor(0, 48);
    u8g2.print(String(CURRENT_INIT) + "Z " + String(CURRENT_3M) + "M");
    u8g2.setCursor(0, 64);
    u8g2.print(String(depth) + "MT");
    u8g2.sendBuffer();
  }
  if (SDok == true && bme280ok == true && ina219ok == true && Conectado == true) {
    pixels.setPixelColor(0, pixels.Color(0, 255, 0));
    pixels.show();
  }


  if (SDok == true && bme280ok == true && ina219ok == true && Conectado == false) {
    pixels.setPixelColor(0, pixels.Color(0, 0, 255));
    pixels.show();
  }

  if (SDok == false && bme280ok == true && ina219ok == true && Conectado == false) {
    pixels.setPixelColor(0, pixels.Color(255, 0, 0));
    pixels.show();
  }

  if (runs == 0) {
    calculateAverageFromEEPROM();
    //writeToEEPROM3(depth, 1);//primera lectura esperando al llenado del promedio
    lora();
    runs = runs - 1;
  }
  runs = runs - 1;
  static unsigned long Tmen = millis();
  if (millis() - Tmen >= menT and Conectado == true)  //  2 minutos
  {
    Tmen = millis();
    lora();
  }

  static unsigned long TSD = millis();
  if (millis() - TSD >= SDT)  //SDT tiempo de guardado
  {
    TSD = millis();
    Serial.print("Guardando en la SD: ");
    Serial.println(sending);
    if (SDok == true && sending == false) {

      DateTime now = myRTCB.now();
      fecha = getFecha(now);
      hora = getHora(now);
      // este es el primer mensaje en el csv para poder usar la misma estructura "Temperatura,Humedad,pression,corriente,columna de agua,promedio,bateria,Veces enviado,Conectado,hora,fecha";
      mensajeSD = String(bme.readTemperature())
                  + "," + String(bme.readHumidity())
                  + "," + String(pressInt)
                  + "," + String(average)
                  + "," + String(depth)
                  + "," + String(promedioAgua)
                  + "," + String(voltageB)
                  + "," + String(contador)
                  + "," + String(Conectado)
                  + "," + String(hora)
                  + "," + String(fecha);

      // open the file. note that only one file can be open at a time,
      // so you have to close this one before opening another.
      dataFile = SD.open(filename, FILE_WRITE);

      // if the file is available, write to it:
      if (dataFile) {
        countSDG++;
        dataFile.println(mensajeSD);
        dataFile.close();
        // print to the serial port too:
        Serial.println("guardado numero: " + String(countSDG) + " en la sd correctamente: " + mensajeSD + " En: " + String(filename));
        firstrun = false;
        savedSD = true;
        ledSD = true;
      }  // if the file isn't open, pop up an error:
      else {
        Serial.println("error opening: " + String(filename));
        savedSD = false;
        ledSD = false;
      }
    } else {
      Serial.println("Error de SD o mandando datos");
      savedSD = false;
      ledSD = false;
    }
    dataFile.close();
  }
  static unsigned long TSDR = millis();
  if (millis() - TSDR >= 1000)  //SDT tiempo de fuardado
  {
    TSDR = millis();
    if (savedSD == true) {
      ledSD = true;
    } else {
      ledSD = false;
    }
  }

  if (ledSD == true) {
    digitalWrite(ledSDP, HIGH);
    savedSD = false;
  } else {
    digitalWrite(ledSDP, LOW);
  }
  if (counterror == limitR) {
    u8g2.clearBuffer();  // clear the internal memory
    u8g2.setCursor(0, 14);
    u8g2.print("ERROR LIMIT");
    u8g2.setCursor(0, 31);
    u8g2.print("REINICIANDO");
    u8g2.sendBuffer();
    Serial.println("error de transmision intentando reconectar, reebooteando");
    NVIC_SystemReset();
  }
  static unsigned long patdog = millis();
  if (millis() - patdog >= 10000)  //patdog evitando que elwatchdog reinicie 10 segundos
  {
    patdog = millis();
    callpatdog();
  }
  static unsigned long dayR = millis();
  if (millis() - dayR >= failSafeReset) {
    dayR = millis();
    u8g2.clearBuffer();  // clear the internal memory
    u8g2.setCursor(0, 14);
    u8g2.print("RESET");
    u8g2.setCursor(0, 31);
    u8g2.print("REINICIANDO");
    u8g2.sendBuffer();
    Serial.println("Reinciando para asegurar conexion lorawan");
    delay(1000);
    dataFile.close();
    digitalWrite(LORA_RESET, LOW);
    SD.end();
    delay(1000);
    digitalWrite(chipSelect, HIGH);  // Desactivar manualmente el chip select
    delay(1000);
    NVIC_SystemReset();
  }

  DateTime now = myRTCB.now();
}
///////////////////////////////////////////////////////////////////fin del loop


void callpatdog() {
  Serial.println("check watchdog");
  MyWatchDoggy.clear();  // refresh wdt - before it loops

  DateTime now = myRTCB.now();
  fecha = getFecha(now);
  hora = getHora(now);
  Serial.println("Fecha: " + fecha);
  Serial.println("Hora: " + hora);
  Serial.println("promedio: " + String(promedioAgua));
  Serial.println("SD: " + String(SDok) + " bme280ok: " + String(bme280ok) + " ina219ok: " + String(ina219ok) + "  Conectado: " + String(Conectado) + " RTC: " + String(rtcBool));
}

String getFecha(DateTime now) {
  String fecha = String(now.day(), DEC) + "/";
  fecha += String(now.month(), DEC) + "/";
  fecha += String(now.year(), DEC);
  return fecha;
}

String getHora(DateTime now) {
  String hora = String(now.hour(), DEC) + ":";
  hora += String(now.minute(), DEC) + ":";
  hora += String(now.second(), DEC);
  return hora;
}
/////////////////////////////////////////////////////////////////////////////////////////7
//funcion para mandar datos
//////////////////////////////////////////////////////////////////////////////////////////
void lora() {

  sending = true;
  contador++;
  // Unsigned 16 bits integer, 0 up to 65535
  uint16_t t = tempInt * 100;
  uint16_t h = humInt * 100;
  uint16_t l = depth * 1000;
  uint16_t n = contador;
  uint16_t b = voltageB * 100;
  uint16_t p = promedioAgua * 1000;
  uint16_t s = SDok;

  byte buffer[14];
  buffer[0] = t >> 8;
  buffer[1] = t;
  buffer[2] = h >> 8;
  buffer[3] = h;
  buffer[4] = l >> 8;
  buffer[5] = l;
  buffer[6] = n >> 8;
  buffer[7] = n;
  buffer[8] = b >> 8;
  buffer[9] = b;
  buffer[10] = p >> 8;
  buffer[11] = p;
  buffer[12] = s >> 8;
  buffer[13] = s;

  modem.setPort(1);
  modem.beginPacket();
  modem.write(buffer, 14);
  err = modem.endPacket(false);

  Serial.print("Message state: ");
  Serial.println(err);

  switch (err) {
    case -2:
      Serial.print("Error channel: ");
      counterror++;
      Serial.println(counterror);
      u8g2.clearBuffer();  // clear the internal memory
      u8g2.setCursor(0, 14);
      u8g2.print("Error -2");
      u8g2.setCursor(0, 31);
      u8g2.print(String(counterror) + "veces fallando");
      u8g2.setCursor(0, 48);
      u8g2.print("a la " + String(limitR) + "vez reinicio");
      u8g2.setCursor(0, 64);
      u8g2.print(String(contador) + " enV" + "b:" + String(voltageB));
      u8g2.sendBuffer();
      callpatdog();
      sending = false;
      delay(3000);
      break;
    case -1:
      Serial.println("Timeout ");
      callpatdog();
      sending = false;
      break;
    case 1:
      Serial.println("Message sent !");
      callpatdog();
      sending = false;
      break;
    default:
      Serial.print("Error while sending message with code: ");
      Serial.println(err);
      callpatdog();
      sending = false;

      break;
  }
  delay(1000);
  if (!modem.available()) {
    Serial.println("No downlink message received at this time.");
    callpatdog();
    sending = false;
  }
  char rcv[64];
  int i = 0;
  while (modem.available()) {
    rcv[i++] = (char)modem.read();
  }
  Serial.print("Received: ");
  for (unsigned int j = 0; j < i; j++) {
    Serial.print(rcv[j] >> 4, HEX);
    Serial.print(rcv[j] & 0xF, HEX);
    Serial.print(" ");
    callpatdog();
    sending = false;
  }
  callpatdog();
  Serial.println();
}

void readFromEEPROM() {
  // Start I2C transmission and check for errors
  Wire.beginTransmission(addressMem);
  byte error = Wire.endTransmission();
  if (error) {
    Serial.print("Error during transmission: ");
    switch (error) {
      case 1: Serial.println("Data too long to fit in transmit buffer"); return;
      case 2: Serial.println("Received NACK on transmit of address"); return;
      case 3: Serial.println("Received NACK on transmit of data"); return;
      case 4: Serial.println("Other error"); return;
    }
  }

  // Specify the EEPROM address from where we want to start reading the first value
  Wire.beginTransmission(addressMem);
  Wire.write(0x00);  // First Word Address
  Wire.write(0x00);  // Second Word Address
  Wire.endTransmission();

  // Request data for the first value
  Wire.requestFrom(addressMem, 2);
  if (Wire.available() < 2) {
    Serial.println("Error reading EEPROM for the first value");
  }
  byte highByte = Wire.read();
  byte lowByte = Wire.read();
  int intVal = ((int)highByte << 8) | lowByte;
  CURRENT_INIT = intVal / 100.0;

  // Specify the EEPROM address from where we want to start reading the second value
  Wire.beginTransmission(addressMem);
  Wire.write(0x00);  // First Word Address
  Wire.write(0x02);  // Second Word Address (2 bytes after the first value)
  Wire.endTransmission();

  // Request data for the second value
  Wire.requestFrom(addressMem, 2);
  if (Wire.available() < 2) {
    Serial.println("Error reading EEPROM for the second value");
  }
  highByte = Wire.read();
  lowByte = Wire.read();
  intVal = ((int)highByte << 8) | lowByte;
  currentemp = intVal / 100.0;

  // Print retrieved data
  Serial.println("Value 1 (CURRENT_INIT) from EEPROM: " + String(CURRENT_INIT));
  Serial.println("Value 2 (currentemp) from EEPROM: " + String(currentemp));
  u8g2.clearBuffer();
  u8g2.setCursor(0, 14);
  u8g2.print(String("MEM LEC"));
  u8g2.setCursor(0, 31);
  u8g2.print(String(CURRENT_INIT) + " Cero");
  u8g2.setCursor(0, 48);  // Adjust this y-coordinate as needed
  u8g2.print(String(currentemp) + " 3MT");
  u8g2.sendBuffer();
  canR = true;
  delay(3000);
}

void writeToEEPROM(float value) {
  int intVal = (int)(value * 100);
  byte highByte = (byte)(intVal >> 8);
  byte lowByte = (byte)intVal;

  // Update OLED display
  u8g2.clearBuffer();
  u8g2.setCursor(0, 14);
  u8g2.print("Guardando 0");
  u8g2.setCursor(0, 31);
  u8g2.print(String(intVal) + "mA");
  u8g2.setCursor(0, 48);
  u8g2.print("CERO");
  u8g2.sendBuffer();
  delay(500);
  // Write data to EEPROM
  Wire.beginTransmission(addressMem);
  Wire.write(0x00);  //First Word Address
  Wire.write(0x00);  //Second Word Address
  Wire.write(highByte);
  Wire.write(lowByte);
  Wire.endTransmission();

  // Print debug information
  Serial.println("Writing to EEPROM");
  Serial.println("EEPROM value: " + String(intVal));
  delay(500);
  // Verify written data by reading back from EEPROM
  readFromEEPROM();
}
void writeToEEPROM2(float value2) {
  int intVal2 = (int)((value2 * 2) * 100);
  byte highByte2 = (byte)(intVal2 >> 8);
  byte lowByte2 = (byte)intVal2;

  // Update OLED display
  u8g2.clearBuffer();
  u8g2.setCursor(0, 14);
  u8g2.print("Guardando M");
  u8g2.setCursor(0, 31);
  u8g2.print(String(intVal2) + "mA");
  u8g2.setCursor(0, 48);
  u8g2.print("1.5MT x 2 (MAX)");
  u8g2.sendBuffer();
  delay(3000);

  // Write data to EEPROM
  Wire.beginTransmission(addressMem);
  Wire.write(0x00);  //First Word Address for value2
  Wire.write(0x02);  //Second Word Address for value2 (2 bytes after value)
  Wire.write(highByte2);
  Wire.write(lowByte2);
  Wire.endTransmission();

  // Print debug information
  Serial.println("Writing to EEPROM2");
  Serial.println("EEPROM value2: " + String(intVal2));
  delay(500);

  // Verify written data by reading back from EEPROM
  // You might want a separate read function to verify the second value or adjust the existing one.
  readFromEEPROM();
}
/////////////////////////////// funcion promediooid
void writeToEEPROMWaterLevel(float valorActual) {
  int intVal = (int)(valorActual * 100);  // Convertir float a int
  byte highByte = (byte)(intVal >> 8);    // Obtener byte alto
  byte lowByte = (byte)intVal;            // Obtener byte bajo

  // Escribir el valor del nivel de agua
  Wire.beginTransmission(addressMem);
  Wire.write(0x00);  // Primer Word Address para el float
  Wire.write(0x04);  // Segundo Word Address para el float
  Wire.write(highByte);
  Wire.write(lowByte);
  Wire.endTransmission();

  Serial.print("Escribiendo en EEPROM - Nivel de agua (intVal): ");
  Serial.println(intVal);
  delay(500);  // Pequeño retraso para asegurar la escritura
}
void writeToEEPROMDataCount(byte dataCount) {
  // Escribir el contador de datos
  Wire.beginTransmission(addressMem);
  Wire.write(0x00);  // Word Address para dataCount
  Wire.write(0x06);
  Wire.write(dataCount);
  Wire.endTransmission();

  Serial.print("Escribiendo en EEPROM - Contador de datos (dataCount): ");
  Serial.println(dataCount);
  delay(500);  // Pequeño retraso para asegurar la escritura
}

float calculateAverageFromEEPROM() {
  Serial.println("Calculando promedio desde EEPROM");

  // Leer el nivel de agua almacenado (float)
  Wire.beginTransmission(addressMem);
  Wire.write(0x00);
  Wire.write(0x04);
  Wire.endTransmission();
  Wire.requestFrom(addressMem, 2);
  byte highByte = Wire.read();
  byte lowByte = Wire.read();
  int intVal = (highByte << 8) | lowByte;
  float waterLevel = intVal / 100.0;

  Serial.print("Leído de la EEPROM - Nivel de agua (waterLevel): ");
  Serial.println(waterLevel);

  // Leer la cantidad de datos almacenados (byte)
  Wire.beginTransmission(addressMem);
  Wire.write(0x00);
  Wire.write(0x06);
  Wire.endTransmission();
  Wire.requestFrom(addressMem, 1);
  dataCount = Wire.read();

  Serial.print("Leído de la EEPROM - Contador de datos (dataCount): ");
  Serial.println(dataCount);

  // Actualizar el promedio y el contador
  if (dataCount == 0 || dataCount >= 24) {
    waterLevel = depth;
    dataCount = 1;
    Serial.println("Reiniciando contador y nivel de agua.");
  } else {
    waterLevel += depth;
    dataCount++;
    Serial.println("Sumando depth al nivel de agua acumulado.");
  }

  Serial.print("Actualizado - Nivel de agua acumulado: ");
  Serial.println(waterLevel);
  Serial.print("Actualizado - Contador de datos: ");
  Serial.println(dataCount);

  // Actualizar la EEPROM con los nuevos valores
  writeToEEPROMWaterLevel(waterLevel);
  writeToEEPROMDataCount(dataCount);
  // Calcular el promedio
  promedioAgua = waterLevel / dataCount;
  Serial.print("Promedio calculado: ");
  Serial.println(promedioAgua);

  return promedioAgua;
}
