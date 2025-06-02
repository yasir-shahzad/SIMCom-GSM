/******************************************************************************
themastemrind.pk
Source file for the GSM GSMCore Library

Yasirshahzad918@gmail.com themastermind Electronics
JAN 23, 2019
+923106256643

This file defines the hardware interface(s) for all the SIMCcom
and abstracts SMS AND CALL and other features of the GSMCore GSM modules

Development environment specifics:
Arduino 1.6.5+
GSMCore GSM Evaluation Board - SIM800L, SIM800C, SIM900, SIM808
******************************************************************************/

#ifndef SIMCOM_h
#define SIMCOM_h

#include <SoftwareSerial.h>
#include "Arduino.h"

//comment this to off the debugging
//#define DEBUG_ON

	
#define RESET_PIN 2   // pin to the reset pin GSMCore GSM

#define COMM_BUF_LEN        120

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

class GSMCore		
{									
  private:
	int _status;
	byte comm_line_status;
	
	byte *p_comm_buf;               // pointer to the communication buffer
    byte comm_buf_len;              // num. of characters in the buffer
	byte rx_state;                  // internal state of rx state machine  
	uint16_t start_reception_tmout; // max tmout for starting reception
	uint16_t interchar_tmout;       // previous time in msec.
    unsigned long prev_time;        // previous time in msec.

	char InitSMSMemory(void);
  //	char setRate(long baudRate);
  	
   public:
     GSMCore(int transmitPin, int receivePin);
    ~GSMCore();
    bool sendSms(char* number,char* text);	 
    enum GSM_st_e { ERROR, IDLE, READY, ATTACHED, TCPSERVERWAIT, TCPCONNECTEDSERVER, TCPCONNECTEDCLIENT };
    byte comm_buf[COMM_BUF_LEN+1];  // communication buffer +1 for 0x00 termination
    void test();
	char DeleteSMS(byte position); bool DeleteAll(); 
    void InitParam (byte group);
	virtual int begin(long baud_rate);
	char SendSMS(char *number_str, char *message_str); 
	char IsSMSPresent(byte required_status);
	int IsSMSPresent(char *phone_number, char *SMS_text, byte max_SMS_len);
	char GetSMS(byte position, char *phone_number, char *SMS_text, byte max_SMS_len);
	char GetSMS(char *phone_number, char *SMS_text, byte max_SMS_len); 	
	char GetAuthorizedSMS(byte position, char *phone_number, char *SMS_text, byte max_SMS_len, 
	     byte first_authorized_pos, byte last_authorized_pos);
    char GetTIME(char *current_Year, char *current_Month, char *current_Day, char *current_Hour, char *current_Minute);
    inline void setStatus(GSM_st_e status) { _status = status;}  
    
	inline void SetCommLineStatus(byte new_status) {comm_line_status = new_status;};
	inline byte GetCommLineStatus(void) {return comm_line_status;};
	void RxInit(uint16_t start_comm_tmout, uint16_t max_interchar_tmout);
    byte IsRxFinished(void);
	byte IsStringReceived(char const *compare_string);
	byte WaitResp(uint16_t start_comm_tmout, uint16_t max_interchar_tmout);
    byte WaitResp(uint16_t start_comm_tmout, uint16_t max_interchar_tmout, char const *expected_resp_string);
	char SendATCmdWaitResp(char const *AT_cmd_string, uint16_t start_comm_tmout, uint16_t max_interchar_tmout, char const *response_string, byte no_of_attempts);
	char SendATCmdWaitResp(const __FlashStringHelper *AT_cmd_string,uint16_t start_comm_tmout, uint16_t max_interchar_tmout,char const *response_string, byte no_of_attempts);	
		// Phonebook's methods
	char GetPhoneNumber(byte position, char *phone_number);
	char WritePhoneNumber(byte position, char *phone_number);
	char DelPhoneNumber(byte position);
	char ComparePhoneNumber(byte position, char *phone_number);
    void Call(char *);
    void Call(int );
	void Echo(byte state);
	SoftwareSerial *GSMserial;
   // char char_array[50];
     
};


#endif 