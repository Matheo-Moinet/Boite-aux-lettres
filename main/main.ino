#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include "wpa2_enterprise.h"
#include "config.h"

#define TRIG_PIN D3
#define ECHO_PIN D5
#define LED_PIN D8
#define BUTTON_PIN D7

#define LED_OFF 0
#define LED_ON 1
#define LED_BLINK 2
#define LED_75_ON 3
#define LED_25_ON 4
#define LED_BLINK_HALF 5
#define LED_BLINK_1T 6
#define LED_BLINK_2T 7
#define LED_BLINK_EVERY_10T 8
#define LED_BLINK_XT 9


const String url = "/trigger/" + trigger + "/with/key/" + key;


int nbr_of_new_letters = 0;
int total_nbr_of_letters = 0;
unsigned long time_of_last_letter = 0;
bool letter_detected = false;

double previous_distance = 0;
double distance_bottom = 0;

int led_state = LED_BLINK;
bool do_chip_reboot = false;

WiFiClientSecure client;

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  wifi_station_set_auto_connect(0);
  distance_bottom = mesureDistance();  
  WiFi.mode( WIFI_OFF );
  WiFi.forceSleepBegin();
  Serial.println("WiFi is down");
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), rebootChip, RISING);
  do_chip_reboot = false;
  timer1_isr_init();
  timer1_attachInterrupt(ledInterrupt);
  timer1_enable(TIM_DIV256, TIM_EDGE, TIM_LOOP);
  timer1_write(1000);
}

void loop() {
  delay(0); //To allow for background WiFi related operations to be performed

  if (do_chip_reboot){
    led_state = LED_OFF;
    Serial.println("Button pressed, the chip will reboot in " + String(button_timer) + "s");
    delay(button_timer*1000);
    ESP.restart();
    do_chip_reboot = false;
  }


  if ((millis() - time_of_last_letter) > time_between_two_letters_are_detected*1000 && isNewObjectDetected()) {
    nbr_of_new_letters ++;
    total_nbr_of_letters ++;
  }

  delay(time_between_two_measurements);

  if (nbr_of_new_letters > 0) {
    led_state = LED_75_ON ;
    int time_temp = int(time_before_weebhook_is_triggered) - int((millis() - time_of_last_letter)/1000);
    if (time_temp>=0){
      Serial.print(" Weebhook will be triggered in ");
      Serial.print(time_temp);
      Serial.print("s, ");
      Serial.print(nbr_of_new_letters);
      Serial.println(" letter(s) detected");
    } else {
      Serial.println("Connection to WiFi is taking : "+ String(-time_temp) + "s");
    }
  } else {
    if (total_nbr_of_letters > 0){
      led_state = LED_BLINK_2T ;
    } else {
      led_state = LED_BLINK_EVERY_10T ;
    }
  }

  if (nbr_of_new_letters > 0 && (millis() - time_of_last_letter) > time_before_weebhook_is_triggered * 1000 ) {
    if (wifi_get_opmode() == WIFI_OFF ){
      Serial.println("Starting WiFi connection");
      connectWifi();
    } else if (WiFi.status() == WL_CONNECTED){
      led_state = LED_ON ;
      Serial.println("WiFi connected");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      Serial.print("Netmask: ");
      Serial.println(WiFi.subnetMask());
      Serial.print("Gateway: ");
      Serial.println(WiFi.gatewayIP());
      Serial.println("Starting weebhook triggering");
      if (sendIFTTTRequest()) {
        Serial.println("Weebhook triggered");
        nbr_of_new_letters = 0;
      } else {
        Serial.println("Failed to trigger weebhook, retrying in 2s");
      }
      delay(2000);
    } else {
      led_state = LED_BLINK_HALF ;
      Serial.print("CONNECTING TO WIFI : ");
      Serial.println(wifi_station_get_connect_status() );
      /*  STATION_IDLE  = 0,
       *  STATION_CONNECTING = 1,
       *  STATION_WRONG_PASSWORD = 2,
       *  STATION_NO_AP_FOUND = 3,
       *  STATION_CONNECT_FAIL = 4,
       *  STATION_GOT_IP = 5
       */
    }
  }
  Serial.println();
}




bool sendIFTTTRequest() {
  // Use WiFiClient class to create TCP connections
  //WiFiClientSecure client;
  //const int API_TIMEOUT = 10000;
  /*
  if (connectWifi() == false) {
    return false;
  }
  */
  client.setInsecure();

  Serial.print("Connecting to ");
  Serial.println(host);

  int connect_code = client.connect(host, httpsPort);
  Serial.println(connect_code);
  if (!connect_code) {
    Serial.println("Connection failed");
    closeConnection(client);
    return false;
  }

  Serial.print("Posting json to URL: ");
  Serial.println(url);

  String json = "{\"value1\": "+ String(nbr_of_new_letters) +",\"value2\": "+ String(total_nbr_of_letters) +",\"value3\": 100}";
  Serial.print("JSON content: ");
  Serial.println(json);
 
  // This will send the request to the server
  client.print("GET " + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: esp8266\r\n" +
               "Content-Type: application/json\r\n" +
               "Accept: */*\r\n"+
               "Content-Length: "+json.length()+"\r\n"+
               "\r\n" +
               json);
  delay(500);

  closeConnection(client);
  return true;
}




bool connectWifi() {
  
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  WiFi.forceSleepWake();
  delay(1);
  wifi_set_opmode_current(STATION_MODE);
  
  if (WPA2_Entreprise_enabled) {
    wifi_station_disconnect();

    struct station_config stationConf;
    stationConf.bssid_set = 0;  //need not check MAC address of AP
    char char_ssid[32] = "";
    char char_password[64] = "";
    ssid.toCharArray(char_ssid, 32);
    password.toCharArray(char_password, 64);
    memcpy(&stationConf.ssid, char_ssid, os_strlen(char_ssid));
    memcpy(&stationConf.password, char_password, os_strlen(char_password));

    if (!wifi_station_set_config(&stationConf)) {
      Serial.print("\r\nset config fail\r\n");
    }
    wifi_station_set_wpa2_enterprise_auth(1);

    char char_username[32] = "";
    username.toCharArray(char_username, 32);
    static u8 ent_username[32] = "";
    for (int i = 0; i < 32; i++) {
      ent_username[i] = u8(char_username[i]);
    }
    static u8 ent_password[] = "test";
    for (int i = 0; i < 64; i++) {
      ent_password[i] = u8(char_password[i]);
    }


    if (wifi_station_set_enterprise_username (ent_username, sizeof(ent_username))) {
      Serial.print("\r\nusername set fail\r\n");
    }

    if (wifi_station_set_enterprise_password (ent_password, sizeof(ent_password))) {
      Serial.print("\r\npassword set fail\r\n");
    }

    wifi_station_connect();

  } else {
    wifi_station_set_wpa2_enterprise_auth(0);
    WiFi.begin(ssid, password);
  }
}


void closeConnection(WiFiClient myClient) {
  myClient.stop();
  WiFi.disconnect();
  Serial.println("Wifi connection closed");
  WiFi.mode( WIFI_OFF );
  WiFi.forceSleepBegin();
  Serial.println("WiFi is down");
}



bool isNewObjectDetected() {
  double distance = mesureDistance();
  Serial.print("Measured distance is : ");
  Serial.print(distance);
  Serial.println(" cm");
  Serial.print("Measured distance was : ");
  Serial.print(previous_distance);
  Serial.println(" cm");
  Serial.print("Distance from bottom is: ");
  Serial.print(distance_bottom);
  Serial.println(" cm");
  double moyenne_distance = previous_distance;
  double last_10_distances[10];
  
  int nbr_mesure = 1;
  if (distance > moyenne_distance * 1.01 || distance < moyenne_distance * 0.99) {
    Serial.println("Measured distance is too far from previous distance, more measurments will be done");
    last_10_distances[1] = distance;
    for (int i = 1; i < 10; i++) {
      last_10_distances[i] = 10000;
    }
  }
  while (nbr_mesure < 100 && distance > moyenne_distance * 1.01 || distance < moyenne_distance * 0.99) {
    moyenne_distance = 0;
    for (int i = 0; i < 10; i++) {
      moyenne_distance += last_10_distances[i];
    }
    moyenne_distance = moyenne_distance / 10;
    last_10_distances[nbr_mesure % 10] = distance;
    nbr_mesure++;
    delayMicroseconds(min_time_before_next_ultrasonic);
    //Serial.println(moyenne_distance);
    distance = mesureDistance();
  }

  Serial.print("Distance was found to be ");
  Serial.print(moyenne_distance);
  Serial.print(" after ");
  Serial.print(nbr_mesure);
  Serial.println(" measurements");

  distance_bottom = distance_bottom*0.99 + moyenne_distance * 0.01;
  
  if (letter_detected == false){
    if (nbr_mesure == 100 || moyenne_distance > (distance_bottom + distance_change_to_detect) || moyenne_distance < (distance_bottom - distance_change_to_detect)) {
      if (nbr_mesure == 100) {
        Serial.println("distance mesurment took " + String(nbr_mesure ) + " iteration");
      } else if (moyenne_distance < distance_bottom - 0.5) {
        Serial.print("Measured distance is more than ");
        Serial.print(distance_change_to_detect);
        Serial.println(" cm shorter than previous distance : a letter has been detected");
        time_of_last_letter = millis();
        previous_distance = moyenne_distance;
        letter_detected = true;
        return true;
      }
    }
  } else {
    if (moyenne_distance > distance_bottom - 0.5){
      Serial.println("The letter is no longer detected");
      letter_detected = false;
    }
  }
    
  previous_distance = moyenne_distance;
  return false;
}





double mesureDistance() {
  double time_to_echo = 0;
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  time_to_echo += pulseIn(ECHO_PIN, HIGH);

  return time_to_echo * 0.01731;

  /* La formule qui donne la vitesse du son dans l'air (en m/s) en fonction de la température est: 20.05 * sqrt(temperature en Kelvin).
     En multipliant cette vitesse par le temps que mettent les ultrasons à faire un aller retour, on obtient la distance parcourue par les ultrasons.
     Puisque la distance qui nous interesse est la distance entre l'émetteur et l'objet, on divise la distance par deux (pour n'avoir la distance que d'un aller)
     La vitesse du son est en mètres par secondes, or le temps mesuré est en microsecondes. Il faut donc multiplier le temps mesuré par 10^-6.
     La distance obtenue est alors en mètres. Pour avoir la distance en centimètres, il faut donc multiplier la distance par 10^2.
     Ici ce n'est pas l'exactitude de la mesure qui importe. On simplifie donc en supposant que la température est constament de 25 degré Celsius, soit 298,15 degrés Kelvin.
     Ob obtient alors la formule suivante :

     distance = (temps * 10^-6 * (20.05 * sqrt(298.15)) )/2 * 10^2
     Simplifiable en : distance = temps * 0.01731018814
  */
}


ICACHE_RAM_ATTR void ledInterrupt(){
  static int led_cpt = 0;
  led_cpt++;
  led_cpt = led_cpt%led_timer_max;
  static int nbr_repete = 0;
  switch (led_state){
    case LED_OFF :
        digitalWrite(LED_PIN, LOW);
      break;
    case LED_ON :
        digitalWrite(LED_PIN, HIGH);
      break;
    case LED_BLINK :
      if (led_cpt > led_timer_max*0.5){
        digitalWrite(LED_PIN, HIGH);
      } else {
        digitalWrite(LED_PIN, LOW);
      }
      break;
    case LED_75_ON :
      if (led_cpt < led_timer_max*0.75){
        digitalWrite(LED_PIN, HIGH);
      } else {
        digitalWrite(LED_PIN, LOW);
      }
      break;
    case LED_25_ON:
      if (led_cpt < led_timer_max*0.25){
        digitalWrite(LED_PIN, HIGH);
      } else {
        digitalWrite(LED_PIN, LOW);
      }
      break;
    case LED_BLINK_HALF : 
      if (led_cpt%(int(led_timer_max*0.5)) < led_timer_max*0.25){
        digitalWrite(LED_PIN, HIGH);
      } else {
        digitalWrite(LED_PIN, LOW);
      }
      break;
    case LED_BLINK_1T:
      if (led_cpt < led_timer_max*0.05){
        digitalWrite(LED_PIN, HIGH);
      } else {
        digitalWrite(LED_PIN, LOW);
      }
      break;
    case LED_BLINK_2T:
      if (led_cpt < led_timer_max*0.05 || (led_cpt > led_timer_max*0.10 && led_cpt < led_timer_max*0.15)){
        digitalWrite(LED_PIN, HIGH);
      } else {
        digitalWrite(LED_PIN, LOW);
      }
      break;
    case LED_BLINK_EVERY_10T :
      if (led_cpt == 0){
        nbr_repete +=1;
      }
      if (nbr_repete >=10){
        if (led_cpt < led_timer_max*0.05){
          digitalWrite(LED_PIN, HIGH);
        } else {
          digitalWrite(LED_PIN, LOW);
          nbr_repete = 0;
        }
      }
      break;
    case LED_BLINK_XT :
      //TODO
      break;
  }
}


ICACHE_RAM_ATTR void rebootChip(){
  do_chip_reboot = true;
}
