#include <Arduino.h>
#include <motor.h>
#include <WiFi.h>
#include <FastLED.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP32Time.h>
#include <time.h>
#include <sys/time.h>
#include "esp_sntp.h"
#include "logger.h"

ESP32Time rtc;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
CRGBArray<8> leds;
TaskHandle_t ReadRtcTask;
void updateTime();
void moveServo(char hand, uint8_t index, uint8_t degree);
void readRtc(void *parameter);
const int bufferLength = 200;
const int commandLenght = 200;

struct Command {
	uint32_t evaluteTime;
	char command[commandLenght];
};
volatile Command commandBuffer[bufferLength];
int commandsCount = 0;

Driver linear;

#include <commandProcessing.h>

#include <WebSocketsClient.h>

#include <SoftwareSerial.h>

SoftwareSerial driverR;
SoftwareSerial driverL;
SoftwareSerial radio;

uint32_t tmr;
boolean flag;


#define pcSerial Serial

const char* ssid     = "CyberTheater";
const char* password = "intelnuc";
char path[] = "/ws/elsa";
// char host[] = "192.168.88.10"; // комп димы
char host[] = "192.168.88.14"; // асус лабный
const uint8_t port = 80;

WebsocketWorker wsHandler;

WebSocketsClient webSocket;
#include <string>

void moveServo(char hand, uint8_t index, uint8_t degree){
  String command = "s" + String(index) + " " + "a w" + String(degree) + " ";
  if(hand == 'r') driverR.print(command);
  if(hand == 'l') driverL.print(command);
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

	switch(type) {
		case WStype_DISCONNECTED:
			pcSerial.printf("[WSc] Disconnected!\n");
			break;
		case WStype_CONNECTED:
			pcSerial.printf("[WSc] Connected to url: %s\n", payload);
			// send message to server when Connected
			webSocket.sendTXT("Connected");
			break;
		case WStype_TEXT:
			pcSerial.printf("[WSc] Got text: %s\n", payload);
      		wsHandler.processCommand((char*)payload);
			// send message to server
			// webSocket.sendTXT("message here");
			break;
		default:
			break;
	}
}

void setup() {

	// Линейный привод на подъём рук
	// linear.init(25, 4, 2, 0);

	driverR.begin(9600, SWSERIAL_8N1, 16, 17, false);
	driverL.begin(9600, SWSERIAL_8N1, 4, 13, false); // проверить пины 

	pinMode(33, OUTPUT); // compressor
    pinMode(25, OUTPUT); // valve
	  
    delay(100);

	pcSerial.begin(115200);
	pcSerial.println("Setup");
	pcSerial.setDebugOutput(true);

  	wsHandler.init();
	WiFi.mode(WIFI_STA);
	WiFi.setSleep(false); 
	WiFi.begin(ssid, password);
	WiFi.setTxPower(WIFI_POWER_19_5dBm);

	// Проверяем статус. Если нет соединения, то выводим сообщение о подключении
	int counter = 0;
	while (!WiFi.isConnected()) {
		pcSerial.println("Connecting to WiFi...");
		counter++;
		if(!(counter % 6)) WiFi.reconnect();
		delay(500);
	}

	updateTime();
	xTaskCreatePinnedToCore(
		readRtc,                  /* pvTaskCode */
		"readRtc",            /* pcName */
		10000,                   /* usStackDepth */
		NULL,                   /* pvParameters */
		1,                      /* uxPriority */
		&ReadRtcTask,                 /* pxCreatedTask */
	0);                     /* xCoreID */


	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());

	Serial.print("Hostname: ");
	Serial.println(WiFi.getHostname());

	Serial.print("ESP Mac Address: ");
	Serial.println(WiFi.macAddress());

	Serial.print("Subnet Mask: ");
	Serial.println(WiFi.subnetMask());

	Serial.print("Gateway IP: ");
	Serial.println(WiFi.gatewayIP());
	Serial.print("DNS: ");
	Serial.println(WiFi.dnsIP());
  	Serial.print("RSSI: ");
	Serial.println(WiFi.RSSI());
	pcSerial.println();
	pcSerial.println("Connected.");
	pcSerial.println();

	// server address, port and URL
	webSocket.begin(host, port, path);

	// event handler
	webSocket.onEvent(webSocketEvent);

	// try again every 500ms if connection has failed
	webSocket.setReconnectInterval(500);
	Serial.println("end setup");
}

void tasker () {
 uint32_t currentTime = rtc.getMillis();
 for(int i = 0; i < commandsCount; i++) {
	if(commandBuffer[i].evaluteTime == currentTime);
 }
}


void loop() {
  webSocket.loop();
}


void updateTime() {
  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }
  uint32_t time = timeClient.getEpochTime();
  rtc.setTime(time);
  Serial.println(timeClient.getFormattedTime());
  Serial.println(time);
}

void readRtc(void *parameter) {
	while(true){
		//Serial.println(rtc.getEpoch());
		wsHandler.scheduler();
		vTaskDelay(10);
	}
}

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}
void ntp2 (){
//    sntp_setservername(0, CONFIG_SNTP_TIME_SERVER);
 	sntp_set_time_sync_notification_cb(time_sync_notification_cb);
}