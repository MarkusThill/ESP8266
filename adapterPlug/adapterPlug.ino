#include <ESP8266WiFi.h>
 #include <PubSubClient.h>
 #include <string.h>
 #define SLEEP_DELAY_IN_SECONDS  10
 #define TOPIC_ROOT "diedackel/f"
 #define DEVICE_ID  "plug-02"
 #define SLASH "/"
 #define CMD_RELAIS_ON "device=on"
 #define CMD_RELAIS_OFF "device=off"
 #define STR_CONFIRM_STATE "confirmNewState="


const char* ssid = "FusulFusul"; // dlink-C148, FusulFusul
const char* password = ""; // , 
const char* mqtt_server = "io.adafruit.com";
const char* mqtt_username = "";
const char* mqtt_password = "";
const char* mqtt_pubs_topic = TOPIC_ROOT SLASH DEVICE_ID;
const char* mqtt_subs_topic = TOPIC_ROOT SLASH DEVICE_ID;
WiFiClient espClient;
PubSubClient client(espClient);

// GPIO-pin 2, used for activating the relais
int GPIO2 = 2;
// GPIO-pin 0, used as input for manually turning on/off the relais
int GPIO0 = 0;
// State of the relais
int state = LOW;
// Flag used for debouncing the push-button
bool debounce = false;
/*
 * Called once during the initialization phase after reset
 ****/
void setup() {
  // First allow interrupts. So we can turn on the plug, even if we dont have WiFi
  // GPIO0 will be an input that is used to turn on the relais using a button
  pinMode(GPIO0, INPUT);
  attachInterrupt(GPIO0, fallingEdge, FALLING);
  
  // setup serial port
  Serial.begin(115200);
  // setup WiFi
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  // setup pins
  // GPIO2 will be used to activate the relais
  pinMode(GPIO2, OUTPUT);
  //pinMode(LED_BUILTIN, OUTPUT); // Does not work. Maybe same pin as TX/RX line?
}
void fallingEdge() {
  if(!debounce) {
    state = !state;
    digitalWrite(GPIO2, state);
    debounce = true;
  }
  // Do not call other functions in this interrupt (such as delay, etc.). This only causes problems.
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  int i = 0;
  while (WiFi.status() != WL_CONNECTED && i++ < 10) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
 /*
  * MQTT callback function. It is always called with we receive a message on the subscribed topics.
  * The topic itself is given as a parameter of the function.
  ****/
void callback(char* topic, byte* payload, unsigned int length) {
  char msg[50];
  int i;
  char string[50];
  char lowerString[50];

  sprintf(msg, "Message arrived [%s] ", topic);
  Serial.println(msg);
  
  // Copy the payload into a C-String and convert all letters into lower-case
  for (i = 0; i < length; i++) {
    string[i] = (char)payload[i];
    lowerString[i] = tolower(string[i]);
  }
  string[i]=0;
  lowerString[i]=0;
  Serial.println(lowerString);

  // If the paylaod contains one of the predefined messages then turn on/off the relais
  uint8_t newState = -1;
  if(strcmp(lowerString, CMD_RELAIS_ON) == 0)
     newState = HIGH;
  else if(strcmp(lowerString, CMD_RELAIS_OFF) == 0)
     newState = LOW;

   
  if(newState == LOW || newState == HIGH) {
    state = newState;
    digitalWrite(GPIO2, state);
    sprintf(msg, "%s%d", STR_CONFIRM_STATE, state);
    Serial.println(msg);
    client.publish(mqtt_pubs_topic, msg);
  }  

  Serial.println();
}
/*
 * Can be used to reconnect to the MQTT-Broker, in case we loose the connection inbetween
 ****/
void reconnect() {
  // Loop until we're reconnected
  int i = 0;
  while (!client.connected() && i++<10) {
    Serial.println("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(DEVICE_ID, mqtt_username, mqtt_password)) {
      Serial.println("connected");
      // Subscribe to a topic
      if(client.subscribe(mqtt_subs_topic))
        Serial.println("Subscribed to the specified topic");
      else
        Serial.println("Failed to subscribe to the specified topic");
      client.publish(mqtt_pubs_topic, "Hello, here I am...");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" trying again in 500 mseconds");
      // Wait 500 seconds before retrying
      delay(500);
    }
  }
}
/*
 * This function will be called after setup() in an infinite loop
 ****/
void loop() {
  static int loopCounter = 0;
  loopCounter++;
  char msg[50];
  if(WiFi.status() != WL_CONNECTED) {
    setup_wifi();
  }
  else if (!client.connected()) {
    reconnect();
  }

  if(loopCounter % 200 == 0) { // Message every minute
    Serial.println("I am still alive...") ;
  }
    
  // send message to the MQTT topic
  //client.publish(mqtt_pubs_topic, "I am still alive...");

  // Example of a retained message
  //client.publish(mqtt_topic, "This will be retained", true);
  client.loop();
  delay(300);
  if (client.connected() && (debounce || loopCounter % 200 == 0)) {
    sprintf(msg, "%s%d", STR_CONFIRM_STATE, state);
    client.publish(mqtt_pubs_topic, msg, true);
  }
  // After two seconds we can be sure that the button is debounced. The interrupt can also
  // come during the delay, but it unlikely to happen exactly (few ns) before the delay is over.
  debounce = false;
}
