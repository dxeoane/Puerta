#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <EEPROM.h>

// Los diferentes estados por lo que pasa el dispositivo
#define STATE_INIT       0 
#define STATE_CONNECTING 1
#define STATE_CONNECTED  2
#define STATE_PROG       3
#define STATE_ERROR      4
// En esta variable guardamos el estado actual
int state = STATE_INIT;

void setup() { 
  // Esperamos un segundo antes de empezar
  delay(1000);
  
  // Configuramos las entradas y salidas
  pinSetup(); 

  // Actualizamos el led de estado 
  ledLoop();
  
  // Configuramos el puerto serie
  Serial.begin(115200);
  Serial.println("\n\n\n");
  
  // Configuramos la EEPROM
  EEPROMSetup();

  // Intentamos conectarnos a la WiFi
  wifiConnect();  

  // Iniciamos el servidor UDP
  if (state == STATE_CONNECTED) {
    startUDPServer();   
  } 
}

void loop() {    
  // Nos aseguramos de que el relé está apagado
  relayOff();

  // Actualizados el led de estado
  ledLoop();

  // Comprobamos si tenemos que entrar en modo de programación
  checkProg();

  // Comprobamos si hay nuevos paquetes, y los procesamos
  receivePacket();  
}

// Tamaño maximo, en caracteres, de un valor de la EEPROM
#define EEPROM_VALUE_SIZE          64
// Estos indices indican la posición que ocupan dentro de la EEPROM cada valor
#define EEPROM_INDEX_WIFI_SSID      0
#define EEPROM_INDEX_WIFI_PASSWORD  1

void EEPROMSetup() {
  EEPROM.begin(2 * EEPROM_VALUE_SIZE);
}

// Guarda un valor en la EEPROM. Si el tamaño excede EEPROM_VALUE_SIZE, se corta
void saveValue(int index, const char* value) {
  int addr = index * EEPROM_VALUE_SIZE;
  int len = strlen(value);
  
  for (int i = 0; i < EEPROM_VALUE_SIZE; i++) {
    if (i < len) {
      EEPROM.write(addr + i, value[i]);
    } else {
      EEPROM.write(addr + i, 0);
    }
  }
  EEPROM.commit();
}

// Lee un valor de de EEPROM
void readValue(int index, char* value) {
  int addr = index * EEPROM_VALUE_SIZE;
  
  for (int i = 0; i < EEPROM_VALUE_SIZE; i++) {
    value[i] = EEPROM.read(addr + i);
    if (value[i] == 0) return;
  }

  value[EEPROM_VALUE_SIZE] = 0;
}

void wifiConnect() {

  // Estos dos valores lod vamos a leer de la EEPROM deben tener como minimo el tamaño EEPROM_VALUE_SIZE + 1
  char ssid[EEPROM_VALUE_SIZE + 1];
  char password[EEPROM_VALUE_SIZE + 1];
  
  readValue(EEPROM_INDEX_WIFI_SSID, ssid);
  readValue(EEPROM_INDEX_WIFI_PASSWORD, password);

  Serial.printf("Conectando con: %s\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Cambiamos el estado del dispositivo a "Conectando"
  state = STATE_CONNECTING;
  // Esperamos hasta que el dispositivo este conectado, o que pasen 30 segundos
  unsigned long start = millis();
  while ((WiFi.status() != WL_CONNECTED) && ((millis() - start) < 30000) ) {
    delay(10);
    ledLoop();
  }

  // Comprobamos si estamos conectados
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Error al conectar!");
    state = STATE_ERROR;
  } else {
    state = STATE_CONNECTED;
    Serial.println("Conectado!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  }
  
}

void startAP() {  
  
  String ssid = "HW622_" + WiFi.softAPmacAddress();

  IPAddress local(192,168,33,1);
  IPAddress gateway(192,168,33,1);
  IPAddress subnet(255,255,255,0);

  Serial.println("Creando punto de acceso ..."  );  
  WiFi.softAPConfig(local, gateway, subnet);
  if (WiFi.softAP(ssid)) {
    state = STATE_PROG;
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());
    startUDPServer();
  } else {
    state = STATE_ERROR;
    Serial.println("No puedo crear el punto de acceso!");
  }
}

WiFiUDP Udp;

void startUDPServer() {
  unsigned int localPort = 61978;
  Udp.begin(localPort);  
  Serial.printf("Servidor UDP escuchando en el puerto: %d\n", localPort);  
}

char packet[UDP_TX_PACKET_MAX_SIZE + 1]; 

void receivePacket() {
  
  if (!Udp.parsePacket()) return;
  
  packet[Udp.read(packet, UDP_TX_PACKET_MAX_SIZE)] = 0; 

  char *token = strtok(packet, ":\n\r"); 
    
  if (strcmp(packet, "pulse") == 0) {
    Serial.println("Abriendo puerta ...");
    relayPulse(1000);
    return;
  }

  if (strcmp(token, "info") == 0) {
    sendInfo();
    return;
  } 

  // A partir de aqui es necesario que estemos en modo de programación
  if (state != STATE_PROG) return;
    
  if (strcmp(token, "ssid") == 0) {
    if (token = strtok(NULL, ":\n\r")) {
      if (strlen(token) > 1) {
        saveValue(EEPROM_INDEX_WIFI_SSID, token);
        Serial.printf("Guardado el nuevo ssid: %s\n", token);
      }
    }
    return;
  } 

  if (strcmp(token, "passwd") == 0) {
    if (token = strtok(NULL, ":\n\r")) {
      if (strlen(token) > 7) {
        saveValue(EEPROM_INDEX_WIFI_PASSWORD, token);
        Serial.println("Guardada la nueva clave");
      }
    }
    return;
  } 
}

void sendInfo() {
  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());

  // Enviamos nuestro estado
  switch(state) {    
    case STATE_CONNECTED:
      Udp.write("Estado del dispositivo: Conectado\n");
      break;
    case STATE_PROG:
      Udp.write("Estado del dispositivo: Configurando\n");      
      Udp.write("AP SSID: ");
      Udp.write(WiFi.softAPSSID().c_str());
      Udp.write("\n"); 
      Udp.write("AP IP: ");
      Udp.write(WiFi.softAPIP().toString().c_str());
      Udp.write("\n"); 
      break;
    default:  
      Udp.write("Estado del dispositivo: Error\n");
      break;
  }

  // Enviamos nuestra IP de cliente, si estamos conectados a una red WiFi   
  if (WiFi.status() != WL_CONNECTED) {
    Udp.write("No fue posible conectarse a la WiFi\n");
  } else {
    Udp.write("Mi IP es: ");
    Udp.write(WiFi.localIP().toString().c_str());
    Udp.write("\n");  
  }
  
  Udp.endPacket();
}

#define LED         2
#define RELAY       4
#define BUTTON      5
#define PROG        0

// Configura las entradas y salidas
void pinSetup() {  
  pinMode(LED, OUTPUT);
  pinMode(RELAY, OUTPUT);
  pinMode(BUTTON, INPUT);
  pinMode(PROG, INPUT_PULLUP); 

  // Apagamos el led
  ledOff();

  // Apagamos el relé
  relayOff();
}

int ledState = LOW;
unsigned long ledPreviousMillis = 0;

void ledOn() {
  digitalWrite(LED, LOW);
}

void ledOff() {
  digitalWrite(LED, HIGH);
}

void ledLoop() {
  
  unsigned long interval = 0;

  switch(state) {
    case STATE_CONNECTING:
      interval = 100; // Parpadeo corto
      break;  
    case STATE_CONNECTED:
      ledOn();
      break;
    case STATE_PROG:
      interval = 1000; // Parpadeo largo
      break;
    default:  
      ledOff();
  }
  
  if (interval > 0) {
    
    unsigned long currentMillis = millis();

    if (currentMillis - ledPreviousMillis >= interval) {
      ledPreviousMillis = currentMillis;
      
      if (ledState == LOW) {
        ledState = HIGH;
      } else {
        ledState = LOW;
      }
      
      digitalWrite(LED, ledState);
    }
  }
}

void relayOn() {
  digitalWrite(RELAY, HIGH);
}

void relayOff() {
  digitalWrite(RELAY, LOW);
}

void relayPulse(unsigned long ms){
  relayOn();
  delay(ms);
  relayOff();
}

// Comprueba si el dispositivo tiene que entrar en modo de programación
void checkProg() {

  if ((digitalRead(PROG) == LOW) && (state != STATE_PROG)) {
    unsigned long start = millis();
    while (digitalRead(PROG) == LOW) {
      ledLoop(); // Actualizamos el led estado
      // Si se pulsa el boton PROG durante al menos 5 segundos
      if ((millis() - start) > 5000) {
        // Entramos en modo de programación
        state = STATE_PROG;
        // y arrancamos el AP
        startAP(); 
        break;       
      } else {
        delay(10);      
      }
    }
  }
  
}
