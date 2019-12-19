
extern void  setData2E2p(const MyInfoSet* pData = NULL);


////////////
void setup(){
	Serial.begin(115200);
	// put your setup code here, to run once:
	pinMode(LED_BUILTIN, OUTPUT);	// "1016"
	pinMode(O_FAN, OUTPUT);
	pinMode(O_GASVALVE_1, OUTPUT);
	pinMode(O_GASVALVE_2, OUTPUT);
	pinMode(O_SETFIRE, OUTPUT);
	pinMode(O_PERFUME, OUTPUT);	//"11262"
	pinMode(I_WATCHFIRE, INPUT_PULLUP); // pull-up
	pinMode(I_CO2, INPUT_PULLUP);    // pull-up
	pinMode(O_LED, OUTPUT);
  pinMode(O_BUZ, OUTPUT);    // "11255"
	pinMode(I_TACTSW, INPUT_PULLUP);  // pull-up
	pinMode(I_ZERO, INPUT);  // pull-up  "11256"
	pinMode(I_TEMP, INPUT);     // analog input? 
	varInit();
	digitalWrite(LED_BUILTIN, LOW);	// "1016"
	digitalWrite(O_SETFIRE, LOW);
	digitalWrite(O_PERFUME, LOW);	// "11262"
	digitalWrite(O_GASVALVE_1, LOW);
	digitalWrite(O_GASVALVE_2, LOW);
	digitalWrite(O_FAN, LOW);
	digitalWrite(O_LED, f_led);     // LED
  digitalWrite(O_BUZ, LOW);  //  "11255"
	// Create a random client ID
	clientId += String(random(0xffff), HEX);	//"1016"
	// rdTempData();
	#ifdef  _DBG
	delay(5000);  // for serial Interface
	#endif
	setup_WiFi();   // setup for Wi-Fi & MQTT
	// for MQTT
	//Serial.setTimeout(500); // Set time out for
	//mqttconnect();
	delay(1000);        // give me time to bring up serial monitor
	#ifdef  _DBG
	Serial.println("ESP32 Analog IN Test");
	#endif
	f_led = false;
	digitalWrite(O_LED, f_led);   // LED
	// put your main code here, to run repeatedly:
  attachInterrupt(digitalPinToInterrupt(I_ZERO), intZero_XSvc, RISING);// GPIO-18, Rising-edge  12101
	Serial.println("\n\n\t\t.... action Start!");
  // EEPROM process start
  if (!EEPROM.begin(e2pSize)) {
    Serial.println("\n\t[Error!!]Failed to initialize EEPROM");
    Serial.println("\t\tRestarting...");
    delay(1000);
    ESP.restart();
  }
  int replay;
  for( replay=0;replay<10; replay++ ){
    if( getDataFromE2p() == false ){
      setData2E2p(&dfltInfo);
      Serial.print("\n\n\t[!!!]failed to get e2prom data\t --> Set default data\n\n");
      delay(1000);
    }else{
      break;
    }
  }
  if(replay >= 10){
    ESP.restart();
  }else{
    fInfoCheck = 0x1f;
    makeTopic();  //1213
  }
  //EEPROM.end();
  //EEPROM process end
  guideInfo();
	time_prev = millis();     // loop counter stateMntr
}

/////////////
void loop(){
  /* if client was disconnected then try to reconnect again */
//
OMATU:
  #ifdef  _ACTDBG
  myZeroCross();
  #endif
  // main loop interval
  time_now = millis();
  if(time_now - time_prev < period ){
    goto OMATU;
  }
  esp_task_wdt_reset(); //1214
  time_prev = time_now;
  //1120S
  // if there's any serial available, read it:
  if(Serial.available() > 0) {
    if( getInfo()==true){
      setData2E2p();
    }
  }
  //1120E
  // mqttPub::client.loop()
  if (!client.connected()){
    reconnect();
  }
  //12101 myZeroCross();  //  "11256"
  // 10242s
  if( netState == true ){
    client.loop();
  }
  //10242e
  // publish check
  if( ++itspubtime == 0 ){
    itspubtime = pub_Interval;
    mqttPub();
  }
  //
  readSens();
  //
  //Mode  myMode = seq_table[seq_index];
  switch(myMode){
    case  MD_OFF:
      acTime = 0;   // 1017
      pf_interval = 0;interval = 0;	// 11263
      ctrlLed(false);
      // Gas-Valve�?
      if(CHK_VALVE){
        if( reqRlyCtrl(REQ_VALV_OFF) == false ){	;  //11256 ctrlValve(false);
	        break;
	      }
      }
      //
      if( CHK_BROKER ){
        if( reqRlyCtrl(REQ_IGNTR_OFF) == false ){	;  //11256 ctrlBroker(false);
	        break;
	      }
      }
      // Fan
      if( CHK_FAN ){
        if( reqRlyCtrl(REQ_FAN_OFF) == false ){  //11256 
	        break;
	      }
      }
      // Perfume
      if( CHK_PERFUME ){
        Serial.print("stateDevice =  "); Serial.println(stateDevice);
        if( reqRlyCtrl(REQ_PERFUME_OFF) == false ){
          break;
        }
      }
      // LED
      if( CHK_LED ){
        ctrlLed(false);
      }
      break;
    case  MD_V_OPEN:
      ctrlLed(true);
      if( reqRlyCtrl(REQ_VALV_ON) == true ){  //11256 ctrlValve(true);  //valve open -> interval -> flag
        myMode++;
      }
      break;
    case  MD_V_DELAY:
      if( interval == 0 ){
        if( CHK_VALVE ){
          myMode++;
        }
      }else{
        interval++;
      }
      break;
    case  MD_F_OPEN:
      #ifdef  _DBG
      //Serial.println("Igniter-On");  // "1017"
      #endif
      if( reqRlyCtrl(REQ_IGNTR_ON) == true ){  //11256 ctrlBroker(true);
        myMode++;
      }
      break;
    case  MD_F_DELAY:
      if( interval == 0 ){
        myMode++;
      }else{
        interval++;
      }
      break;
    case  MD_F_CLOSE:
      #ifdef  _DBG
      //Serial.println("Igniter-Off");
      #endif
      if( reqRlyCtrl(REQ_IGNTR_OFF) == true ){	//ctrlBroker(false);
        myMode++;
      }
      break;
    case  MD_FC_DELAY:
      if( interval == 0 ){
	      if( reQueue == 0 ){
          myMode++;
        }
      }else{
        interval++;
      }
      break;
    case  MD_F_CHECK:
      if( reqRlyCtrl(REQ_FAN_ON) == true ){	//ctrlFan(true);
        myMode++;
      }
    case  MD_T_CHECK:
      if( !CHK_FIRE ){
      	if( reqRlyCtrl(REQ_FAN_OFF) == true ){	//   ctrlFan(false);
      	  myMode = MD_F_OPEN;
	        //pf_interval = 0;	// 1213
	      }
      }else if( CHK_TEMP ){
        if( CHK_VALVE ){
	        if( reqRlyCtrl(REQ_VALV_OFF) == false ){   //ctrlValve(false);
	          break;
	        }
	      }
      	// only Sut-bul	"11261"
      	/*
          if( reqRlyCtrl(REQ_FAN_OFF) == false ){   //ctrlFan(false);
        	  break;
	        }
	      */
        blinkCounter = led_interval;  // LED -> 
      	//pf_interval = 0;	// 1213
        myMode++;
        #ifdef  _DBG
        Serial.println("Temp.-Over");
        #endif
      }else{
//      Serial.print("Current Temp. = ");
//      Serial.println(analog_value);
    	  // 1213s
        /*
	      if(pf_interval == 0){
	        pf_interval = df_pfInterval;
	      }
        */
	      // 1213e
      }
      break;
    case  MD_B_CHECK:
      //1213s only Act-Fan for Sut-bul
      if( currentTemperature < setTemperature - onlyFanHot ){ // depend on Fan
        if( reqRlyCtrl(REQ_FAN_OFF) == true ){
          myMode = MD_V_OPEN;
          #ifdef  _DBG
          Serial.println("Temp.-Down");
          #endif
        }
      }else{
        if( ++blinkCounter == 0 ){
          blinkCounter = led_interval;
          if( CHK_LED ){
            ctrlLed(false);
          }else{
            ctrlLed(true);
          }
        }
      }
      /*
      if( !CHK_TEMP ){
      	// only Sut-bul	"1213"
        if( reqRlyCtrl(REQ_FAN_OFF) == true ){   //ctrlFan(false);
          myMode = MD_V_OPEN;
          #ifdef  _DBG
          Serial.println("Temp.-Down");
          #endif
      	}
      }else{
        if( ++blinkCounter == 0 ){
          blinkCounter = led_interval;
          if( CHK_LED ){
            ctrlLed(false);
          }else{
            ctrlLed(true);
          }
        }
      }
      */
    default:
      break;
  }
  // 1017s
  #ifdef  _DBG
  if( prevMode != myMode ){
    #ifdef  _DBG
    //Serial.print("Current Mode: ");
    //Serial.println(myMode);
    #endif
    prevMode = myMode;
  }
  #endif

  if( myMode != MD_OFF ){
    perfumer();
    if( acTime ){
      if( ++min_counter == 0 ){
        if( --acTime == 0 ){
          myMode = MD_OFF;
        }else{
          min_counter = mIn_1;
        }
      }
    }
  }
  // 1017e
}
