/******************************************************************************
themastemrind.pk
Source file for the GSM SIMCOM Library

Yasirshahzad @ themastermind Electronics
JAN 23, 2019
+923106256643

This file defines the hardware interface(s) for all the SIMCcom
and abstracts SMS AND CALL and other features of the SIMCOM GSM modules

Development environment specifics:
Arduino 1.6.5+
SIMCOM GSM Evaluation Board - SIM800L, SIM800C, SIM900, SIM808
******************************************************************************/

#ifndef SIMCOM_h
#define SIMCOM_h

#include "Arduino.h"
#include <string.h>
#include "lcd1202.h"
#include "Settings.h"


extern Settings _Settings;
#define MQTT_WIFI // commnet to switch gsm


#define TubeWellType         2      // 1 for simple 2 for watering

#define DEVICE_ID             "1036"
#define MQTT_CONNECT_ID       "TWMS" DEVICE_ID
#define MQTT_TOPIC_SUB_REQ    "TWS/REQ/" DEVICE_ID
#define MQTT_TOPIC_PUB_ACK    "TWS/ACK/" DEVICE_ID
#define MQTT_TOPIC_PUB_NOTIFY    "TWS/NOTIFY/" DEVICE_ID
#define MQTT_TOPIC_PUB_TELE    "TWS/TELE/" DEVICE_ID
#define MQTT_TOPIC_PUB_STATE  "TWS/STATE/" DEVICE_ID
#define MQTT_TOPIC_PUB_DEVICE_ACTIVE_STATE "TWS/ACTIVE/" DEVICE_ID
/* Query Time from a Device */
#define MQTT_TOPIC_SUB_TIMER_REQ "TWS/TREQ/" DEVICE_ID
#define MQTT_TOPIC_PUB_TIMER_REQ "TWS/TACK/" DEVICE_ID


//comment this to off the debugging
//#define DEBUG_ON
//#define StatusDebug
//#define DebugResponse

  
#define RESET_PIN 13   // pin to the reset pin SIMCOM GSM

#define RXD2 16
#define TXD2 17

#define COMM_BUF_LEN        128

// some constants for the IsRxFinished() method
#define RX_NOT_STARTED      0
#define RX_ALREADY_STARTED  1

// some constants for the InitParam() method
#define PARAM_SET_0   0
#define PARAM_SET_1   1

enum sms_type_enum
{
  SMS_UNREAD,
  SMS_READ,
  SMS_ALL,
  SMS_LAST_ITEM
};


enum gprs_type_enum
{ 
  NO_RESP,
  UNKNOWN_RESP,
  IP_INITIAL,
  IP_START,
  IP_CONFIG,
  IP_GPRSACT,
  IP_STATUS,
  GPRS_CONNECTED,    // IP_STATUS
  GPRS_DISCONNECTED,  //PDP DEACT
  TCP_WAIT,     //9
  TCP_CONNECTED,    //CONNECT OK  
  TCP_CLOSING,
  TCP_CLOSED   //12
};

enum conn_state_enum{
    NO_RESPONSE = -1,
    CON_STATUS_DIFF = 0,
    CON_STATUS_OK
};
  
enum comm_line_status_enum 
{  
  // CLS like CommunicationLineStatus
  CLS_FREE,   // line is free - not used by the communication and can be used
  CLS_ATCMD,  // line is used by AT commands, includes also time for response
  CLS_DATA,   // for the future - line is used in the CSD or GPRS communication  
  CLS_LAST_ITEM
};

enum rx_state_enum 
{
  RX_NOT_FINISHED = 0,      // not finished yet
  RX_FINISHED,              // finished, some character was received
  RX_FINISHED_STR_RECV,     // finished and expected string received
  RX_FINISHED_STR_NOT_RECV, // finished, but expected string not received
  RX_TMOUT_ERR,             // finished, no character received                          
  RX_LAST_ITEM
};


enum at_resp_enum 
{
  AT_RESP_ERR_NO_RESP = -1,   // nothing received
  AT_RESP_ERR_DIF_RESP = 0,   // response_string is different from the response
  AT_RESP_OK = 1,             // response_string was included in the response
  AT_RESP_LAST_ITEM
};


enum getsms_ret_val_enum
{
  GETSMS_NO_SMS   = 0,
  GETSMS_UNREAD_SMS,
  GETSMS_READ_SMS,
  GETSMS_OTHER_SMS,
  GETSMS_NOT_AUTH_SMS,
  GETSMS_AUTH_SMS,
  GETSMS_LAST_ITEM
};

enum lcd_var_enum
{

  LCD_GSM_CON_STATUS=0,
  LCD_PRINT_LENGTH
};



enum GSM_state { ERROR, IDLE, READY, ATTACHED, TCPWAIT, TCPCONNECTED, BROKERCONNECTED, MQTTSUBSCRIBED };



class SIMCOM		
{									
  private:
	int _status;
    bool _retVal;
	byte comm_line_status;
    LCD1202* lcd_handler;
	
	char *p_comm_buf;               // pointer to the communication buffer
    byte comm_buf_len;              // num. of characters in the buffer
	byte rx_state;                  // internal state of rx state machine  
	uint16_t start_reception_tmout; // max tmout for starting reception
	uint16_t interchar_tmout;       // previous time in msec.
    unsigned long prev_time, statusTime;        // previous time in msec.
    unsigned long reset_time;
    
    int contVal, defaultVal;
    unsigned int datalength;
    unsigned char encodedByte;
    uint8_t topiclength;
    uint8_t MQTTProtocolNameLength;
    uint8_t MQTTClientIDLength;
    uint8_t MQTTUsernameLength;
    uint8_t MQTTPasswordLength;

    const char MQTTProtocolName[10] = "MQIsdp";
    const char MQTTLVL = 0x03;
    const char MQTTFlags = 0xC2;
    const unsigned int MQTTKeepAlive = 3600;

	char InitSMSMemory(void);

  void (*callback)(char*, uint8_t*, size_t);  // Callback signature
  	
   public:
    uint8_t LCDGSM[LCD_PRINT_LENGTH];
    SIMCOM(LCD1202* lcd_handler);
    void setCallback(void (*callback)(char*, uint8_t*, size_t)); // callback signature is passed
   
    bool sendSms(char* number,char* text);	 
   
    char comm_buf[COMM_BUF_LEN+1];  // communication buffer +1 for 0x00 termination
	char DeleteSMS(byte position); bool DeleteAll(); 
    void InitParam (byte group);
	virtual int begin(long baud_rate, uint8_t TryCont =4);
	char SendSMS(char *number_str, char *message_str); 
	char IsSMSPresent(byte required_status);
	int IsSMSPresent(char *phone_number, char *SMS_text, byte max_SMS_len);
	char GetSMS(byte position, char *phone_number, char *SMS_text, byte max_SMS_len);
	char GetSMS(char *phone_number, char *SMS_text, byte max_SMS_len); 	
	char GetAuthorizedSMS(byte position, char *phone_number, char *SMS_text, byte max_SMS_len, 
	     byte first_authorized_pos, byte last_authorized_pos);
    char GetTIME(char *current_Year, char *current_Month, char *current_Day, char *current_Hour, char *current_Minute);
    int GetTIME(uint8_t& _year, uint8_t& _month, uint8_t& _day_of_month, uint8_t& _hour, uint8_t& _minute, uint8_t& _second);
    int GetNetworkTime(uint32_t& _EPOC);
    inline void SetGSMState(GSM_state status) { _status = status;}  
    inline int GetGSMState(void) {return _status;};
	inline void SetCommLineStatus(byte new_status) {comm_line_status = new_status;};
	inline byte GetCommLineStatus(void) {return comm_line_status;};
	void RxInit(uint16_t start_comm_tmout, uint16_t max_interchar_tmout);
    int  CheckResp( unsigned int max_interchar_tmout);
    int IsRxFinished(void);
	int IsStringReceived(char const *compare_string);
	int WaitResp(uint16_t start_comm_tmout, uint16_t max_interchar_tmout);
    int WaitResp(uint16_t start_comm_tmout, uint16_t max_interchar_tmout, char const *expected_resp_string);
	int SendATCmdWaitResp(char const *AT_cmd_string, uint16_t start_comm_tmout, uint16_t max_interchar_tmout, char const *response_string, byte no_of_attempts);
	int SendATCmdWaitResp(const __FlashStringHelper *AT_cmd_string,uint16_t start_comm_tmout, uint16_t max_interchar_tmout,char const *response_string, byte no_of_attempts);	
		// Phonebook's methods
	char GetPhoneNumber(byte position, char *phone_number);
	char WritePhoneNumber(byte position, char *phone_number);
	char DelPhoneNumber(byte position);
	char ComparePhoneNumber(byte position, char *phone_number);
	void Echo(byte state);
    char signalStrength();
    int attachGPRS(char* domain, char* dom1="", char* dom2="");
    int ConnectTCP(const char* server_ip, const char* server_port);
    int connectionStatus();


    bool connectMQTT(const char* client_id, const char* username="", const char* password="");
    bool publishMQTT(const char* topic, const char* payload);
    bool publishMQTT(const char* topic, const uint8_t* payload, size_t payload_length);
    bool subscirbeMQTT(int packet_id, const char* topic, byte qos=1);
    inline void  RLengthEncode(int length);
    bool MQTTConnected();
    void MQTTloop(void);
    void parse_packet(char* packet, uint16_t packet_len);
};


#endif 
