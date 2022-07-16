/******************************************************************************
themastemrind.pk
Source file for the GSM SIMCOM Library

Yasirshahzad @ themastermind Electronics
JAN 23, 2019


This file defines the hardware interface(s) for all the SIMCcom
and abstracts SMS AND CALL and other features of the SIMCOM GSM modules

Development environment specifics:
Arduino 1.6.5+
SIMCOM GSM Evaluation Board - SIM800L, SIM800C, SIM900, SIM808
******************************************************************************/
#include "Arduino.h"
#include "SIMCOM.h"

#ifdef ESP8266
 #include <pgmspace.h>
#endif
/*
  Value RSSI(receive signal strength indicator) dBm  Condition 
  4signal: 1marginal, 2Ok, 3Good, 4Excellent
        2 -109  Marginal
        3 -107  Marginal
        4 -105  Marginal
        5 -103  Marginal
        6 -101  Marginal
        7 -99 Marginal
        8 -97 Marginal
        9 -95 Marginal
        10  -93 OK
        11  -91 OK
        12  -89 OK
        13  -87 OK
        14  -85 OK
        15  -83 Good
        16  -81 Good
        17  -79 Good
        18  -77 Good
        19  -75 Good
        20  -73 Excellent
        21  -71 Excellent
        22  -69 Excellent
        23  -67 Excellent
        24  -65 Excellent
        25  -63 Excellent
        26  -61 Excellent
        27  -59 Excellent
        28  -57 Excellent
        29  -55 Excellent
        30  -53 Excellent
*/
int sqTable[] = {-115, -111, -109, -107, -105, -103, -101, -99, -97, -95, -93, -91, -89, -87, -85, -83, -81, -79, -77, -75, -73, -71, -69, -67, -65, -63, -61, -59, -57, -55, -53};

SIMCOM::SIMCOM(LCD1202* lcd_handler){
   this->lcd_handler = lcd_handler;

}



int SIMCOM::begin(long baud_rate, uint8_t TryCont){
 //   lcd_handler->battery(78,0,90,0);
//    lcd_handler->Update ();
    int cont=0;
	boolean norep=false;
	boolean turnedON=false;
	SetCommLineStatus(CLS_ATCMD);  
    Serial2.begin(baud_rate, SERIAL_8N1, RXD2, TXD2);
    pinMode(RESET_PIN, OUTPUT);
    digitalWrite(RESET_PIN, LOW);
  
	for (cont=0; cont<TryCont; cont++)   // trying for GSM print
    {
        Serial.print("\nTRY:");
        Serial.println(cont);
		if (!turnedON)
        { 
            if(cont > 0)delay(100);
            int resp = SendATCmdWaitResp("AT", 300, 100, "OK", 2);

            if( resp == AT_RESP_OK ){
                norep=false;
                break;
			}
           else if( resp == AT_RESP_ERR_NO_RESP || resp == AT_RESP_ERR_DIF_RESP)
            {
    			Serial.println(F("Resting GSM:Please wait..."));
    			digitalWrite(RESET_PIN, HIGH);
    			delay(2500);
    			digitalWrite(RESET_PIN, LOW);
    			delay(15000);
    			norep=true;
            }
			
		}
	}
	
	if (AT_RESP_OK == SendATCmdWaitResp("AT", 500, 100, "OK", 2)){
		turnedON=true;
        norep=false;
    }
    
	if(cont== TryCont && norep){
		Serial.println(F("Error: initializing GSM failed"));
        delay(200);
		Serial.println(F("Please check the power and serial connections, thanks"));
        SetCommLineStatus(CLS_FREE);
		return 0;
	}
	
	

	if(turnedON){   
	    // put the initializing settings here
		InitParam(PARAM_SET_0);
		InitParam(PARAM_SET_1);//configure the module 
		WaitResp(50, 50); 
		Echo(0);               //enable AT echo
		SetGSMState(READY);
        SetCommLineStatus(CLS_FREE);
		return(1);
	}
}



void SIMCOM::InitParam(byte group){
  
	switch (group) {
	case PARAM_SET_0:
		SetCommLineStatus(CLS_ATCMD);		
		SendATCmdWaitResp("AT&F", 1000, 50, "OK", 1);   // Reset to the factory settings    		
		SendATCmdWaitResp("ATE0", 500, 50, "OK", 1);    // switch off echo
		SendATCmdWaitResp("AT+CLTS=1", 1000, 50, "OK", 1);   // Enable auto network time sync
		//SendATCmdWaitResp("AT&W", 1000, 50, "OK", 1);   // Save the setting to permanent memory so that module enables sync on restart also	
		SetCommLineStatus(CLS_FREE);
		break;

	case PARAM_SET_1:
		SetCommLineStatus(CLS_ATCMD);		
		SendATCmdWaitResp(F("AT+CLIP=1"), 500, 50, "OK", 1);   // Request calling line identification		
		SendATCmdWaitResp(F("AT+CMEE=1"), 500, 50, "OK", 1);   // Mobile Equipment Error Code			
		SendATCmdWaitResp(F("AT+CMGF=1"), 500, 50, "OK", 1);   // set the SMS mode to text 				
		SendATCmdWaitResp(F("AT+CPBS=\"SM\""), 1000, 50, "OK", 1); // select phonebook memory storage
		SendATCmdWaitResp(F("AT+CIPSHUT"), 500, 50, "SHUT OK", 1);
		SetCommLineStatus(CLS_FREE);
		break;
	}
}


char SIMCOM::InitSMSMemory(void) 
{
  char ret_val = -1;

  if (CLS_FREE != GetCommLineStatus()) return (ret_val);
  SetCommLineStatus(CLS_ATCMD);
  ret_val = 0; // not initialized yet
 // SendATCmdWaitResp("AT+CNMI=1,2,0,0,0", 1000, 50, "OK", 2); // Disable messages about new SMS from the GSM module 
  if (AT_RESP_OK == SendATCmdWaitResp("AT+CPMS=\"SM\",\"SM\",\"SM\"", 1000, 10, "+CPMS:", 1)) {
       ret_val = 1;
  }
  else ret_val = 0;

  SetCommLineStatus(CLS_FREE);
  return (ret_val);
}

/**********************************************************
Method sends AT command and waits for response

return: 
      AT_RESP_ERR_NO_RESP = -1,   // no response received
      AT_RESP_ERR_DIF_RESP = 0,   // response_string is different from the response
      AT_RESP_OK = 1,             // response_string was included in the response
**********************************************************/
int SIMCOM::SendATCmdWaitResp(char const *AT_cmd_string, uint16_t start_comm_tmout, uint16_t max_interchar_tmout, char const *response_string, byte no_of_attempts)
{
  byte status;
  int ret_val = AT_RESP_ERR_NO_RESP;
  byte i;
  
  for (i = 0; i < no_of_attempts; i++) {

     if (i > 0) delay(300); 
    Serial2.println(AT_cmd_string);
    status = WaitResp(start_comm_tmout, max_interchar_tmout); 
    if (status == RX_FINISHED) {
      if(IsStringReceived(response_string)) {
        ret_val = AT_RESP_OK;      
        break;  // response is OK => finish
      }
      else ret_val = AT_RESP_ERR_DIF_RESP;
    }
    else {
      ret_val = AT_RESP_ERR_NO_RESP;
    }   
  }

  return (ret_val);
}


/**********************************************************
Method sends AT command and waits for response

return: 
      AT_RESP_ERR_NO_RESP = -1,   // no response received
      AT_RESP_ERR_DIF_RESP = 0,   // response_string is different from the response
      AT_RESP_OK = 1,             // response_string was included in the response
**********************************************************/
int SIMCOM::SendATCmdWaitResp(const __FlashStringHelper *AT_cmd_string, uint16_t start_comm_tmout, uint16_t max_interchar_tmout,char const *response_string, byte no_of_attempts)
{
  byte status;
  int ret_val = AT_RESP_ERR_NO_RESP;
  byte i;

  for (i = 0; i < no_of_attempts; i++) {
	
    if (i > 0) delay(300); 
    Serial2.println(AT_cmd_string);
    status = WaitResp(start_comm_tmout, max_interchar_tmout); 
    if (status == RX_FINISHED) {
      if(IsStringReceived(response_string)) {
        ret_val = AT_RESP_OK;      
        break;  // response is OK
      }
      else ret_val = AT_RESP_ERR_DIF_RESP;
    }
    else {
        
      ret_val = AT_RESP_ERR_NO_RESP;
    }    
  }

  return (ret_val);
}



int SIMCOM::WaitResp(uint16_t start_comm_tmout, uint16_t max_interchar_tmout, char const *expected_resp_string)
{
  byte status;
  int ret_val;

  RxInit(start_comm_tmout, max_interchar_tmout); 
//  long int strtim=millis();
  do {    
        status = IsRxFinished();
		#ifdef ESP8266
			yield();
		#endif
     } while (status == RX_NOT_FINISHED);
 //       Serial.print("\nresp:");
 //  Serial.println((millis()-strtim));
  if (status == RX_FINISHED) {
    if(IsStringReceived(expected_resp_string)) {  
      ret_val = RX_FINISHED_STR_RECV;     
    }
    else {  //string received but compare does not match
	  ret_val = RX_FINISHED_STR_NOT_RECV;
	}
  }
  else {
    ret_val = RX_TMOUT_ERR;
  }
  return (ret_val);
}


/**********************************************************
Method waits for the given time or till response

compare_string - pointer to the string which should be find

return: 0 - string was NOT received
        1 - string was received
**********************************************************/

int SIMCOM::WaitResp(uint16_t start_comm_tmout, uint16_t max_interchar_tmout)
{
  int status;

  RxInit(start_comm_tmout, max_interchar_tmout);  // starting initilizations
  do {
   
    status = IsRxFinished();    
  } while (status == RX_NOT_FINISHED);
  return (status);
}



int SIMCOM::CheckResp( unsigned int max_interchar_tmout)
{
  int status;
  comm_buf_len = 0;
  rx_state = RX_NOT_STARTED;
  interchar_tmout = max_interchar_tmout;
  start_reception_tmout = 1;
  prev_time = millis();
  comm_buf[0] = 0x00; // end of string
  p_comm_buf = &comm_buf[0];

  do {
    status = IsRxFinished();
  } while (status == RX_NOT_FINISHED);
  return (status);
}


/**********************************************************
Method checks received bytes

compare_string - pointer to the string which should be find

return: 0 - string was NOT received
        1 - string was received
**********************************************************/

int SIMCOM::IsRxFinished(void)
{
  byte num_of_bytes;
  int ret_val = RX_NOT_FINISHED;  // default not finished

  if (rx_state == RX_NOT_STARTED) {

    if (!Serial2.available()) {

      if ((unsigned long)(millis() - prev_time) >= start_reception_tmout) {
        comm_buf[comm_buf_len] = '\0';
        ret_val = RX_TMOUT_ERR;
      }  
    }
    else {
      prev_time = millis(); // init tmout for inter-character space
      rx_state = RX_ALREADY_STARTED;
    }
  }

  if (rx_state == RX_ALREADY_STARTED) {
    num_of_bytes = Serial2.available();
  
    if (num_of_bytes) prev_time = millis();
    
    while (num_of_bytes) { 
      num_of_bytes--;
      if (comm_buf_len < COMM_BUF_LEN) {
        *p_comm_buf = Serial2.read();
        p_comm_buf++;
        comm_buf_len++;
        comm_buf[comm_buf_len] = '\0';   // and finish currently received characters                              
      }
      else {
        Serial2.read();
      }
    }
    if ((unsigned long)(millis() - prev_time) >= interchar_tmout) {
      comm_buf[comm_buf_len] = '\0';  // for sure finish string again
	  #ifdef Partial_Debugg
	  Serial.print("Buffer Data:");
	  Serial.println(comm_buf[comm_buf_len]);
	   #endif
	 
      ret_val = RX_FINISHED;
	  
    }
  }
			
  return (ret_val);
}









/**********************************************************
Method checks received bytes

compare_string - pointer to the string which should be find

return: 0 - string was NOT received
        1 - string was received
**********************************************************/
int SIMCOM::IsStringReceived(char const *compare_string)
{
  char *ch;
  int ret_val = 0;

  if(comm_buf_len) {
	#ifdef DEBUG_ON || Partial_Debugg
		Serial.print("ATT: ");
		Serial.print(compare_string);
		Serial.print("RIC: ");
		Serial.println((char *)comm_buf);
	#endif
    ch = strstr((char *)comm_buf, compare_string);
    if (ch != NULL) {
      ret_val = 1;
    }
  }

  return (ret_val);
}

//http://www.smssolutions.net/tutorials/gsm/receivesmsat/

void SIMCOM::RxInit(uint16_t start_comm_tmout, uint16_t max_interchar_tmout)
{
  rx_state = RX_NOT_STARTED;
  start_reception_tmout = start_comm_tmout;
  interchar_tmout = max_interchar_tmout;
  prev_time = millis();
  comm_buf[0] = 0x00; // end of string
  p_comm_buf = &comm_buf[0];
  comm_buf_len = 0;
  Serial2.flush(); // erase rx circular buffer
}

void SIMCOM::Echo(byte state)
{
	if (state == 0 or state == 1)
	{
	  SetCommLineStatus(CLS_ATCMD);

	  Serial2.print("ATE");
	  Serial2.print((int)state);    
	  Serial2.print("\r");
	  delay(500);
	  SetCommLineStatus(CLS_FREE);   
	}
}




/**********************************************************
Method sends SMS

number_str:   pointer to the phone number string
message_str:  pointer to the SMS text string


return: 
        ERROR ret. val:
        ---------------
        -1 - comm. line to the GSM module is not free
        -2 - GSM module didn't answer in timeout
        -3 - GSM module has answered "ERROR" string

        OK ret val:
        -----------
        0 - SMS was not sent
        1 - SMS was sent


an example of usage:
        GSM gsm;
        Serial2.SendSMS("00XXXYYYYYYYYY", "SMS text");
**********************************************************/
char SIMCOM::SendSMS(char *number_str, char *message_str) 
{
  char ret_val = -1;
  byte i;
  char end[2]; // ctrlz
  end[0]=0x1a;
  end[1]='\0';
  
  if (CLS_FREE != GetCommLineStatus()) return (ret_val);
  SetCommLineStatus(CLS_ATCMD);
  ret_val = 0;
  
    Serial2.print(F("AT+CMGF=1\r\n")); //set sms to text mode
	Serial.println("AT+CMGF");
	delay(50);
    Serial2.flush();
    Serial2.print(F("AT+CMGS=\""));  // command to send sms
    Serial2.print(number_str);  
    Serial2.println(F("\"\r"));
    Serial.println("AT+CMGS");
    if (RX_FINISHED_STR_RECV == WaitResp(2000, 300, ">")) {
		Serial.println("DEBUG:>");
      Serial2.print(message_str); 
	  //Serial2.print((char)26);	  
      Serial2.println(end);
	  //Serial2.flush(); // erase rx circular buffer
    //  if (RX_FINISHED_STR_RECV == WaitResp(5000, 100, "+CMGS")) {  
	//   Serial.println("SMS Successfully Sent: Acknowledgement received");
    //  ret_val = 1;    // SMS was send correctly 
   ///  }
    }
   


  SetCommLineStatus(CLS_FREE);
  return (ret_val);
}







/**********************************************************
Method finds out if there is present at least one SMS with
specified status

Note:
if there is new SMS before IsSMSPresent() is executed
this SMS has a status UNREAD and then
after calling IsSMSPresent() method status of SMS
is automatically changed to READ

required_status:  SMS_UNREAD  - new SMS - not read yet
                  SMS_READ    - already read SMS                  
                  SMS_ALL     - all stored SMS

return: 
        ERROR ret. val:
        ---------------
        -1 - comm. line to the GSM module is not free
        -2 - GSM module didn't answer in timeout

        OK ret val:
        -----------
        0 - there is no SMS with specified status
        1..20 - position where SMS is stored 
                (suitable for the function GetSMS())


an example of use:
        GSM gsm;
        char position;  
        char phone_number[20]; // array for the phone number string
        char sms_text[100];

        position = IsSMSPresent(SMS_UNREAD);
        if (position) {
          // read new SMS
          GetSMS(position, phone_num, sms_text, 100);
          // now we have phone number string in phone_num
          // and SMS text in sms_text
        }
**********************************************************/
char SIMCOM::IsSMSPresent(byte required_status) 
{
  char ret_val = -1;
  char *p_char;
  byte status;

  if (CLS_FREE != GetCommLineStatus()) return (ret_val);
  SetCommLineStatus(CLS_ATCMD);
  ret_val = 0; // still not present

  switch (required_status) {
    case SMS_UNREAD:
      Serial2.println(F("AT+CMGL=\"REC UNREAD\",1\r\n"));
      break;
    case SMS_READ:
      Serial2.println(F("AT+CMGL=\"REC READ\""));
      break;
    case SMS_ALL:
      Serial2.println(F("AT+CMGL=\"ALL\""));
      break;
  }

  RxInit(2000, 1500); 
  do {
    if (IsStringReceived("OK")) { 
      // perfect - we have some response, but what:
      status = RX_FINISHED;
      break; 
    }
    status = IsRxFinished();
  } while (status == RX_NOT_FINISHED);

  switch (status) {
    case RX_TMOUT_ERR:
      ret_val = -2;
      break;

    case RX_FINISHED:
      if(IsStringReceived("+CMGL:")) { 
        p_char = strchr((char *)comm_buf,':');
        if (p_char != NULL) {
          ret_val = atoi(p_char+1);
        }
      }
      else {
        ret_val = 0;
      }
      WaitResp(20, 20); 
      break;
  }

  SetCommLineStatus(CLS_FREE);
  return (ret_val);
}





/**********************************************************
Method reads SMS from specified memory(SIM) position

position:     SMS position <1..20>
phone_number: a pointer where the phone number string of received SMS will be placed
              so the space for the phone number string must be reserved - see example
SMS_text  :   a pointer where SMS text will be placed
max_SMS_len:  maximum length of SMS text excluding also string terminating 0x00 character
              
return: 
        ERROR ret. val:
        ---------------
        -1 - comm. line to the GSM module is not free
        -2 - GSM module didn't answer in timeout
        -3 - specified position must be > 0

        OK ret val:
        -----------
        GETSMS_NO_SMS       - no SMS was not found at the specified position
        GETSMS_UNREAD_SMS   - new SMS was found at the specified position
        GETSMS_READ_SMS     - already read SMS was found at the specified position
        GETSMS_OTHER_SMS    - other type of SMS was found 


an example of usage:
        GSM gsm;
        char position;
        char phone_num[20]; // array for the phone number string
        char sms_text[100]; // array for the SMS text string

        position = IsSMSPresent(SMS_UNREAD);
        if (position) {
          // there is new SMS => read it
          GetSMS(position, phone_num, sms_text, 100);
          #ifdef DEBUG_PRINT
            Serial2.DebugPrint("DEBUG SMS phone number: ", 0);
            Serial2.DebugPrint(phone_num, 0);
            Serial2.DebugPrint("\r\n          SMS text: ", 0);
            Serial2.DebugPrint(sms_text, 1);
          #endif
        }        
**********************************************************/
char SIMCOM::GetSMS(byte position, char *phone_number, char *SMS_text, byte max_SMS_len) 
{
  char ret_val = -1;
  char *p_char; 
  char *p_char1;
  byte len;

  if (position == 0) return (-3);
  if (CLS_FREE != GetCommLineStatus()) return (ret_val);
  SetCommLineStatus(CLS_ATCMD);
  phone_number[0] = 0;  // end of string for now
  ret_val = GETSMS_NO_SMS; // still no SMS
  
  //send "AT+CMGR=X" - where X = position
  Serial2.print(F("AT+CMGR="));
  Serial2.println((int)position);  

  switch (WaitResp(5000, 100, "+CMGR")) {
    case RX_TMOUT_ERR:
      // response was not received in specific time
      ret_val = -2;
      break;

    case RX_FINISHED_STR_NOT_RECV:
      // OK was received => there is NO SMS stored in this position
      if(IsStringReceived("OK")) {
        // there is only response <CR><LF>OK<CR><LF> 
        ret_val = GETSMS_NO_SMS;
      }
      else if(IsStringReceived("ERROR")) {
        // error should not be here but for sure
        ret_val = GETSMS_NO_SMS;
      }
      break;

    case RX_FINISHED_STR_RECV:
      // find out what was received exactly
      if(IsStringReceived("\"REC UNREAD\"")) { 
        ret_val = GETSMS_UNREAD_SMS;
      }
      //response for already read SMS = old SMS:

      else if(IsStringReceived("\"REC READ\"")) {
        // get phone number of received SMS
        // --------------------------------
        ret_val = GETSMS_READ_SMS;
      }
      else {
        // other type like stored for sending.. 
        ret_val = GETSMS_OTHER_SMS;
      }

      // extract phone number string
      // ---------------------------
      p_char = strchr((char *)(comm_buf),',');
      p_char1 = p_char+2; // we are on the first phone number character
      p_char = strchr((char *)(p_char1),'"');
      if (p_char != NULL) {
        *p_char = 0; // end of string
        strcpy(phone_number, (char *)(p_char1));
      }


      // get SMS text and copy this text to the SMS_text buffer
      // ------------------------------------------------------
      p_char = strchr(p_char+1, 0x0a);  // find <LF>
      if (p_char != NULL) {
        // next character after <LF> is the first SMS character
        p_char++; // now we are on the first SMS character 

        // find <CR> as the end of SMS string
        p_char1 = strchr((char *)(p_char), 0x0d);  
        if (p_char1 != NULL) {
          // finish the SMS text string 
          // because string must be finished for right behaviour 
          // of next strcpy() function
          *p_char1 = 0; 
        }
        // in case there is not finish sequence <CR><LF> because the SMS is
        len = strlen(p_char);

        if (len < max_SMS_len) {
          // buffer SMS_text has enough place for copying all SMS text
          // so copy whole SMS text
          // from the beginning of the text(=p_char position) 
          // to the end of the string(= p_char1 position)
          strcpy(SMS_text, (char *)(p_char));
        }
        else {
          // buffer SMS_text doesn't have enough place for copying all SMS text
          memcpy(SMS_text, (char *)(p_char), (max_SMS_len-1));
          SMS_text[max_SMS_len] = 0; // finish string
        }
      }
      break;
  }

  SetCommLineStatus(CLS_FREE);
  return (ret_val);
}



/**********************************************************
Method deletes SMS from the specified SMS position

position:     SMS position <1..20>

return: 
        ERROR ret. val:
        ---------------
        -1 - comm. line to the GSM module is not free
        -2 - GSM module didn't answer in timeout
        -3 - position must be > 0

        OK ret val:
        -----------
        0 - SMS was not deleted
        1 - SMS was deleted
**********************************************************/
char SIMCOM::DeleteSMS(byte position) 
{
  char ret_val = -1;

  if (position == 0) return (-3);
  if (CLS_FREE != GetCommLineStatus()) return (ret_val);
  SetCommLineStatus(CLS_ATCMD);
  ret_val = 0; // not deleted yet
  
  //send "AT+CMGD=XY" - where XY = position
  Serial2.print(F("AT+CMGD="));
  Serial2.println((int)position);  


  // 5000 msec. for initial comm tmout
  // 20 msec. for inter character timeout
  switch (WaitResp(5000, 50, "OK")) {
    case RX_TMOUT_ERR:
      // response was not received in specific time
      ret_val = -2;
      break;

    case RX_FINISHED_STR_RECV:
      // OK was received => SMS deleted
      ret_val = 1;
      break;

    case RX_FINISHED_STR_NOT_RECV:
      // other response: e.g. ERROR => SMS was not deleted
      ret_val = 0; 
      break;
  }

  SetCommLineStatus(CLS_FREE);
  return (ret_val);
}


/**********************************************************
Method deletes SMS from all the positions

position:     SMS position <1..50>

return: 
        ERROR ret. val:
        ---------------
        -1 - comm. line to the GSM module is not free
        -2 - GSM module didn't answer in timeout

        OK ret val:
        -----------
        0 - SMS was not deleted
        1 - All the SMS was deleted
**********************************************************/


bool SIMCOM::DeleteAll(){ 
  char ret_val = -1;
  if (CLS_FREE != GetCommLineStatus()) return (ret_val);
  SetCommLineStatus(CLS_ATCMD);
  ret_val = 0; // not deleted yet
  
  Serial2.print(F("AT+CMGDA=\"DELL ALL\"\n\r"));
    switch (WaitResp(2000, 50, "OK")) {
    case RX_TMOUT_ERR:
      // response was not received in specific time
      ret_val = -2;
      break;

    case RX_FINISHED_STR_RECV:
      // OK was received => SMS deleted
      ret_val = 1;
      break;

    case RX_FINISHED_STR_NOT_RECV:
      // other response: e.g. ERROR => SMS was not deleted
      ret_val = 0; 
      break;
  }
    SetCommLineStatus(CLS_FREE);
    return (ret_val); 
}


/**********************************************************
Method finds out if there is present at least one SMS with
specified status

Note:
if there is new SMS before IsSMSPresent() is executed
this SMS has a status UNREAD and then
after calling IsSMSPresent() method status of SMS
is automatically changed to READ

required_status:  SMS_UNREAD  - new SMS - not read yet
                  SMS_READ    - already read SMS                  
                  SMS_ALL     - all stored SMS

return: 
        ERROR ret. val:
        ---------------
        -1 - comm. line to the GSM module is not free
        -2 - GSM module didn't answer in timeout

        OK ret val:
        -----------
        0 - there is no SMS with specified status
        1..20 - position where SMS is stored 
                (suitable for the function GetSMS())


an example of use:
        GSM gsm;
        char position;  
        char phone_number[20]; // array for the phone number string
        char sms_text[100];

        position = IsSMSPresent(SMS_UNREAD);
        if (position) {
          // read new SMS
          GetSMS(position, phone_num, sms_text, 100);
          // now we have phone number string in phone_num
          // and SMS text in sms_text
        }
**********************************************************/
int SIMCOM::IsSMSPresent(char *phone_number, char *SMS_text, byte max_SMS_len) 
{
  int ret_val = -1;
  int i;
   char *p_char; 
  char *p_char1;
  byte len;
  
  
  if (CLS_FREE != GetCommLineStatus()) return (ret_val);
  SetCommLineStatus(CLS_ATCMD);
  ret_val = 0; // still not present
	 
   if(Serial2.available() > 0){
        
        delay(20); 
     // while  (Serial2.available() > 0)  Serial.write(Serial2.read());

		
        String Command=Serial2.readString();
		//Serial.println("Ready to print command");
		//Serial.println(Command);
		
	 
    int n = Command.length();  
      
    // declaring character array 
    char char_array[n+1];
	  
    // copying the contents of the  
    // string to char array 
    strcpy(char_array, Command.c_str());
	//Serial.println("Printing Char");
	//Serial.print(char_array);
		
		while(Serial2.available() > 0)  // Clearing all the data
                Serial2.read();
	 
      if(strstr((char *)char_array, "+CMT:") != NULL){
	     phone_number[0] = 0;
	  
	  
	  
	          // extract phone number string
      // ---------------------------
      p_char = strchr((char *)char_array,':');
      p_char1 = p_char+3; // we are on the first phone number character
      p_char = strchr((char *)(p_char1),'"');
      if (p_char != NULL) {
        *p_char = 0; // end of string
        strcpy(phone_number, (char *)(p_char1));
      }

      // get SMS text and copy this text to the SMS_text buffer
      // ------------------------------------------------------
      p_char = strchr(p_char+1, 0x0a);  // find <LF>
      if (p_char != NULL) {
        // next character after <LF> is the first SMS character
        p_char++; // now we are on the first SMS character 

        // find <CR> as the end of SMS string
        p_char1 = strchr((char *)(p_char), 0x0d);  
        if (p_char1 != NULL) {
          // finish the SMS text string 
          // because string must be finished for right behaviour 
          // of next strcpy() function
          *p_char1 = 0; 
        }
        // in case there is not finish sequence <CR><LF> because the SMS is
        len = strlen(p_char);

        if (len < max_SMS_len) {
          // buffer SMS_text has enough place for copying all SMS text
          // so copy whole SMS text
          // from the beginning of the text(=p_char position) 
          // to the end of the string(= p_char1 position)
          strcpy(SMS_text, (char *)(p_char));
        }
        else {
          // buffer SMS_text doesn't have enough place for copying all SMS text
          memcpy(SMS_text, (char *)(p_char), (max_SMS_len-1));
          SMS_text[max_SMS_len] = 0; // finish string
        }
      }
	  
	  
	    Serial.println("Sms found at serial");
        ret_val = 1;		
      }
	  
  
  } 

  SetCommLineStatus(CLS_FREE);
  return (ret_val);
}




char SIMCOM::GetTIME(char *current_Year, char *current_Month, char *current_Day, char *current_Hour, char *current_Minute) 
{
  int ret_val = -1;
  int i;
   char *p_char; 
  char *p_char1;
  byte len;
  
  if (CLS_FREE != GetCommLineStatus()) return (ret_val);
  SetCommLineStatus(CLS_ATCMD);
  ret_val = 0; // still not present
	 Serial2.print(F("AT+CCLK?\r\n")); //set sms to text mode
	 delay(400); 
   if(Serial2.available() > 0){
        
        delay(20); 
     // while  (Serial2.available() > 0)  Serial.write(Serial2.read());

		
        String Command=Serial2.readString();
		//Serial.println("Ready to print command");
		//Serial.println(Command);
		
	 
    int n = Command.length();  
      
    // declaring character array 
    char char_array[n+1];
	  
    // copying the contents of the  
    // string to char array 
    strcpy(char_array, Command.c_str());
	//Serial.println("Printing Char");
	//Serial.print(char_array);
		
		while(Serial2.available() > 0)  // Clearing all the data
                Serial2.read();
	 
      if(strstr((char *)char_array, "+CCLK:") != NULL){
	     current_Year[0] = 0;
	  
	  
	  
	          // extract phone number string
      // ---------------------------------------
      p_char = strchr((char *)char_array,':');
      p_char1 = p_char+3; // we are on the first phone number character
      p_char = strchr((char *)(p_char1),'/');
      if (p_char != NULL) {
        *p_char = 0; // end of string
        strcpy(current_Year, (char *)(p_char1));
		 p_char1 = p_char+1;
      }
	  
	
	   p_char = strchr((char *)(p_char1),'/'); 
     if (p_char != NULL) {
        *p_char = 0; // end of string      
        strcpy(current_Month, (char *)(p_char1));
        p_char1 = p_char+1;
       // printf("String current_Month |%s|\n", current_Month);
      }
      
      
     p_char = strchr((char *)(p_char1),',');  
     if (p_char != NULL) {
        *p_char = 0; // end of string      
        strcpy(current_Day, (char *)(p_char1));
        p_char1 = p_char+1;
        printf("String current_Day |%s|\n", current_Day);
      }
      
      p_char = strchr((char *)(p_char1),':');  
     if (p_char != NULL) {
        *p_char = 0; // end of string      
        strcpy(current_Hour, (char *)(p_char1));
        p_char1 = p_char+1;
        printf("String current_Hour |%s|\n", current_Hour);
      }
      
        p_char = strchr((char *)(p_char1),':');  
     if (p_char != NULL) {
        *p_char = 0; // end of string      
        strcpy(current_Minute, (char *)(p_char1));
        p_char1 = p_char+1;
        printf("String current_Minute |%s|\n", current_Minute);
      }

 
      
	  
	  
	    Serial.println("Sms found at serial");
        ret_val = 1;		
      }
	  
  
  } 

  SetCommLineStatus(CLS_FREE);
  return (ret_val);
}

int SIMCOM::GetTIME(uint8_t& _year, uint8_t& _month, uint8_t& _day_of_month, uint8_t& _hour, uint8_t& _minute, uint8_t& _second){

    int ret_val = -1;
    if (CLS_FREE != GetCommLineStatus()) return (ret_val);
    SetCommLineStatus(CLS_ATCMD);
    ret_val = 0;
    Serial2.println(F("AT+CCLK?")); 
    delay(2000); 
    if(Serial2.available() > 0){
        String GSM_DATETIME = Serial2.readString();
        Serial.print("GMS TIME:"); Serial.println(GSM_DATETIME);
        while(Serial2.available() > 0)
            Serial2.read();
        if(GSM_DATETIME.indexOf("+CCLK:") != -1){
            GSM_DATETIME.replace("+CCLK:", "");
            GSM_DATETIME.replace("OK", "");
            GSM_DATETIME.replace("\"", "");
            GSM_DATETIME.trim();
            Serial.print("GMS TIME:"); Serial.println(GSM_DATETIME);
            sscanf(GSM_DATETIME.c_str(), "%d/%d/%d,%d:%d:%d", &_year, &_month, &_day_of_month, &_hour, &_minute, &_second);
            ret_val = 1;    
        }
    } 
    SetCommLineStatus(CLS_FREE);
    return (ret_val);
}
int SIMCOM::GetNetworkTime(uint32_t& EPOC){
    uint8_t PACKET[48];
    memset(PACKET, 0, 48);
    int ret_val = -1;
    if (CLS_FREE != GetCommLineStatus()) return (ret_val);
    SetCommLineStatus(CLS_ATCMD);
    ret_val = 0;
    if(SendATCmdWaitResp("AT+CIPSTART=\"UDP\", \"3.pk.pool.ntp.org\", \"123\"" , 1000, 20, "OK", 1) == AT_RESP_OK){
        Serial.println("UDP STARTED");
        if(WaitResp (15000, 50, "CONNECT OK") == RX_FINISHED_STR_RECV){
            Serial.println("UDP CONNECT OK");
            // IniTialize values needed To form NTP requesT
            PACKET[0] = 0b11100011;   // LI, Version, Mode
            PACKET[1] = 0;     // STraTum, or Type of clock
            PACKET[2] = 6;     // Polling InTerval
            PACKET[3] = 0xEC;  // Peer Clock Precision
            PACKET[12]  = 49;
            PACKET[13]  = 0x4E;
            PACKET[14]  = 49;
            PACKET[15]  = 52;
             
           Serial2.print(F("AT+CIPSEND\r\n"));  // command to send sms
            int resp = WaitResp(1000, 50, ">");
            if (resp == RX_FINISHED_STR_RECV ){
                delay(1);
                Serial.print("SENT "); Serial.println(Serial2.write(PACKET, 48));
                Serial2.write(0x1A);
            } 
            if(WaitResp (5000, 50, "SEND OK") == RX_FINISHED_STR_RECV){
                  unsigned long START_UDP_WAIT = millis();
                  delay(10);
                 
                  while(millis() - START_UDP_WAIT < 1000){
                      delay(100);
                      if(Serial2.read(PACKET, 48) == 48){
                          uint32_t SECONDS_SINCE_1900 = ( word(PACKET[40], PACKET[41]) << 16 | word(PACKET[42], PACKET[43]) ) + 18000;
                          EPOC = SECONDS_SINCE_1900 - 2208988800UL;
                          Serial.println("RECEIVED UDP");
                          ret_val = 1;
                          break;
                          
                      }
                  }
            } 
            SendATCmdWaitResp("AT+CIPSHUT\r\n", 1000, 6, "SHUT OK", 1);
            Serial2.readString();
        }      
                
    }
    SetCommLineStatus(CLS_FREE);
    return (ret_val);
  
}

char SIMCOM:: signalStrength(){
    char ret_val = 0;
    int i;
    char *p_char; 
    char *p_char1;
    byte len;
    
    if (CLS_FREE != GetCommLineStatus()) return (ret_val);
      SetCommLineStatus(CLS_ATCMD);

     Serial2.print(F("AT+CSQ\r\n")); 

     int resp =  WaitResp(700, 100, "+CSQ");
     if (resp == RX_FINISHED_STR_RECV) {
        p_char = strchr((char *)comm_buf,':');
        
        if (p_char != NULL) {
          ret_val = atoi(p_char+1);
          ret_val   = map(ret_val, 2, 30, 0, 100);
          ret_val   =   constrain(ret_val , 0, 100);
        }
      
      else {
        ret_val = -1;
      }
    }


    SetCommLineStatus(CLS_FREE);
    return (ret_val);


}



int SIMCOM:: attachGPRS(char* APN, char* USER, char* PWD){
    int ret_val = 0;
    byte status;
    byte count = 0;
    
    char domian_buffer[100];
    sprintf(domian_buffer, "AT+CSTT=\"%s\",\"%s\",\"%s\"\r\n", APN, USER, PWD);
    if (CLS_FREE != GetCommLineStatus()) return (ret_val);
      SetCommLineStatus(CLS_ATCMD);
   
       SendATCmdWaitResp("AT+CIPCLOSE\r\n", 1000, 6, "OK", 2); 
       SendATCmdWaitResp("AT+CIPSHUT\r\n", 1000, 6, "SHUT OK", 2); 
       SendATCmdWaitResp(domian_buffer, 300, 4, "OK", 2);  //  status =  SendATCmdWaitResp("AT+CSTT=\"connect.mobilinkworld.com\",\"\",\"\"\r\n", 300, 10, "OK", 2);
    
     
       delay(500);
       status = SendATCmdWaitResp("AT+CIICR\r\n", 10000, 6, "OK", 2); 
 
        if (AT_RESP_OK == status){

           delay(1000);     
           Serial2.print("AT+CIFSR\r\n");
           status =  WaitResp (500, 10);

           #ifdef ResponseDebug
           Serial.print("LOCAL IP:");
           Serial.println((char*)comm_buf); 
           #endif
      
         if (status == RX_FINISHED) {
            for(int i= 0; i < comm_buf_len; i++){
                if(comm_buf[i] == 0x2E)
                    count++;
             }
                               
             if(count == 3 ){
                 ret_val = 1;
                 SetGSMState (ATTACHED);
                }
             else
                 ret_val = 0;
           }

        }
     SetCommLineStatus(CLS_FREE);
     return (ret_val);
}


int SIMCOM:: ConnectTCP(const char* server_ip, const char* server_port){
    char com_buffer[100];
    int ret_val = -1;
    byte status;

    
    if (CLS_FREE != GetCommLineStatus()) return (ret_val);
      SetCommLineStatus(CLS_ATCMD);
      sprintf(com_buffer, "AT+CIPSTART =\"TCP\",\"%s\", \"%s\"\r\n", server_ip, server_port);

      status = SendATCmdWaitResp(com_buffer, 1000, 20, "OK", 1);  //SendATCmdWaitResp("AT+CIPSTART =\"TCP\", \"210.56.21.194\", \"1883\"\r\n", 2000, 10, "OK", 1);

      if(status == AT_RESP_OK){
        if(!IsStringReceived("CONNECT OK")){
         status =  WaitResp (15000, 50, "CONNECT OK");
         if(status == RX_FINISHED_STR_RECV){
            ret_val = 1;
            SetGSMState (TCPCONNECTED);
           }
         else 
            ret_val = 0;
         }
        else {
            SetGSMState (TCPCONNECTED);
            ret_val = 1;
         }
      }
     // if(ret_val == 1 ) Serial.println("TCP IS connected");
     SetCommLineStatus(CLS_FREE);
     return (ret_val);
 }



int SIMCOM:: connectionStatus(){   
    int ret_val = -1;
    byte status;
   

    if (CLS_FREE != GetCommLineStatus()) return (ret_val);
      SetCommLineStatus(CLS_ATCMD);
      
      Serial2.print("AT+CIPSTATUS\r\n");
      status = WaitResp (500, 100);
    if(status == RX_FINISHED){                    //    Serial.print("CIPSTATUS:>"); Serial.println((char*)comm_buf); 
                        
        if(IsStringReceived ("CONNECT OK"))
            ret_val = TCP_CONNECTED;
       else if(IsStringReceived ("IP STATUS"))
            ret_val = GPRS_CONNECTED;
       else if(IsStringReceived ("TCP CLOSED"))
            ret_val = TCP_CLOSED;
       else if(IsStringReceived ("PDP DEACT"))
            ret_val = GPRS_DISCONNECTED;
       else if(IsStringReceived ("IP GPRSACT"))
            ret_val = IP_GPRSACT;
       else if(IsStringReceived ("TCP CONNECTING"))
            ret_val = TCP_WAIT;
       else if(IsStringReceived ("IP INITIAL"))
            ret_val = IP_INITIAL;
       else if(IsStringReceived ("IP CONFIG"))
            ret_val = IP_CONFIG;
       else if(IsStringReceived ("IP START"))
            ret_val = IP_START;
       else if(IsStringReceived ("TCP CLOSING"))
            ret_val = TCP_CLOSING;
       else
            ret_val = UNKNOWN_RESP;
                        
      }
    else if (status == RX_TMOUT_ERR){          
                ret_val = NO_RESP;
    }
    else {
        //can restart gsm here if not responding
        ret_val = -2; // there is no response
     }
     SetCommLineStatus(CLS_FREE);
     return (ret_val);
}

bool  SIMCOM::connectMQTT(const char* client_id, const char* username, const char* password){
         bool ret_val = false;
         byte status;
         
    if (CLS_FREE != GetCommLineStatus()) return (ret_val);
       SetCommLineStatus(CLS_ATCMD);

       Serial2.print(F("AT+CIPSEND\r\n"));  // command to send sms

       int resp = WaitResp(1000, 50, ">");

    if (resp == RX_FINISHED_STR_RECV ) {
          delay(1);
          Serial2.write(0x10);
          MQTTProtocolNameLength = strlen(MQTTProtocolName);
          MQTTClientIDLength = strlen(client_id);
          MQTTUsernameLength = strlen(username);
          MQTTPasswordLength = strlen(password);
          
          if( MQTTUsernameLength == 0 || MQTTPasswordLength == 0)
            datalength = 2+ MQTTProtocolNameLength + 4 + 2+ MQTTClientIDLength;
          else
            datalength = 2+ MQTTProtocolNameLength + 4 + 2+ MQTTClientIDLength+2+MQTTUsernameLength+2+MQTTPasswordLength;
            
          unsigned int X = datalength;
          RLengthEncode(X);

          Serial2.write(MQTTProtocolNameLength >> 8);
          Serial2.write(MQTTProtocolNameLength & 0xFF);
          Serial2.print(MQTTProtocolName);
          Serial2.write(MQTTLVL); // LVL
          Serial2.write(MQTTFlags); // Flags
          Serial2.write(MQTTKeepAlive >> 8);
          Serial2.write(MQTTKeepAlive & 0xFF);
          Serial2.write(MQTTClientIDLength >> 8);
          Serial2.write(MQTTClientIDLength & 0xFF);
          Serial2.print(client_id);
          Serial2.write(0x1A); 
       } 
       status = WaitResp (10000, 50, "SEND OK");
    
     if(status == RX_FINISHED_STR_RECV){
        SetGSMState (BROKERCONNECTED);
       // Serial.println("MQTT Connected");
        ret_val = true;
        }
     SetCommLineStatus(CLS_FREE);
     return (ret_val);    

 }
bool SIMCOM::publishMQTT(const char* topic, const char* payload){
        bool ret_val = false;
     
    if (CLS_FREE != GetCommLineStatus()) return (ret_val);
      SetCommLineStatus(CLS_ATCMD);

    Serial2.print(F("AT+CIPSEND\r\n"));  // command to send sms
    
    int resp = WaitResp(1000, 50, ">");
    
    if (resp == RX_FINISHED_STR_RECV) {
        topiclength = strlen(topic);
        datalength = strlen(payload);
        String data = String(topic) + String(payload);
    
        delay(1);
        /* BAIG CHANGED*/
        Serial2.write(0x30 | 1);
    
        unsigned int X = 2 + topiclength + datalength ;
        RLengthEncode(X);

        Serial2.write(topiclength >> 8);
        Serial2.write(topiclength & 0xFF);
        Serial2.print(data);
        Serial2.write(0x1A);
     }

     if(RX_FINISHED_STR_RECV == WaitResp (10000, 50, "SEND OK"))
        ret_val = true;

     SetCommLineStatus(CLS_FREE);
     return (ret_val);   
}

bool SIMCOM::publishMQTT(const char* topic, const uint8_t* payload, size_t payload_length){
        bool ret_val = false;
     
    if (CLS_FREE != GetCommLineStatus()) return (ret_val);
      SetCommLineStatus(CLS_ATCMD);

    Serial2.print(F("AT+CIPSEND\r\n"));  // command to send sms
    
    int resp = WaitResp(1000, 50, ">");
    
    if (resp == RX_FINISHED_STR_RECV) {
        topiclength = strlen(topic);
        datalength = payload_length;
        String data = String(topic);
    
        delay(5);
        /* BAIG CHANGED*/
        Serial2.write(0x30 | 1);
    
        unsigned int X = 2 + topiclength + datalength ;
        RLengthEncode(X);

        Serial2.write(topiclength >> 8);
        Serial2.write(topiclength & 0xFF);
        Serial2.print(data);
        for(int i = 0; i < payload_length; i++)
            Serial2.write(payload[i]);
        Serial2.write(0x1A);
     }

     if(RX_FINISHED_STR_RECV == WaitResp (10000, 50, "SEND OK"))
        ret_val = true;

     SetCommLineStatus(CLS_FREE);
     return (ret_val);   



}



bool SIMCOM::subscirbeMQTT(int packet_id, const char* topic, byte qos) {
       bool ret_val = false; 
       
       if (CLS_FREE != GetCommLineStatus()) return (ret_val);
         SetCommLineStatus(CLS_ATCMD);
    
         Serial2.print(F("AT+CIPSEND\r\n"));  // command to send sms    
    
       if (RX_FINISHED_STR_RECV == WaitResp(1000, 50, ">")) {
           topiclength =strlen(topic);
           
           delay(1);
           Serial2.write(0x82);
           
           unsigned int X = 2 + 2 + topiclength + 1;
           RLengthEncode(X);
           
           Serial2.write(packet_id >> 8);
           Serial2.write(packet_id & 0xFF);
           Serial2.write(topiclength >> 8);
           Serial2.write(topiclength & 0xFF);
           Serial2.print(topic);
           Serial2.write(qos);
           Serial2.write(0x1A);
        }

        if(RX_FINISHED_STR_RECV == WaitResp (10000, 50, "SEND OK")){
           SetGSMState (MQTTSUBSCRIBED);
         //  Serial.println("MQTT SUBSCRIBED");
           ret_val = true;
          }
        SetCommLineStatus(CLS_FREE);
        return (ret_val);

  }
inline void SIMCOM::RLengthEncode(int length){
    unsigned int len = length;
    uint8_t encoded_byte;
    
        do {
               encoded_byte = len % 128;
               len = len / 128;
              if (len > 0) {
                 encoded_byte |= 128;
                }
             Serial2.write(encoded_byte);
        }
        while (len > 0);

}


void SIMCOM::MQTTloop(void){
         
      if(Serial2.available() > 0){ 
         int resp =  CheckResp (5); 

        if(resp == RX_FINISHED){ 

         #ifdef DebugResponse
            for(int i =0; i < comm_buf_len; i++){ 
            Serial.print(">");
            Serial.print(i);
            Serial.print(":");
            Serial.write(comm_buf[i]);
            }
         #endif


         if(IsStringReceived ("CLOSED")){
           statusTime = millis();
           Serial.println("TCP CLOSED");
          }
            
        else   

          if (comm_buf_len > 5){
              uint8_t index = 0;
              char topic[16];                //increase the size accordingly
              uint8_t payload[128];          ////increase the size accordingly
              uint16_t RL = 0;
              uint16_t multiplier = 1;
              uint8_t encodedByte; 
                        
            char type = comm_buf[index++];

            if(type == 0x30){


              do {
                encodedByte = comm_buf[index++];                
                uint32_t intermediate = encodedByte & 0x7F;
                intermediate *= multiplier;
                RL += intermediate;
                multiplier *= 128;
                if (multiplier > (128UL * 128UL * 128UL)) {
                  Serial.print(F("Malformed packet len\n"));
                 // return 0;
                }
              } while (encodedByte & 0x80);


           uint16_t topic_len = ((comm_buf[index++] << 8) | comm_buf[index++]);


                for(uint16_t i = 0; i < topic_len; i++){
                   topic[i] = comm_buf[index + i];
                 }
                topic[topic_len] = 0x00;


           uint16_t payload_length = RL - (topic_len+2);  



                for(uint16_t i = 0; i < payload_length; i++){
                  payload[i] = comm_buf[index+topic_len+i];

                 }
                payload[payload_length] = 0x00;

          callback(topic,payload, payload_length);

           
       
      }
            }
            }
        }
      
}


/*
 Value RSSI dBm  Condition
        2 -109  Marginal
        3 -107  Marginal
        4 -105  Marginal
        5 -103  Marginal
        6 -101  Marginal
        7 -99 Marginal
        8 -97 Marginal
        9 -95 Marginal
        10  -93 OK
        11  -91 OK
        12  -89 OK
        13  -87 OK
        14  -85 OK
        15  -83 Good
        16  -81 Good
        17  -79 Good
        18  -77 Good
        19  -75 Good
        20  -73 Excellent
        21  -71 Excellent
        22  -69 Excellent
        23  -67 Excellent
        24  -65 Excellent
        25  -63 Excellent
        26  -61 Excellent
        27  -59 Excellent
        28  -57 Excellent
        29  -55 Excellent
        30  -53 Excellent
*
void SIMCOM::signalQuality(){
/*Response
+CSQ: <rssi>,<ber>Parameters
<rssi>
0 -115 dBm or less
1 -111 dBm
2...30 -110... -54 dBm
31 -52 dBm or greater
99 not known or not detectable
<ber> (in percent):
0...7 As RXQUAL values in the table in GSM 05.08 [20]
subclause 7.2.4
99 Not known or not detectable 
*
  Serial2.print(F("AT+CSQ\r\n"));
  //Serial.println(_readSerial());
}

void getSignalQuality(){

  SIMCOM.signalQuality();
  
}


*/

bool SIMCOM :: MQTTConnected(){

  if(millis() > statusTime){
      _retVal = 0;
   int respVal =  connectionStatus ();
   int gsmSTATE = GetGSMState ();
   
    if(LCDGSM[LCD_GSM_CON_STATUS] != respVal) LCDGSM[LCD_GSM_CON_STATUS] = respVal;
   
   /*
      lcd_handler->fillTriangle(15,40,15,40, 45, 60,1  );
      lcd_handler->print(15, 40, 1, "RESP:");
      lcd_handler->print(45 , 40, 1, respVal);
      lcd_handler->Update ();
*/
    //  #ifdef StatusDebug
    //  Serial.print(">");
    //  Serial.println(respVal);
   //   #endif

    switch(respVal)
    {
        case TCP_CONNECTED:
            
          switch(gsmSTATE){
             case MQTTSUBSCRIBED:
                 statusTime = millis()+20000;
                 _retVal = 1;
             break;
             case BROKERCONNECTED:
                  subscirbeMQTT (1, MQTT_TOPIC_SUB_REQ);
                  subscirbeMQTT (1, MQTT_TOPIC_SUB_TIMER_REQ);
                 
             break;
             case TCPCONNECTED:
                 connectMQTT (MQTT_CONNECT_ID);
                  defaultVal = 0;
                
              }
        break;
        case TCP_CLOSED: case GPRS_CONNECTED:  
                       
            //ConnectTCP ("210.56.21.194", "1883");
            ConnectTCP(_Settings.getBrokerIP().c_str(), "1883");
            
        break;
        case TCP_WAIT:  case TCP_CLOSING:
            
            statusTime = millis()+2000;  
        break;
        case IP_GPRSACT: case IP_INITIAL: case GPRS_DISCONNECTED:  
        case IP_CONFIG: case IP_START:    
           attachGPRS("connect.mobilinkworld.com");
          
        break;       
        case NO_RESP:
            
            contVal +=1;
            statusTime = millis()+2000;
                 if(contVal == 4 && millis() > reset_time){
                    Serial.println("in restart  GSM");
                    contVal = 0;
                    reset_time = millis()+600000;
                    begin (115200, 2);
                 }             
        break;
            
        default:
            defaultVal +=1;
            if(defaultVal == 8)
                attachGPRS("connect.mobilinkworld.com");
            else           
                statusTime = millis()+2000;
        break;
    }
      
  }

  return _retVal;
}

void SIMCOM::setCallback(void (*callback)(char*, uint8_t*, size_t)){
    this->callback = callback;

}

void SIMCOM::parse_packet(char* packet, uint16_t packet_len){

    char topic[20];
    uint8_t payload[100];

                char type = packet[0];
                if (type == '0' ) {
                     uint16_t tl = (packet[2]<<8)+packet[3]; /* topic length in bytes */

                        //= (char*) packet+llen+2;
                        memset(topic, '0', 20);
                        memset(payload, 0x00, 100);
                        strncpy(topic, packet+4, tl);
                        topic[tl] = '\0';
                        size_t payload_length = packet_len - tl - 1 -1;
                        memcpy(payload, packet+4+tl, payload_length);
                        payload[payload_length] = '\0';
                        
             
                        callback(topic,payload, payload_length);
                        // msgId only present for QOS>0
                        /*
                        if ((this->buffer[0]&0x06) == MQTTQOS1) {
                            msgId = (this->buffer[llen+3+tl]<<8)+this->buffer[llen+3+tl+1];
                            payload = this->buffer+llen+3+tl+2;
                            callback(topic,payload,len-llen-3-tl-2);

                            this->buffer[0] = MQTTPUBACK;
                            this->buffer[1] = 2;
                            this->buffer[2] = (msgId >> 8);
                            this->buffer[3] = (msgId & 0xFF);
                            _client->write(this->buffer,4);
                            lastOutActivity = t;

                        } else { 
                            payload = packet+llen+3+tl;
                            Serial.println(topic);
                            Serial.println(payload);
                            Serial.println(packet_len-llen-3-tl);
                           // callback(topic,payload,len-llen-3-tl);*/
                    }


}






 //+923106256643   //WHATSAPP
