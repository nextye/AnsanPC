//"12102s"
/*
void WiFiEvent(WiFiEvent_t event)
{
    Serial.printf("[WiFi-event] event: %d\n", event);

    switch (event) {
        case SYSTEM_EVENT_WIFI_READY: 
            Serial.println("WiFi interface ready");
            break;
        case SYSTEM_EVENT_SCAN_DONE:
            Serial.println("Completed scan for access points");
            break;
        case SYSTEM_EVENT_STA_START:
            if( wf_step == 4 ){
  	          wf_step = 5;
              Serial.println("WiFi client started");
            }
          break;
        case SYSTEM_EVENT_STA_STOP:
          if( wf_step == 2 ){
        	  wf_step = 3;
            Serial.println("WiFi clients stopped");
          }
          break;
        case SYSTEM_EVENT_STA_CONNECTED:
            Serial.println("Connected to access point");
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
        	  wf_step = 98;
            Serial.println("Disconnected from WiFi access point");
            break;
        case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
            Serial.println("Authentication mode of access point has changed");
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            Serial.print("Obtained IP address: ");
            Serial.println(WiFi.localIP());
            break;
        case SYSTEM_EVENT_STA_LOST_IP:
            wf_step = 99;
            Serial.println("Lost IP address and IP address is reset to 0");
            break;
        case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
            Serial.println("WiFi Protected Setup (WPS): succeeded in enrollee mode");
            break;
        case SYSTEM_EVENT_STA_WPS_ER_FAILED:
            Serial.println("WiFi Protected Setup (WPS): failed in enrollee mode");
            break;
        case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
            Serial.println("WiFi Protected Setup (WPS): timeout in enrollee mode");
            break;
        case SYSTEM_EVENT_STA_WPS_ER_PIN:
            Serial.println("WiFi Protected Setup (WPS): pin code in enrollee mode");
            break;
        case SYSTEM_EVENT_AP_START:
            Serial.println("WiFi access point started");
            break;
        case SYSTEM_EVENT_AP_STOP:
            Serial.println("WiFi access point  stopped");
            break;
        case SYSTEM_EVENT_AP_STACONNECTED:
            Serial.println("Client connected");
            break;
        case SYSTEM_EVENT_AP_STADISCONNECTED:
            Serial.println("Client disconnected");
            break;
        case SYSTEM_EVENT_AP_STAIPASSIGNED:
            Serial.println("Assigned IP address to client");
            break;
        case SYSTEM_EVENT_AP_PROBEREQRECVED:
            Serial.println("Received probe request");
            break;
        case SYSTEM_EVENT_GOT_IP6:
            Serial.println("IPv6 is preferred");
            break;
        case SYSTEM_EVENT_ETH_START:
            Serial.println("Ethernet started");
            break;
        case SYSTEM_EVENT_ETH_STOP:
            Serial.println("Ethernet stopped");
            break;
        case SYSTEM_EVENT_ETH_CONNECTED:
            Serial.println("Ethernet connected");
            break;
        case SYSTEM_EVENT_ETH_DISCONNECTED:
            Serial.println("Ethernet disconnected");
            break;
        case SYSTEM_EVENT_ETH_GOT_IP:
            Serial.println("Obtained IP address");
        
        default:
			break;
    }
}
*/
//"12102e"
void hard_restart(){
	esp_task_wdt_init(1,true);
	esp_task_wdt_add(NULL);
	while(true);
}
/*
	Wi-Fi Connect
	"1016"
*/
//  1213
void  makeTopic(){
  sprintf(subTopic, "%s%s/%d", pre_subtopic, myinfo.storeId, myinfo.tableId);
  sprintf(pubTopic, "%s%s/%d", pre_topic, myinfo.storeId, myinfo.tableId);
}

bool WIFI_Connect()
{
	static  byte wifi_step;
	static  byte wifiinterval;
	bool	retb = false;

	if(WiFi.status() != WL_CONNECTED){
		switch(wifi_step){
			case	0:
				//WiFi.mode(WIFI_STA);
				digitalWrite(LED_BUILTIN, LOW);
				wifi_step++;
				break;
			case	1:
				WiFi.disconnect(true);
		    //Serial.println("\t>>> WiFi.disconnect(true)");
				wifiinterval = 0x100 - 1000/period;
		    wifi_step++;
				break;
			case	2:
				if( ++wifiinterval == 0 ){
					wifi_step++;
				}
				break;
			case	3:
				WiFi.begin(myinfo.ssid, myinfo.pswd);
        #ifdef  _DBG
				//Serial.print("\nWiFi.begin(");Serial.print(myinfo.ssid);Serial.print(", ");
        //Serial.print(myinfo.pswd);Serial.println(");");
        Serial.print(".");
        #endif
				wifiinterval = 0x100 - 1000/period;
		    wifi_step++;
				break;
			case	4:
				if( ++wifiinterval == 0 ){
          Serial.println();
					wifi_step = 3;
				}else{
          Serial.print(".");
        }
			default:
				break;
		}
	} else {
		if( wifi_step != 0 ){
			wifi_step = 0;
			Serial.println("\n\tWiFi Connected!!");
			Serial.print("IP address: ");
			Serial.println(WiFi.localIP());
			digitalWrite(LED_BUILTIN, HIGH);
		};
		retb = true;
	}
  #ifdef  _DBG
  //Serial.print("wifi_step = ");Serial.println(wifi_step);
  #endif
	return	retb;
}

void setup_WiFi(){
  //strcpy(subTopic, "notafan/edward/1");
  //strcpy(pubTopic, "notafan/edward/1");

	delay(10000);
	//WiFi.onEvent(WiFiEvent);
	//10242s
	/*
	while( WIFI_Connect() == false ){
		delay(5000);
	}
	client.setServer(myinfo.mqtt_server, 1883);
	client.setCallback(callback);
	*/
	if( WIFI_Connect() == true ){
		client.setServer(myinfo.mqtt, 1883);
		client.setCallback(callback);
		//1213 netState = true;
	}else{
		netState = false;
	}
	// 10242e
}

void callback(char* topic, byte* message, unsigned int length){
	unsigned int wrki;
	#ifdef  _DBG
	Serial.print("[Rx-Msg]topic: ");
	Serial.print(topic);
	Serial.print(", Message: ");
	#endif
	//String messageTemp;
	char  msg[SIZEOFMSGBUF];
	for(int i = 0; i < length; i++){
		//messageTemp += (char)message[i];
		msg[i] = (char)message[i];
	}
	msg[length] = 0;
	#ifdef  _DBG
	Serial.println(msg);
	#endif
	if( strcmp(subTopic, topic) == 0 ){
    // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
    // Changes the output state according to the message
    //if(messageTemp == "off"){
		if( strcmp(msg, "off") == 0 ){
			myMode = MD_OFF;
      reQueue = 0;
      #ifdef  _DBG
      Serial.println("Remote Power-OFF");
      #endif
		}else{
			char* token = strtok(msg, ":");
			strcpy(rx_menu, token);
			token = strtok(NULL, ":");
			setTemperature = atof(token);
			// 1024s
			token = strtok(NULL, ":");
			wrki = atoi(token);
			if( wrki == 0 ){
				wrki = df_acTime;
			}
      acTime = (byte)wrki;
      min_counter = mIn_1;
      pf_interval = df_pfInterval;
			//acTime = 0x100000000 - wrki * 60 * 1000 / period;
			// 1024e
			myMode = MD_V_OPEN;
      #ifdef  _DBG
      Serial.println("Remote Power-ON");
      #endif
		}
	}
}
// "1016"
void reconnect(){
	bool  retb = true;
	if(client.connected()){
		//1213 netState = true;
		return;
	}

	//Serial.println("mqtt reconnect start");
	// check Wi-Fi state
	//if( WiFi.status() != WL_CONNECTED ){
		if( WIFI_Connect() == false ){
	    	netState = false;
    		return;
		};
	//}
	client.setServer(myinfo.mqtt, 1883);
	client.setCallback(callback);
	Serial.println("Attempting MQTT connection...");

	if (client.connect(clientId.c_str())){
		Serial.println("\t...Connected");
		client.publish(pubTopic, "Net-O.K.");
		// Subscribe
		client.subscribe(subTopic);
		// pub interval
		itspubtime = pub_Interval;
    netState = true; // 1212
	}else {
		Serial.print("\tFailed, rc=");Serial.print(client.state());
		netState = false;
	}
}

void mqttPub(){
  static bool tx_off = false;
	char  temp[64];
	//10242s
	if( netState == false ){
    //Serial.println("\n\t\tnetState == false");
		return;
	}
  //Serial.println("\n\tnetState == true");
	//10242e
	if( myMode == MD_OFF){
    if( tx_off == false ){
      sprintf(pubMsg, "off");
      tx_off = true;
    }else{
      return;
    }
	}else{
    tx_off = false;
		dtostrf(currentTemperature, 6, 2, temp);
		// Serial.print("current temp = ");Serial.println(temp);
		sprintf(pubMsg, "%s:%s", rx_menu, temp);
	}
	client.publish(pubTopic, pubMsg);
  #ifdef  _DBG
	//sprintf(temp, "[Pub]%s::%s", pubTopic, pubMsg);
	//Serial.println(temp);
  #endif
}
