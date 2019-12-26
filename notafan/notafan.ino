/*
 * 1. In_Temp port변경 "1226"
 *  -. ESP-32 GPIO 불량?  GPIO27 -> GPIO34 ADC2 is used by Wi-Fi Driver.
 *  -. makeCelsius()  1024(10bit) -> 4095(12bit: 0xFFF)
 *  
 * 2. Reg-Val change	"1014"
 *  -. 100ohm -> 8020ohm
 * 3. "1024"
 *    1. valve interval: 5sec -> 1sec
 *    2. wi-fi x 대응 --> 아직 불량
 *    3. key만으로 동작
 * 4. 1025
 *    -. wi-fi loop reconnect 처리 수정: test 미확인
 * 5. "1125"
 *    -. Valve <--> igniter portqusrud  "11251"
 *    -. Valve => Valv_1 - (inter 20ms) - Valv_2  "11252"
 *    -. Act Perfume: 
 *    -. Gas-Sensor:		not implement.
 *    -. Buzzer:          "11255"  not implement.
 *    -. Zero-Cross:      "11256"
 * 6. "1126"
 *    -. Sut-bul only : when >temperature, fan-on "11261"
 *    -. Perfume control : "11262"
 *    -. Vlv_1/2 : "11263"
 * 7. "1210"
 *    -. Check Zero-Cross by interrupt routine.
 *
*/
#include  <string.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
// "1016s"
#include <esp_int_wdt.h>
#include <esp_task_wdt.h>
// "1016e"
////////////////////////////////
//"1016" extern PubSubClient  client;
#define	uint	unsigned short int    // 1214 short int 2byte, int 4byte
#define	ANALOGUE_TYPE
#define _DBG
//#define _ACTDBG     //  1213
//#define _RESTART	// "1016"

#define SIZEOFMSGBUF  128
//typedef enum{
enum MODE{
  MD_PREV,
	MD_OFF,
	MD_V_OPEN,
	MD_V_DELAY,
	MD_F_OPEN,
	MD_F_DELAY,
	MD_F_CLOSE,
	MD_FC_DELAY,
	MD_F_CHECK,
	MD_T_CHECK,
	MD_B_CHECK
};  // Mode;
//1120S
// Form of Information
typedef struct {
  char  ssid[48];
  char  pswd[16];
  char  mqtt[48];
  char  storeId[16];
  byte  tableId;
  char  validmsg[8];  // "malkori"
} MyInfoSet;
const  int  pE2p = 0;  // e2prom address
const  int  pE2pValid = 0x100;
const  char *pValid = "malkori";
const MyInfoSet dfltInfo = {
  "AndroidHotspot3026", "01075213026", "broker.hivemq.com", "edward", 1, "malkori"
};
MyInfoSet  myinfo;
int   e2pSize;
char  msgBuf[256];
byte  msgIndex;    // 7 6 5  4   3     2     1    0
byte  fInfoCheck;  //[ | | |tId|sId|broker|pswd|ssid]
//1120E

// for postfix ++
MODE  operator++(MODE &s, int){
  //return MODE*(s + 1);
  int i = (int)s + 1;
  s = MODE(i);
  return  s;
}
// for prefix ++
MODE  operator++(MODE const& s){
  return MODE(s + 1);
}
/*
Mode	seq_table[8] = {MD_OFF, MD_V_OPEN, MD_V_DELAY, MD_F_OPEN, MD_F_DELAY, MD_F_CLOSE
				, MD_FC_DELAY, MD_T_CHECK};
*/
MODE  myMode, prevMode;	 //seq_index;
byte    reQueue;  //11256
byte	stateMntr;
byte  stateDevice;
/*     7         6          5        4          3          2        1        0
     s_fan    s_valve   s_borker    s_led    f_button    f_Fire    f_O2    f_Temp
       on      open	   on	     on	        on         on       on       on  // if(true)
*/
#define	ON_TEMP		stateMntr |= 0x01;
#define	OFF_TEMP	stateMntr &= 0xFE;
#define	CHK_TEMP	(stateMntr & 0x01)
#define	ON_CO2		stateMntr |= 0x02;
#define	OFF_CO2		stateMntr &= 0xFD;
#define	CHK_CO2		(stateMntr & 0x02)
#define	ON_FIRE		stateMntr |= 0x04;
#define	OFF_FIRE	stateMntr &= 0xFB;
#define	CHK_FIRE	(stateMntr & 0x04)
#define	ON_BUTTON	stateMntr |= 0x08;
#define	OFF_BUTTON	stateMntr &= 0xF7;
#define	CHK_BUTTON	(stateMntr & 0x08)
#define	ON_LED		stateMntr |= 0x10;
#define	OFF_LED		stateMntr &= 0xEF;
#define	CHK_LED		(stateMntr & 0x10)
#define	ON_BROKER	stateMntr |= 0x20;
#define	OFF_BROKER	stateMntr &= 0xDF;
#define	CHK_BROKER	(stateMntr & 0x20)
#define	ON_VALVE	stateMntr |= 0x40;
#define	OFF_VALVE	stateMntr &= 0xBF;
#define	CHK_VALVE	(stateMntr & 0x40)
#define	ON_FAN		stateMntr |= 0x80;
#define	OFF_FAN		stateMntr &= 0x7F;
#define	CHK_FAN		(stateMntr & 0x80)
// 11262s
#define ON_PERFUME  stateDevice |= 0x01;
#define OFF_PERFUME stateDevice &= 0xFE;
#define CHK_PERFUME (stateDevice & 0x01)
// 11262e
// 11256S
#define  REQ_VALV_OFF      0x10
#define  REQ_VALV_ON       0x11
#define  REQ_IGNTR_OFF     0x20
#define  REQ_IGNTR_ON      0x21
#define  REQ_FAN_OFF       0x40
#define  REQ_FAN_ON        0x41
#define  REQ_PERFUME_OFF   0x80
#define  REQ_PERFUME_ON    0x81
// 11256E
//const int LED_BUILTIN = 2;	// GPIO02	"1016"
const int	O_FAN = 26;			// GPIO26
const int	O_GASVALVE_1 = 25;	// GPIO25       "11251"
const int	O_GASVALVE_2 = 5;	// GPIO5       "11252"
const int	I_WATCHFIRE = 35;	// GPIO35       
const int	I_CO2 = 32;			// GPIO32
const int	O_PERFUME = 21;		// GPIO21
const int	O_SETFIRE = 33;		// GPIO33       "11251"
const int	I_TEMP = 34;		// "1013" GPIO27(SNVP) ADC1_CH0        10bit Resolution
const int	I_TACTSW = 14;		// GPIO14
const int	O_LED = 12;			// GPIO12
const int       O_BUZ = 4;      // GPIO4 Buzzer  "11255"  not implement
const int       I_ZERO = 18;      // GPIO18  Zero-Cross  "11256"
const double defaultSet = 145.0;	// default Celsius.
const double	coverageCel = 3.0;	//+/- 3C
const double  onlyFanHot = 30.0;
const byte	period = 20;	// 20ms cycle
// 5Sec up-counter
const byte	pub_Interval = (byte)(0x100 - 5000/period); // 5sec
// 5Sec up-counter valve open�ҿ�ð�
const byte	valv_Interval = (byte)(0x100 - 100/period); // 5sec -> 0.1sec "1213"
const byte  wifi_interval = pub_Interval; // 10242
// 3Sec up-counter
const byte	fire_Interval = (byte)(0x100 - 3000/period); // 3sec
const byte	fire_CheckInterval = (byte)(0x100 - 1000/period); // 1sec
const byte	led_interval = 256 - 500/period;	// 0.5sec
//byte  wifiinterval; // 1025
byte	interval;		// check for valv_Interval, fire_Interval
byte	itspubtime;		// publish Cycle
unsigned long	time_now, time_prev;	// for loop check
byte	blinkCounter;
const byte  anyoneSec = (byte)(0x100 - 1000/period); // 1sec
byte  secForAnyone;
#define LENOFTEMPBUF	10
//const double	coverageCel = 2.0;	//+/- 2C

byte	i_temp, i_co2, i_fire, i_btn;
double	analog_value;			// value from ADC1_CH0(Temperature Sensor)
unsigned int	tempbuf[LENOFTEMPBUF];
double  currentTemperature;	// ����µ�
double  setTemperature;		// �����µ�
bool	f_led;
////////// MQTT
// Replace the next variables with your SSID/Password combination
//1120 const char* ssid = "AndroidHotspot7656";  // "AndroidHotspot3026" 10244
//1120 const char* password = "SM-J510L021f";     // "01075213026"  10244

// Add your MQTT Broker IP address, example:
// MQTT Server  using "Hive-MQ" for test ???????????????????????????
// 1120 const char* mqtt_server = "broker.hivemq.com";
// 1120 const char* store_ID = "edward";
// 1120 const char* table_No = "1";
char  rx_menu[20];  //
//??????????????????????????????????????????????????????????????????
//byte  wf_step = 0;	//12102
const char* pre_topic = "notafan.net/ack/";
const char* pre_subtopic = "notafan.net/req/";
char  subTopic[64];
char  pubTopic[64];
char  pubMsg[64];
WiFiClient    espClient;
PubSubClient  client(espClient);
int   value = 0;
String clientId = "notafan-";	// "1016"
const uint  mIn_1 = 0x10000 - 60000 / period;   // inc timer
const byte  df_acTime = 180;  // 180min   dec timer
byte  acTime;			// "1016" action-time dec timer
uint  min_counter;
bool  netState = false;   // 10242
// 11262S
const uint  df_pfInterval = 0x10000 - 300000 / period;	// 500ms/5min
//const uint  df_pfInterval = 0x10000 - 60000 / period;  // 500ms/1min for test
const uint  pf_onLen = 0x10000 - 500 / period;  // 500ms
uint	pf_interval;
// 11262E
////////////////////////
//
void	varInit(){
	stateMntr = 0;
	stateDevice = 0;  // "11262"  [|||||||perfume]
	blinkCounter = 0;
	interval = 0;itspubtime = 0;
	pf_interval = 0;	// 11262
  reQueue = 0;  // 11256
	//wifiinterval = wifi_interval; // 1025
	time_now=0;time_prev=0;
	analog_value=0.0;
	currentTemperature = 0.0;setTemperature = defaultSet;
	f_led = true;
	i_co2=0;i_fire=0;i_btn=0;i_temp=0;
	for(int i=0;i<LENOFTEMPBUF;i++){
		tempbuf[i] = 0;
	}
	itspubtime = pub_Interval;
	secForAnyone = anyoneSec; //"1015" for general purpose
	myMode = MD_OFF;
	prevMode = MD_PREV;
	//1120S
	fInfoCheck = 0x00;
	msgIndex = 0;
  e2pSize = sizeof(MyInfoSet);
	//1120E
  acTime = 0;
  min_counter = 0;
}
/*//////
  Convert Analogue_Value to Celsius
*/////////
/*
	ohm -> Celsius
	.. -- ... 50C ~ 300C ... -- ..
	250 적당온도 if( t >300C ) 육즙이 빠지고 탄다. 
*/
// setting for PT100 Temperature Sensor ##############################
#define	SIZEOF_R2_TABLE		400  //"1014" ( 0C ~ 499C )
const double	vdd = 3.300;	// 3V
const double	ohm_1 = 82.00;	// "1015" 100.0;	// (Ohm)reg in voltage divider.
const double	r2table[SIZEOF_R2_TABLE] = {	// 0C ~ 490C의 R2값으로 구성
// ------------------- fill 0 ~ 499 1C 단위
/*
100.00, 103.85, 107.70, 111.55, 115.40, 119.25, 123.10, 126.95, 130.80, 134.65,	// 0,10,20, ... ,80,90
138.50, 142.35, 146.20, 150.05, 153.90, 157.75, 161.60, 165.45, 169.30, 173.15,	// 100 ~ 190
177.00, 180.85, 184.70, 188.55, 192.40, 196.25, 200.10, 203.95, 207.80, 211.65,	// 200 ~ 290
215.50, 219.35, 223.20, 227.05, 230.90, 234.75, 238.60, 242.45, 246.30, 250.15,	// 300 ~ 390
254.00, 257.85, 261.70, 265.55, 269.40, 273.25, 277.10, 280.95, 284.80, 288.65,	// 400 ~ 490
*/
30259, 28593, 27027, 25555, 24171, 22870, 21646, 20493, 19409, 18387,	// 0 ~ 9
17425, 16518, 15663, 14857, 14096, 13378, 12701, 12061, 11457, 10887,	// 10 ~ 19
10347,  9838,  9355,  8899,  8468,  8060, 7673, 7307, 6960, 6631,		// 20 ~ 29
6320, 6025, 5745, 5479, 5227, 4988, 4761, 4545, 4340, 4146,			// 30 ~ 39
3961, 3785, 3618, 3459, 3308, 3164, 3027, 2897, 2773, 2655,			// 40 ~ 49
2542, 2435, 2333, 2235, 2142, 2054, 1969, 1888, 1811, 1738,			// 50 ~ 59
1668, 1601, 1537, 1476, 1417, 1361, 1308, 1257, 1208, 1161,			// 60 ~ 69
1117, 1074, 1033, 993.6, 956.1, 920.1, 885.7, 852.7, 821.1, 790.7,	// 70 ~ 79
761.7, 733.8, 707.1, 681.5, 656.9, 633.3, 610.7, 588.9, 568.0, 548.0,	// 80 ~ 89
528.8, 510.3, 492.5, 475.5, 459.1, 443.3, 428.1, 413.6, 399.5, 386.1,	// 90 ~ 99

373.1, 360.6, 348.6, 337.0, 325.9, 315.2, 304.9, 294.9, 285.4, 276.2,	// 100 ~ 109
267.3, 258.7, 250.5, 242.5, 234.8, 227.4, 220.3, 213.4, 206.8, 200.4,	// 110 ~ 119
194.2, 188.2, 182.5, 176.9, 171.6, 166.4, 161.4, 156.5, 151.9, 147.4,	// 120 ~ 129
143.0, 138.8, 134.7, 130.8, 127.0, 123.3, 119.7, 116.3, 112.9, 109.7,	// 130 ~ 139
106.6, 103.6, 100.7, 97.84, 95.10, 92.46, 89.89, 87.41, 85.00, 82.67,	// 140 ~ 149
80.41, 78.22, 76.10, 74.05, 72.06, 70.13, 68.26, 66.45, 64.69, 62.98,	// 150 ~ 159
61.33, 59.72, 58.16, 56.65, 55.19, 53.77, 52.39, 51.05, 49.75, 48.48,	// 160 ~ 169
47.26, 46.07, 44.91, 43.79, 42.70, 41.64, 40.61, 39.61, 38.64, 37.69,	// 170 ~ 179
36.77, 35.88, 35.01, 34.17, 33.35, 32.55, 31.78, 31.02, 30.29, 29.58,	// 180 ~ 189
28.88, 28.21, 27.55, 26.91, 26.29, 25.68, 25.09, 24.52, 23.96, 23.41,	// 190 ~ 199

22.88, 22.37, 21.86, 21.37, 20.90, 20.43, 19.98, 19.54, 19.11, 18.69,	// 200 ~ 209
18.28, 17.88, 17.49, 17.11, 16.75, 16.39, 16.04, 15.69, 15.36, 15.03,	// 210 ~ 219
14.72, 14.41, 14.11, 13.81, 13.52, 13.24, 12.97, 12.70, 12.44, 12.19,	// 220 ~ 229
11.94, 11.70, 11.46, 11.23, 11.00, 10.78, 10.56, 10.35, 10.15, 9.948,	// 230 ~ 239
9.752, 9.560, 9.373, 9.190, 9.011, 8.836, 8.666, 8.499, 8.335, 8.176,	// 240 ~ 249
8.020, 7.868, 7.718, 7.573, 7.430, 7.291, 7.154, 7.021, 6.890, 6.763,	// 250 ~ 259
6.638, 6.516, 6.396, 6.279, 6.165, 6.053, 5.943, 5.836, 5.731, 5.628,	// 260 ~ 269
5.528, 5.429, 5.333, 5.238, 5.146, 5.055, 4.967, 4.880, 4.795, 4.711,	// 270 ~ 279
4.630, 4.550, 4.472, 4.395, 4.320, 4.246, 4.174, 4.103, 4.034, 3.966,	// 280 ~ 289
3.899, 3.834, 3.770, 3.707, 3.646, 3.586, 3.527, 3.469, 3.412, 3.356,	// 290 ~ 299

3.302, 3.248, 3.196, 3.144, 3.094, 3.044, 2.996, 2.948, 2.901, 2.855,	// 300 ~ 309
2.810, 2.766, 2.722, 2.680, 2.638, 2.597, 2.557, 2.517, 2.478, 2.440,	// 310 ~ 319
2.403, 2.366, 2.330, 2.295, 2.260, 2.226, 2.192, 2.159, 2.127, 2.095,	// 320 ~ 329
2.064, 2.033, 2.003, 1.974, 1.945, 1.916, 1.888, 1.860, 1.833, 1.807,	// 330 ~ 339
1.781, 1.755, 1.730, 1.705, 1.681, 1.657, 1.633, 1.610, 1.587, 1.565,	// 340 ~ 349
1.543, 1.521, 1.500, 1.479, 1.458, 1.438, 1.418, 1.399, 1.379, 1.361,	// 350 ~ 359
1.342, 1.324, 1.306, 1.288, 1.270, 1.253, 1.237, 1.220, 1.204, 1.188,	// 360 ~ 369
1.172, 1.156, 1.141, 1.126, 1.111, 1.096, 1.082, 1.068, 1.054, 1.040,	// 370 ~ 379
1.027, 1.014, 1.001, 0.9878, 0.9751, 0.9627, 0.9504, 0.9383, 0.9264, 0.9147,	// 380 ~ 389
0.9032, 0.8918, 0.8806, 0.8696, 0.8588, 0.8481, 0.8376, 0.8272, 0.8170, 0.8070	// 390 ~ 399
};
//
double	cnvOhmtoCel(double reg){
	unsigned int	index = 0;
	if( reg > r2table[0] )
		return	-999.99;	// < 0C

	if( reg < r2table[SIZEOF_R2_TABLE - 1] )
		return	-999.99;		// > 499C "1017"
	
	while( reg < r2table[index] )	index++;

	if( reg == r2table[index] )
		return	index;

	double	r1 = r2table[index-1] - reg;
	double	r2 = r2table[index-1] - r2table[index];

	//return	(index - 1) * 10 + (r1 / r2) * 10;
	return	index - 1 + r1 / r2;
}

double  makeCelsius(double anaVal){
	double	vOut, ohm_2;
	//"1015S"
	// get Vout
	vOut = vdd - anaVal * vdd / 4095; //"1015"  1024;	// Vdd = 3.3V
	if( vOut ){
		//double	ohm_2 = ohm_1 * 1 / (vdd / vOut - 1);
		ohm_2 = ohm_1 / vOut * vdd - ohm_1;	// get oHm from Vo
	}else{
		ohm_2 = 0.1;	// Set Invalid Value.
	}
	//"1015E"
	return  cnvOhmtoCel(ohm_2);
}
////////
void  readSens(){
	unsigned int	wrki;
	// read SW
	i_btn <<= 1;
	if( digitalRead(I_TACTSW) ){
		i_btn++;
	}
	if( i_btn == 0xF0 ){
		if( myMode == MD_OFF){
			myMode++;			// off -> on
			setTemperature = defaultSet;	// Key default.
      acTime = df_acTime;
      min_counter = mIn_1;
      #ifdef  _DBG
      Serial.println("Power-ON");
      #endif
		}else{
			myMode = MD_OFF;	//	MD_OFF;	// on -> off
      reQueue = 0;
      #ifdef  _DBG
      Serial.println("Power-OFF");
      #endif
		}
		ON_BUTTON
    #ifdef  _DBG
    //Serial.print("\n\t>>>> Button-In");
    #endif   
	}else{
		OFF_BUTTON
	}
	////////////////
	// read CO2 ???? [확인] Is device digital or analog type?
	///////////////////////
	i_co2 <<= 1;
	if( digitalRead(I_CO2) ){
		i_co2++;
	}
	if(i_co2==0xFF){
		ON_CO2
	}else if(i_co2==0x00){
		OFF_CO2
	}
	/////////////
	// read fire	data-sheet: if(0) flame-on
	//		if(analog_type)
	//			if(val < 500)
	//				flame-ON_BUTTON
	//////////////////////
	i_fire <<= 1;
	#ifdef	ANALOGUE_TYPE
	wrki = analogRead(I_WATCHFIRE);
	if( wrki < 500 ){
		i_fire++;
	}
	#else
	if( !digitalRead(I_WATCHFIRE) ){
		i_fire++;
	}
	#endif
	if(i_fire==0xFF){
    #ifdef  _DBG
    //Serial.println("Fire-sens: Sensing");
    #endif
		ON_FIRE
	}else if(i_fire==0x00){
		OFF_FIRE
	}
	///////////////////////////////////////////
	// tempbuf & analog_value
	// shift buffered data
	//////////////////////////////////////////
	// 
	for( int i=LENOFTEMPBUF-1;i;i-- ){
		tempbuf[i] = tempbuf[i-1];
	}
	tempbuf[0] = analogRead(I_TEMP);
	unsigned long sum = 0;
	for( int i=0;i<LENOFTEMPBUF;i++ ){
		sum += tempbuf[i];
	}
	analog_value = (double)sum / LENOFTEMPBUF;
	currentTemperature = makeCelsius(analog_value);

	i_temp <<= 1;
	if( currentTemperature != 999.99 && currentTemperature != -999.99 ){
  	if( CHK_VALVE ){	// ����� ��
	  	if( currentTemperature > setTemperature + coverageCel ){
		  	i_temp++;
		  }
	  }else{			// ������ ��
		  if( currentTemperature > setTemperature - coverageCel ){
			  i_temp++;
		  }
	  }
	}
	if( i_temp == 0xff ){
		ON_TEMP
	}else if( i_temp == 0x00 ){
		OFF_TEMP
	}
}
//////////////////////////
//  Zero-cross check
////////////
// 11262S
// 
void  perfumer(){
  if( pf_interval == 0 && CHK_PERFUME ){
    if( reqRlyCtrl(REQ_PERFUME_OFF) == true ){
      pf_interval = df_pfInterval;
    }
  }else{
    if( pf_interval == pf_onLen ){ // sweet during 500ms
      if( reqRlyCtrl(REQ_PERFUME_ON) == true ){
        pf_interval++;
      }
    }else{
      pf_interval++;
    }
  }
}

void	ctrlPerfume(bool f_per){
	if( f_per ){
    ON_PERFUME
		digitalWrite(O_PERFUME, HIGH);
    #ifdef  _DBG
    Serial.println("Perfumer: OPEN");
    #endif
	}else{
    OFF_PERFUME
		digitalWrite(O_PERFUME, LOW);
    #ifdef  _DBG
    Serial.println("Perfumer: CLOSE");
    #endif
	}
}
// 11262E
void	ctrlBroker(bool f_brok){
	if(f_brok){
		ON_BROKER
		digitalWrite(O_SETFIRE, HIGH);
    #ifdef  _DBG
    Serial.println("Igniter: ON");
    #endif
		interval = fire_Interval;
	}else{
		OFF_BROKER
		digitalWrite(O_SETFIRE, LOW);
    #ifdef  _DBG
    Serial.println("Igniter: OFF");
    #endif
		interval = fire_CheckInterval;
	}
}

void	ctrlFan(bool f_fan){
	if(f_fan){
		ON_FAN
		digitalWrite(O_FAN, HIGH);
    #ifdef  _DBG
    Serial.println("FAN: ON");
    #endif
	}else{
		OFF_FAN
		digitalWrite(O_FAN, LOW);
    #ifdef  _DBG
    Serial.println("FAN: OFF");
    #endif
	}
}
// 1213S
bool  ctrlValve(bool f_open){
  static int  selvlv;
  bool retb = false;

  if( interval ){
    return  retb;
  }
  
  if(selvlv){
    retb = true;
  }

  if(f_open){
    if( selvlv ){
      ON_VALVE
      digitalWrite(O_GASVALVE_2, HIGH);
      #ifdef  _DBG
      Serial.println("GasValve_2: OPEN");
      #endif
      selvlv = 0;
    }else{
      digitalWrite(O_GASVALVE_1, HIGH);
      #ifdef  _DBG
      Serial.println("GasValve_1: OPEN");
      #endif
      selvlv = 1;
    }
    interval = valv_Interval;
  }else{
    if( selvlv ){
      OFF_VALVE
      digitalWrite(O_GASVALVE_2, LOW);
      #ifdef  _DBG
      Serial.println("GasValve_2: CLOSE");
      #endif
      selvlv = 0;
    }else{
      digitalWrite(O_GASVALVE_1, LOW);
      //interval = valv_Interval;
      #ifdef  _DBG
      Serial.println("GasValve_1: CLOSE");
      #endif
      selvlv = 1;
    }
  }
  return  retb;
}
/////// 1213E
void	ctrlLed(bool f_led){
	if(f_led){
		ON_LED
		digitalWrite(O_LED, HIGH);
	}else{
		OFF_LED
		digitalWrite(O_LED, LOW);
	}
}
////////////////////////////////////////
//
//  Zero-Cross check Function  "11256"
//
////////////////////////////////////////
bool  reqRlyCtrl(byte dvcAct){
  bool  ret = false;
  if(reQueue == 0){
    reQueue = dvcAct;
    #ifdef  _DBG
    //Serial.print("reQueue = ");Serial.println(dvcAct);
    #endif
    ret = true;
  }
  return  ret;
}
#ifdef  _ACTDBG
void  myZeroCross(){
  if( reQueue ){ //&& !digitalRead(I_ZERO) ){
    switch( reQueue ){
      case  REQ_VALV_OFF:
        if( ctrlValve(false) == true ){
          reQueue = 0;
        }
        break;
      case  REQ_VALV_ON:
        if( ctrlValve(true) == true ){
          reQueue = 0;
        }
        break;
      case  REQ_IGNTR_OFF:
        ctrlBroker(false);
        break;
      case  REQ_IGNTR_ON:
        ctrlBroker(true);
        break;
      case  REQ_FAN_OFF:
        ctrlFan(false);
        break;
      case  REQ_FAN_ON:
        ctrlFan(true);
        break;
      case  REQ_PERFUME_OFF:
        ctrlPerfume(false);
        break;
      case  REQ_PERFUME_ON:
        ctrlPerfume(true);
      default:
        break;
    }
    if( !(reQueue & 0x10) ){ //!= REQ_VALV_OFF || reQueue != REQ_VALV_ON){
      reQueue = 0;
    }
  }  
}
#endif
void IRAM_ATTR  intZero_XSvc(){
	//static byte	vlvnum = 0;
  #ifdef  _DBG
  //Serial.print("Z");
  #endif
  if( reQueue ){ //&& !digitalRead(I_ZERO) ){
  #ifdef  _DBG
  //Serial.println("#");
  #endif
    switch( reQueue ){
      case  REQ_VALV_OFF:
        if( ctrlValve(false) == true ){
          reQueue = 0;
        }
        break;
      case  REQ_VALV_ON:
        if( ctrlValve(true) == true ){
          reQueue = 0;
        }
        break;
      case  REQ_IGNTR_OFF:
        ctrlBroker(false);
        break;
      case  REQ_IGNTR_ON:
        ctrlBroker(true);
        break;
      case  REQ_FAN_OFF:
        ctrlFan(false);
        break;
      case  REQ_FAN_ON:
        ctrlFan(true);
        break;
      case  REQ_PERFUME_OFF:
        ctrlPerfume(false);
        break;
      case  REQ_PERFUME_ON:
        ctrlPerfume(true);
      default:
        break;
    }
    if( !(reQueue & 0x10) ){ //!= REQ_VALV_OFF || reQueue != REQ_VALV_ON){
      reQueue = 0;
    }
  }  
}
