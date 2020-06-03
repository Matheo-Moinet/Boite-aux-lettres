#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include "wpa2_enterprise.h"
#include "config.h"

#define TRIG_PIN D3
#define ECHO_PIN D5

const String url = "/trigger/"+trigger+"/with/key/"+key;


int mail_to_send = 0;
unsigned long time_since_last_mail = 0;

double moyenne_distance = 0;


void setup() {
  Serial.begin(9600);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  wifi_station_set_auto_connect(0);
}

void loop() {
  
  Serial.println(mesureDistance());
  if (isNewObjectDetected()){
    mail_to_send ++;
  }

  delay(250);

  if (mail_to_send > 0){
    Serial.print(mail_to_send);
    Serial.println(" mail will be sent");
  }
  
  while (mail_to_send >0 && (millis()-time_since_last_mail)>15000 ){
    Serial.println(millis());
    Serial.println(time_since_last_mail);
    if (sendIFTTTRequest()){
      Serial.println("mail sent");
      mail_to_send --;
    } else {
       Serial.println("failed to send mail, retrying in 2s");
    }
    delay(2000);
  }
}




bool sendIFTTTRequest(){
  // Use WiFiClient class to create TCP connections
  WiFiClientSecure client;
  const int API_TIMEOUT = 10000;

  if (connectWifi() == false){
    return false;
  }

  client.setInsecure();

  Serial.print("connecting to ");
  Serial.println(host);

  int connect_code = client.connect(host, httpsPort);
  Serial.println(connect_code);
  if (!connect_code) {
    Serial.println("connection failed");
    closeConnection(client);
    return false;
  }

  Serial.print("Requesting URL: ");
  Serial.println(url);
  
  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" + 
               "User-Agent: esp8266\r\n" +
               "Connection: close\r\n\r\n");
  delay(500);

  if (mail_to_send == 1){
    closeConnection(client);
  }
  return true;
}




bool connectWifi(){
  if (WiFi.status() == WL_CONNECTED){
    return true;
  }
  if (WPA2_Entreprise_enabled){
    wifi_station_disconnect();
    wifi_set_opmode(STATION_MODE);

    struct station_config stationConf;
    stationConf.bssid_set = 0;  //need not check MAC address of AP
    char char_ssid[32] = "";
    char char_password[64] = "";
    myssid.toCharArray(char_ssid,32);
    password.toCharArray(char_password,64);
    memcpy(&stationConf.ssid, char_ssid, os_strlen(char_ssid));
    memcpy(&stationConf.password, char_password, os_strlen(char_password));
    
    if(!wifi_station_set_config(&stationConf)){
      Serial.print("\r\nset config fail\r\n");
    }
    wifi_station_set_wpa2_enterprise_auth(1);

    char char_username[32] = "";
    username.toCharArray(char_username,32);
    static u8 ent_username[32] = "";
    for (int i=0; i<32; i++){
      ent_username[i] = u8(char_username[i]);
    }
    static u8 ent_password[] = "test";
    for (int i=0; i<64; i++){
      ent_password[i] = u8(char_password[i]);
    }

    
    if(wifi_station_set_enterprise_username (ent_username, sizeof(ent_username))){
      Serial.print("\r\nusername set fail\r\n");
    }
    
    if(wifi_station_set_enterprise_password (ent_password, sizeof(ent_password))){
      Serial.print("\r\npassword set fail\r\n");
    }

    wifi_station_connect();
  
  } else {
    wifi_station_set_wpa2_enterprise_auth(0);
    WiFi.begin(myssid,password);
  }
  
  int cpt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    cpt ++;
    if (cpt >100){
      Serial.println("could not connect to wifi");
      WiFi.disconnect();
      return false;
    }
  }
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Netmask: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());
}


void closeConnection(WiFiClientSecure myClient){
  myClient.stop();
  WiFi.disconnect();
}



bool isNewObjectDetected(){
  double distance = mesureDistance();
  double moyenne_distance_temp = moyenne_distance;
  int nbr_mesure = 1;
  while (distance > moyenne_distance_temp*1.03 || distance < moyenne_distance_temp*0.97){
    moyenne_distance_temp = moyenne_distance_temp*0.9 + distance*0.1;
    nbr_mesure++;
    delay(1);
    distance = mesureDistance();
  }

  if (nbr_mesure>150 || moyenne_distance_temp > moyenne_distance+0.5|| moyenne_distance_temp < moyenne_distance-0.5){
    if (nbr_mesure>150){
      Serial.println("distance mesurment took "+String(nbr_mesure )+" iteration");
    } else if (moyenne_distance_temp < moyenne_distance-0.5){
      Serial.println("detected change");
      time_since_last_mail = millis();
      moyenne_distance = moyenne_distance_temp;
      return true;
    }
    moyenne_distance = moyenne_distance_temp;
  }
  return false;
}





double mesureDistance(){
  double time_to_echo = 0;
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  time_to_echo += pulseIn(ECHO_PIN,HIGH);

  return time_to_echo*0.01731;
  
  /* La formule qui donne la vitesse du son dans l'air (en m/s) en fonction de la température est: 20.05 * sqrt(temperature en Kelvin).
   * En multipliant cette vitesse par le temps que mettent les ultrasons à faire un aller retour, on obtient la distance parcourue par les ultrasons.
   * Puisque la distance qui nous interesse est la distance entre l'émetteur et l'objet, on divise la distance par deux (pour n'avoir la distance que d'un aller)
   * La vitesse du son est en mètres par secondes, or le temps mesuré est en microsecondes. Il faut donc multiplier le temps mesuré par 10^-6.
   * La distance obtenue est alors en mètres. Pour avoir la distance en centimètres, il faut donc multiplier la distance par 10^2.
   * Ici ce n'est pas l'exactitude de la mesure qui importe. On simplifie donc en supposant que la température est constament de 25 degré Celsius, soit 298,15 degrés Kelvin.
   * Ob obtient alors la formule suivante :
   * 
   * distance = (temps * 10^-6 * (20.05 * sqrt(298.15)) )/2 * 10^2 
   * Simplifiable en :
   * distance = temps * 0.01731018814
   */
}
