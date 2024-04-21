

#include <Arduino.h>
#include <ArduinoJson.h>			// Library for JSON parsing. This is a more efficient alternative to the arduino built in JSON library

#define ELEGANTOTA_USE_ASYNC_WEBSERVER 1	// Tell ElegantOTA to use the AsyncWebServer
#include <WiFi.h>
#include <ESPAsyncWebServer.h>		// This will automatically use whatever core is available, at priority 3
#include <ElegantOTA.h>				// Library for Over The Air (OTA) updates

// #include <SPIFFS.h>



#define SERIAL_ENABLED 1 // Set to 1 to enable Serial Monitor Debugging]

#define NUM_SENSORS 8


//======================================================================================================
//	Hardware Connections
//======================================================================================================

// const gpio_num_t echoPins[] = {GPIO_NUM_36, GPIO_NUM_39, GPIO_NUM_34, GPIO_NUM_35, GPIO_NUM_32, GPIO_NUM_33, GPIO_NUM_25, GPIO_NUM_26};
const gpio_num_t echoPins[] = {GPIO_NUM_26, GPIO_NUM_25, GPIO_NUM_33, GPIO_NUM_32, GPIO_NUM_35, GPIO_NUM_34, GPIO_NUM_39, GPIO_NUM_36};
// const gpio_num_t echoPins[] = {GPIO_NUM_25, GPIO_NUM_33, GPIO_NUM_34, GPIO_NUM_39, GPIO_NUM_36};


const gpio_num_t MuxAPin = GPIO_NUM_27;// LSB of the Mux
const gpio_num_t MuxBPin = GPIO_NUM_14;
const gpio_num_t MuxCPin = GPIO_NUM_12;// MSB of the Mux
const gpio_num_t MuxSigPin = GPIO_NUM_13;

// const gpio_num_t motorPins[] = {GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5, GPIO_NUM_17, GPIO_NUM_16, GPIO_NUM_4, GPIO_NUM_2, GPIO_NUM_15};
const gpio_num_t motorPins[] = {GPIO_NUM_15, GPIO_NUM_2, GPIO_NUM_4, GPIO_NUM_16, GPIO_NUM_17, GPIO_NUM_5, GPIO_NUM_18, GPIO_NUM_19};
// const gpio_num_t motorPins[] = {GPIO_NUM_2, GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_18, GPIO_NUM_19};


//======================================================================================================
//	Enums
//======================================================================================================







//======================================================================================================
//	Structs
//======================================================================================================





//======================================================================================================
//	Variables
//======================================================================================================

#define WIFI_SSID "COLLIN-LAPTOP" // Set the SSID of the WiFi network
#define WIFI_PASSWORD "blinkyblinky" // Set the password of the WiFi network

// Web Server Variables
AsyncWebServer server(80);// Create AsyncWebServer object on port 80
AsyncWebSocket ws("/ws");// Create a WebSocket object on path "ip:80/ws"


// Web UI Data
JsonDocument messageJSON;			// JSON Object to store data to send to the web UI
SemaphoreHandle_t jsonSemaphore;	// Semaphore to control access to the JSON object

// Task Variables
TaskHandle_t sendDataToUITask;

// Event Group for Tasks and related Defines
typedef enum{
	SEND_DATA = 0x01
} EventFlags;

EventGroupHandle_t mainEventGroup;


// Distance Sensor Variables
long duration[NUM_SENSORS], cm[NUM_SENSORS], inches[NUM_SENSORS];

const uint16_t MaxDurration = 30000;// Maximum duration for the pulseIn function to wait for a pulse. This is in microseconds


// Motor Variables
const uint16_t motorPWMFreq = 1000;				// Frequency for the PWM Output signal to the motors. This will also work with up to a max resolution of 13 bits
const uint8_t motorPWMResolution = 12;			// 10 bit resolution for PWM. We may want to try a higher resolution at some point
const uint16_t motorAbsMaxSpeed = (1 << (motorPWMResolution)) - 1;// Absolute maximum speed of the motor. 1023 for 10 bit PWM, 4095 for 12 bit PWM. This is the maximum value that can be sent to the motor driver


const TickType_t readSensorsInterval = pdMS_TO_TICKS(500);// Time in ms to wait between reading the sensors
TickType_t lastLoopStartTime = 0;// Time in ms that the last loop started


//======================================================================================================
//	Setup Functions
//======================================================================================================

// Function to setup the WiFi connection
void initWiFi(){
	// WiFi.mode(WIFI_AP);// Set WiFi mode to Station (Connecting to some other access point, ie a laptop's hotspot)
	// WiFi.config(IPAddress(192,168,137,2), IPAddress(192,168,137,1),IPAddress(255,255,255,0));// Set Static IP Address (IP, Gateway, Subnet Mask). NOTE: This line does not work on Laptop Hotspot
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);// Start Wifi Connection
	// WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);// Start Wifi Connection
	while(WiFi.status() != WL_CONNECTED){// Wait for WiFi to connect
		delay(500);
		#if SERIAL_ENABLED
			Serial.print(".");
		#endif
	}
	#if SERIAL_ENABLED
		Serial.println("\nConnected to WiFi\n");
	#endif
}




// Sensor ID's
const uint8_t sensors[NUM_SENSORS] = {0, 1, 2, 3, 4, 5, 6, 7};



// // Initialize SPIFFS
// void initSPIFFS() {
// 	if (!SPIFFS.begin(true)) {
// 		#if SERIAL_ENABLED
// 			Serial.println("An Error has occurred while mounting SPIFFS");
// 		#endif
// 	}
// 	#if SERIAL_ENABLED
// 		Serial.println("SPIFFS mounted successfully");
// 	#endif
// }





//======================================================================================================
//	Web Socket Server Functions
//======================================================================================================

// Function to send data to all clients on the web UI
void notifyClients(String data) {
	#if SERIAL_ENABLED
		Serial.println("Sending Data to Web UI: " + data + "\n");
	#endif
	ws.textAll(data);
}





// Function to handle the verified content of the WebSocket Messages. This is run in the Web Server thread with priority 3
void selectCommand(char* msg){
	JsonDocument doc; // Create a JSON document to store the message
	DeserializationError error = deserializeJson(doc, msg);// Deserialize the JSON message

	if(error){// If there was an error deserializing the JSON message
		#if SERIAL_ENABLED
			Serial.print(F("deserializeJson() failed: "));
			Serial.println(error.c_str());
		#endif
		ws.textAll("{\"message\": \"Invalid JSON received\"}");// Send a message to the web UI that the JSON was invalid
		return;
	}

	ws.textAll("{\"message\": \"JSON Message Recieved\"}");// Send a message to the web UI that the JSON was invalid
	// const char* cmd = doc["cmd"];// Get the command from the JSON message

	// Check what the message is and set the appropriate bit in the event group
	{
		// If the command is not recognized, send a message to the web UI
		ws.textAll("{\"message\": \"Invalid Command Received\"}");
		// xEventGroupSetBits(mainEventGroup, SEND_DATA);// Set the SEND_DATA bit to send data to the web UI
	}
}



// AwsEventHandler Function. This largely comes from the ESPAsyncWebServer Documentation example.
void onEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
	if(type == WS_EVT_CONNECT){
		//client connected
		#if SERIAL_ENABLED
			Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
		#endif
		client->printf("{\"message\": \"Hello Client %u :)\"}", client->id());
		client->ping();
	} else if(type == WS_EVT_DISCONNECT){
		//client disconnected
		#if SERIAL_ENABLED
			Serial.printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id(), client->id());
		#endif
	} else if(type == WS_EVT_ERROR){
		//error was received from the other end
		#if SERIAL_ENABLED
			Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
		#endif
	} else if(type == WS_EVT_PONG){
		//pong message was received (in response to a ping request maybe)
		#if SERIAL_ENABLED
			Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
		#endif
	} else if(type == WS_EVT_DATA){
		//data packet
		AwsFrameInfo * info = (AwsFrameInfo*)arg;
		if(info->final && info->index == 0 && info->len == len){
			//the whole message is in a single frame and we got all of it's data
			#if SERIAL_ENABLED
				Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);
			#endif
			if(info->opcode == WS_TEXT){
				data[len] = 0;// Set last character to null terminator
				#if SERIAL_ENABLED
					Serial.printf("%s\n", (char*)data);
				#endif
				selectCommand((char*)data);
			} else {
				for(size_t i=0; i < info->len; i++){
					#if SERIAL_ENABLED
						Serial.printf("%02x ", data[i]);
					#endif
				}
				#if SERIAL_ENABLED
					Serial.printf("\n");
				#endif
			}
			// if(info->opcode == WS_TEXT)
			// 	client->text("{\"message\": \"I got your text message\"}");
			// else
			// 	client->binary("{\"message\": \"I got your binary message\"}");
		} else {
			//message is comprised of multiple frames or the frame is split into multiple packets
			if(info->index == 0){
				if(info->num == 0){
					#if SERIAL_ENABLED
						Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
					#endif
				}
				#if SERIAL_ENABLED
					Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
				#endif
			}

			#if SERIAL_ENABLED
				Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);
			#endif
			if(info->message_opcode == WS_TEXT){
				data[len] = 0;
				#if SERIAL_ENABLED
					Serial.printf("%s\n", (char*)data);
				#endif
			} else {
				for(size_t i=0; i < len; i++){
					#if SERIAL_ENABLED
						Serial.printf("%02x ", data[i]);
					#endif
				}
				#if SERIAL_ENABLED
					Serial.printf("\n");
				#endif
			}

			if((info->index + len) == info->len){
				#if SERIAL_ENABLED
					Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
				#endif
				if(info->final){
					#if SERIAL_ENABLED
						Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
					#endif
					// if(info->message_opcode == WS_TEXT)
					// 	client->text("{\"message\": \"I got your text message\"}");
					// else
					// 	client->binary("{\"message\": \"I got your binary message\"}");
				}
			}
		}
	}
}



void initWebSocket() {
	ws.onEvent(onEvent);
	server.addHandler(&ws);
}




// New Printf function for the program to use for logging via the web UI console, rather than serial
int asyncLogPrintf(const char *format, va_list args) {
	char buffer[256]; // Buffer to hold the formatted message
	vsnprintf(buffer, sizeof(buffer), format, args); // Format the message
	String message = "{\"message\": \"" + String(buffer) + "\"}"; // Add the JSON structure
	message.replace("\n", "\\n"); // Replace newline characters with the escape sequence, as newlines are not valid in the JSON string

	ws.textAll(message.c_str()); // Send the message to all WebSocket clients
	return strlen(buffer); // Return the length of the formatted message
}





// Task to Send Data to Web UI
void sendDataToUI(void *pvParameters){
	/*#if SERIAL_ENABLED
		Serial.println("Send Data to UI Task Running");
	#endif*/
	while(true){
		// Wait for the SEND_DATA bit to be set
		EventBits_t bits = xEventGroupWaitBits(mainEventGroup, SEND_DATA, pdTRUE, pdFALSE, pdMS_TO_TICKS(readSensorsInterval));
		if((bits & SEND_DATA) == SEND_DATA){
			/*#if SERIAL_ENABLED
				Serial.println("sendDataToUI Task: Sending Data to Web UI Triggered\n");
			#endif*/

			xSemaphoreTake(jsonSemaphore, portMAX_DELAY);// Wait for the JSON object to be free (not being sent to the web UI

			String tmp;
			serializeJson(messageJSON, tmp);
			messageJSON.clear();// Clears the data that was stored in messageJSON, so it will be empty for the next time we want to send data
			xSemaphoreGive(jsonSemaphore);// Give the JSON object back to the main control loop

			notifyClients(tmp);// Send the JSON data to the web UI
		}
	}
}





//======================================================================================================
//	Helper Functions
//======================================================================================================

void selectMuxChannel(uint8_t channel){
	digitalWrite(MuxAPin, channel & 0x01);
	digitalWrite(MuxBPin, (channel >> 1) & 0x01);
	digitalWrite(MuxCPin, (channel >> 2) & 0x01);
}



void triggerSensor(){
	digitalWrite(MuxSigPin, LOW);
	delayMicroseconds(5);
	digitalWrite(MuxSigPin, HIGH);
	delayMicroseconds(10);
	digitalWrite(MuxSigPin, LOW);
}




void readDistance(uint8_t sensorNum){
	// Read the distance from the sensor
	duration[sensorNum] = pulseIn(echoPins[sensorNum], HIGH);
	//cm[sensorNum] = (duration[sensorNum]/2) / 29.1;
	inches[sensorNum] = (duration[sensorNum]/2) / 74;

	/*#if SERIAL_ENABLED
		Serial.printf("Distance for Sensor %d: ", sensorNum);
		//Serial.print(cm[sensorNum]);
		//Serial.print("cm\t");
		Serial.print(inches[sensorNum]);
		Serial.print("in");
		Serial.println();
	#endif*/

}




void setMotorPWM(uint8_t motor){
	uint16_t tmp = map(constrain(inches[motor], 24, 156), 156, 0, 0, motorAbsMaxSpeed);// Map the duration to the motor speed
	ledcWrite(motor, tmp);	// Set the PWM signal to the motor
  // ledcWrite(motor, motorAbsMaxSpeed);
}




// Task to buz motor
void buzMotorTask(void *pvParameters){
	uint8_t* tmp = (uint8_t*)pvParameters;
	uint8_t sensor = *tmp;

	while(true){
		if(inches[sensor] < 24){
			ledcWrite(sensor, motorAbsMaxSpeed);
			vTaskDelay(pdMS_TO_TICKS(50));
			ledcWrite(sensor, 0);
			vTaskDelay(pdMS_TO_TICKS(50));
		}else{
			setMotorPWM(sensor);
			vTaskDelay(pdMS_TO_TICKS(50));
		}

	}
}





//======================================================================================================
//	 Main Code
//======================================================================================================

void setup(){
	esp_log_set_vprintf(asyncLogPrintf);// Change the location the default logging goes to to the asyncLogPrintf function, rather than printing to serial. 

	// if SERIAL_ENABLED is defined, then Serial will be enabled	
	#if SERIAL_ENABLED
		Serial.begin(115200);
	#endif

	// initSPIFFS();
	initWiFi();
	jsonSemaphore = xSemaphoreCreateMutex();// Create a semaphore to control
	if( jsonSemaphore != NULL ) {
		// The semaphore was created successfully.
		// The semaphore can now be used.
		#if SERIAL_ENABLED
			Serial.println("Semaphore Created Successfully");
		#endif
	}else{
		// The semaphore was not created because there was not enough FreeRTOS heap available.
		// Take appropriate action.
		#if SERIAL_ENABLED
			Serial.println("Semaphore Creation Failed");
		#endif
	}

	ElegantOTA.begin(&server);	// Prep the ElegantOTA server (Over The Air updates)
	initWebSocket();			// Prep the WebSocket server
	server.begin();				// Start the web server. Automatically makes a new task at priority 3, on whatever core is available.


	// // Route for root / web page
	// server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
	// 	request->send(SPIFFS, "/index.html", "text/html",false);
	// });

	// server.serveStatic("/", SPIFFS, "/");

	// Create the event group
	mainEventGroup = xEventGroupCreate();

	// Create the task to send data to the web UI
	xTaskCreatePinnedToCore(
		sendDataToUI,		// Task Function
		"Send Data to Server",	// Task Name
		10000,					// Stack Size, should check utilization later with uxTaskGetStackHighWaterMark
		NULL,					// Parameters
		0,						// Priority 3, so it is at the same priority as the recieving data task. This is to help prevent the two from getting locked up
		&sendDataToUITask,		// Task Handle
		0						// Core 0
	);



	// Create the tasks to buzz the motors
	for(int i = 0; i < NUM_SENSORS; i++){
		xTaskCreatePinnedToCore(
			buzMotorTask,		// Task Function
			"Buzz Motor",		// Task Name
			1000,				// Stack Size, should check utilization later with uxTaskGetStackHighWaterMark
			(void*)&sensors[i],		// Parameters
			0,					// Priority 3, so it is at the same priority as the recieving data task. This is to help prevent the two from getting locked up
			NULL,				// Task Handle
			0					// Core 0
		);
	}



	// Init pins
	for(uint8_t i = 0; i < NUM_SENSORS; i++){
		// Set Up Echo Pins
		pinMode(echoPins[i], INPUT);

		// Set up Motor Pins
		pinMode(motorPins[i], OUTPUT);
		ledcSetup(i, motorPWMFreq, motorPWMResolution);	// Setup PWM for the motor
		ledcAttachPin(motorPins[i], i);					// Attach the PWM signal to the pin
		ledcWrite(i, 50);								// Set the PWM signal to 0
	}

	pinMode(MuxAPin, OUTPUT);
	pinMode(MuxBPin, OUTPUT);
	pinMode(MuxCPin, OUTPUT);
	pinMode(MuxSigPin, OUTPUT);

	// Set the Last Loop Start Time
	lastLoopStartTime = xTaskGetTickCount();// Get the current time in tick count
}




void loop(){
	// ws.textAll("{\"message\": \"Starting loop\"}");// Send a message to the web UI that the JSON was invalid
	for(int i = 0; i < NUM_SENSORS; i++){
		selectMuxChannel(i);
		triggerSensor();
		readDistance(i);
		delay(100);
		// setMotorPWM(i);
	} 

	// prep data to send to web server
	// JsonArray durVals = messageJSON["durVals"].to<JsonArray>();
	// JsonArray distCM = messageJSON["distCM"].to<JsonArray>();

	BaseType_t status = xSemaphoreTake(jsonSemaphore, readSensorsInterval/2);// Wait for the JSON object to be free (not being sent to the web UI), for half the time between reads
	
	if(status == pdTRUE){											// If the semaphore was aquired, send stuff to the web UI
		JsonArray distIN = messageJSON["distIN"].to<JsonArray>();
		for(int i = 0; i < NUM_SENSORS; i++){
			// durVals.add(duration[i]);
			// distCM.add(cm[i]);
			distIN.add(inches[i]);  
			// distIN.add(duration[i]);
		}
		xSemaphoreGive(jsonSemaphore);// Give the JSON object back to the main control loop
		xEventGroupSetBits(mainEventGroup, SEND_DATA);// Set the SEND_DATA bit to send data to the web UI
	}

	// vTaskDelayUntil(&lastLoopStartTime, readSensorsInterval);// Delay for readSensorsInterval between reads
}