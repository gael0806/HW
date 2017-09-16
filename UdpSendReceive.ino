/*
 WiFiEsp example: WiFi UDP Send and Receive String

 This sketch wait an UDP packet on localPort using a WiFi shield.

 For more details see: http://yaab-arduino.blogspot.com/p/wifiesp-example-client.html
*/

#include <WiFiEsp.h>
#include <WiFiEspUdp.h>
#include <ArduinoJson.h>
#include <SimpleDHT.h>

// Emulate Serial1 on pins 2/3 if not present
#ifndef HAVE_HWSERIAL1
#include "SoftwareSerial.h"
SoftwareSerial Serial1(2, 3); // RX, TX
#endif

char ssid[] = "ggon";            // your network SSID (name)
char pass[] = "kirinledemon";        // your network password
int status = WL_IDLE_STATUS;     // the Wifi radio's status

unsigned int localPort = 3600;  // local port to listen on

char packetBuffer[300];          // buffer to hold incoming packet

char idArduino[] ="Arduino1";
SimpleDHT11 dht11;
WiFiEspUDP Udp;

void setup() {
  //initialize pin output : IMPORTANT SINON LES DISPOSITIF NE RECEVRONT PAS DE COURANT ELECTRIQUE SUR LES PIN
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  
  // initialize serial for debugging
  Serial.begin(9600);
  // initialize serial for ESP module
  Serial1.begin(9600);
  // initialize ESP module
  WiFi.init(&Serial1);

  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue:
    while (true);
  }

  // attempt to connect to WiFi network
  while ( status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    
    /*  !!!!! IP STATIQUE !!!!!
     *  vous pouvez fixer l'ip du module ESP01 , 
     *  ATTENTION : l'adresse ip dois correspondre à votre réseau (la plus courante sur les boxs/modems -> 192.168.0.X,  X étant un nombre compris entre 2 et 255)
     *  vérifiez aussi que l'adresse ip que vous voulez utiliser n'est pas déja pris par une autre machine (PC, smartphone, etc...)
        vous pouvez décommenter les deux lignes du dessous*/
    IPAddress ipfixe(192, 168, 43 ,101);
    WiFi.config(ipfixe);

    // Connect to WPA/WPA2 network
    status = WiFi.begin(ssid, pass);
  }
  
  Serial.println("Connected to wifi");
  printWifiStatus();

  //Serial.println("\nStarting connection to server...");
  // if you get a connection, report back via serial:
  Udp.begin(localPort);
  
  Serial.print("Listening on port ");
  Serial.println(localPort);

}

void loop() {

  // if there's data available, read a packet
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    //Serial.print("Received packet of size ");
    //Serial.println(packetSize);
    Serial.print("From ");
    IPAddress remoteIp = Udp.remoteIP();
    Serial.print(remoteIp);
    //Serial.print(", port ");
    //Serial.println(Udp.remotePort());

    // read the packet into packetBufffer
    int len = Udp.read(packetBuffer, 200);
    if (len > 0) {
      packetBuffer[len] = 0;
    }
    Serial.println("Contents:");
    Serial.println(packetBuffer);
    //Serial.println("fin du messsage");
        
    StaticJsonBuffer<300> jsonBuffer;
    JsonObject& json = jsonBuffer.parseObject(packetBuffer);
    if (!json.success()) {
      Serial.println("parseObject() failed");
      return;
    }
    
    //lecture de la clé "action" pour décider de l'action a effectuer , sois un renvoie des valeurs spécifié dans le json, sois un changement d'état dans une pin 
    /*
     * RENVOIE DE TOUTES LES VALEURS DES DISPOSITIFS
     */
    if ( json["action"] == "sendValues" ) {
      //création du json avec l'identifiant de l'arduino et les états des différents dispositifs 
      JsonObject& object = jsonBuffer.createObject();
      for (int i=4; i <= 13; i++){
        //cas special pin9 : on controle si dispositif => capteur humidité
        if(i==9){
          byte temperature = 0;
          byte humidity = 0;
          if (dht11.read(i, &temperature, &humidity, NULL)) {
            //Serial.print("Read DHT11 failed");
          } else {
            String c = String(i);
            object.set(c, (int)humidity);
          }
        }
        //on enregistre toute les valeurs des pins de l'arduino
        else {
          String c = String(i);
          object.set(c, digitalRead(i));
        }
      }
      for (int j=0; j <= 5 ; j++) {
        String d = String(j+14);
        object.set(d, analogRead(j));
      }
      object.set("idArduino",idArduino);
      //le json va être contenu dans un Buffer qui sera envoyé a la suite (Buffer = tableau de caractères) 
      object.printTo(packetBuffer); 
      Serial.println("json");
      Serial.println(packetBuffer);

      // send a reply, to the IP address and port that sent us the packet we received
      Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
      Udp.write(packetBuffer);
      Udp.endPacket();
    }

    /*
     * CHANGEMENTS DES ETATS DES DISPOSITIFS
     */
    else if( json["action"] == "writeValues") {
      //Serial.println("WRITE VALUES !!!");
      json.remove("action");
      json.remove("idArduino");
      
      JsonObject& object = jsonBuffer.createObject();
      object.set("idArduino",idArduino);

      //apres avoir enlevé les valeurs inutiles du json, on parcoure les différentes valeurs clé->"pin" et valeur->"état"
      for (auto pinEtat : json) {
        char* cle = pinEtat.key;
        int pin = atoi(cle);
        int changeEtat = pinEtat.value;
 
        int readinput = digitalRead(pin);
        if( (readinput== 1 && changeEtat==0) || (readinput==0 && changeEtat==1) ) {
          //Serial.println(pin);
          //Serial.println(changeEtat); 
          if (changeEtat == 0) {
            digitalWrite(pin, LOW);
          }
          else {
            digitalWrite(pin, HIGH);
          }
          //object.set(cle, true);
        }
        else
        {
          //object.set(cle, false);
        }
      }
      //object.set("message", "états changés pour les pins");
      //le json va être contenu dans un Buffer qui sera envoyé a la suite (Buffer = tableau de caractères) 
      //object.printTo(packetBuffer);
      //Serial.println(ReplyBuffer);
    }
    
    /*
     * !A SUPPRIMER! TEST CHANGEMENT D'UN DISPOSITIF
     */
    /*else {
      char* pin  = json["action"];
      int changeEtat = atoi(json[pin]);
      int intpin = atoi(intpin);
      Serial.println(pin);
      int readinput = digitalRead(intpin);
      
      //controle si on devra bien changer l'etat du dispositif
      if( (readinput== 1 && changeEtat==0) || (readinput==0 && changeEtat==1) ) {
          if (changeEtat == 0) {
            digitalWrite(intpin, LOW);
          }
          else {
            digitalWrite(intpin, HIGH);
          }
          char prepareMessage[40];
          char message1[] = "état changé pour pin";
          Serial.println(message1);
          //concaténation de tableau de caractères (tableau de caractère = chaine de caractère)
          sprintf(ReplyBuffer,"%s : %s %s", idArduino, message1,pin); 
          Serial.println(ReplyBuffer);
      }
    }*/
  }
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // print the received signal strength:
  //Serial.print("signal strength (RSSI):");
  //Serial.print(WiFi.RSSI());
  //Serial.println(" dBm");
}
