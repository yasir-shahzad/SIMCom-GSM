/******************************************************************************
themastemrind.pk
Source file for the GSM SIMCOM Library

Yasirshahzad918@gmail.com themastermind Electronics
JAN 23, 2019


This file defines the hardware interface(s) for all the SIMCcom
and abstracts SMS, GPRS AND CALL and other features of the SIMCOM GSM modules

Development environment specifics:
Arduino 1.6.5+
SIMCOM GSM Evaluation Board - SIM800L, SIM800C, SIM900, SIM808
******************************************************************************/
#include "Arduino.h"
#include "SIMCom.h"
#include <SoftwareSerial.h>
#ifdef ESP8266
 #include <pgmspace.h>
#endif

int sqTable[] = {-115, -111, -109, -107, -105, -103, -101, -99, -97, -95, -93, -91, -89, -87, -85, -83, -81, -79, -77, -75, -73, -71, -69, -67, -65, -63, -61, -59, -57, -55, -53};

SIMCOM::SIMCOM(int transmitPin, int receivePin) {
#ifdef ESP8266
    GSMserial = new SoftwareSerial(receivePin, transmitPin, false, 1024);
#else
    GSMserial = new SoftwareSerial(receivePin, transmitPin, false);
#endif
}

SIMCOM::~SIMCOM() {
    delete GSMserial; // delete it to avoide any memory leak
}

 
int SIMCOM::begin(long baud_rate){
	int response=-1;
	int cont=0;
	boolean norep=false;
	boolean turnedON=false;
	SetCommLineStatus(CLS_ATCMD);  
	GSMserial->begin(baud_rate);	
    GSMserial->flush();
	for (cont=0; cont<3; cont++)
    {
        Serial.print(F("Try:"));
        Serial.println(cont);
		if (!turnedON)
        {
            int resp = SendATCmdWaitResp("AT", 500, 100, "OK", 3);
            if( resp == AT_RESP_OK ){
				Serial.print("recieved Ok");
                break;
			}
            if( resp == AT_RESP_ERR_NO_RESP || resp == AT_RESP_ERR_DIF_RESP)
            {
			// generate turn on pulse
			Serial.print(F("Resting GSM:Please wait..."));
			digitalWrite(RESET_PIN, HIGH);
			delay(2500);
			digitalWrite(RESET_PIN, LOW);
			delay(15000);
			norep=true;
            }
			
		}
		else{
			#ifdef DEBUG_ON
			Serial.println(F("DB:ELSE"));
			#endif
			norep=false;
		}
	}
	
	if (AT_RESP_OK == SendATCmdWaitResp("AT", 500, 100, "OK", 3)){
		turnedON=true;
    }
	
	if(cont==2 && norep){
		Serial.println(F("Error: initializing GSM failed"));
		delay(700);
		Serial.println(F("Please check the power and serial connections, thanks"));
		return 0;
	}
	
	SetCommLineStatus(CLS_FREE);

	if(turnedON){   
	    // put the initializing settings here
		InitParam(PARAM_SET_0);
		InitParam(PARAM_SET_1);//configure the module 
		WaitResp(50, 50);
	    Serial.println("Done"); 
		Echo(0);                   //enable AT echo
		setStatus(READY);
		return(1);
	}
}



void SIMCOM::InitParam(byte group){
  
	switch (group) {
	case PARAM_SET_0:
		SetCommLineStatus(CLS_ATCMD);		
		SendATCmdWaitResp("AT&F", 1000, 50, "OK", 5);   // Reset to the factory settings    		
		SendATCmdWaitResp("ATE0", 500, 50, "OK", 5);    // switch off echo
		SendATCmdWaitResp("AT+CLTS=1", 1000, 50, "OK", 5);   // Enable auto network time sync
		SendATCmdWaitResp("AT&W", 1000, 50, "OK", 5);   // Save the setting to permanent memory so that module enables sync on restart also
		
		SetCommLineStatus(CLS_FREE);
		break;

	case PARAM_SET_1:
		SetCommLineStatus(CLS_ATCMD);		
		SendATCmdWaitResp(F("AT+CLIP=1"), 500, 50, "OK", 5);   // Request calling line identification		
		SendATCmdWaitResp(F("AT+CMEE=0"), 500, 50, "OK", 5);   // Mobile Equipment Error Code			
		SendATCmdWaitResp(F("AT+CMGF=1"), 500, 50, "OK", 5);   // set the SMS mode to text 		
		SetCommLineStatus(CLS_FREE);  						   // checks comm line if it is free		
		InitSMSMemory();                                       // init SMS storage		
		SendATCmdWaitResp(F("AT+CPBS=\"SM\""), 1000, 50, "OK", 5);  // select phonebook memory storage
		SendATCmdWaitResp(F("AT+CIPSHUT"), 500, 50, "SHUT OK", 5);
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
  SendATCmdWaitResp("AT+CNMI=1,2,0,0,0", 1000, 50, "OK", 2); // Disable messages about new SMS from the GSM module 
  if (AT_RESP_OK == SendATCmdWaitResp("AT+CPMS=\"SM\",\"SM\",\"SM\"", 1000, 1000, "+CPMS:", 10)) {
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
char SIMCOM::SendATCmdWaitResp(char const *AT_cmd_string, uint16_t start_comm_tmout, uint16_t max_interchar_tmout, char const *response_string, byte no_of_attempts)
{
  byte status;
  char ret_val = AT_RESP_ERR_NO_RESP;
  byte i;

  for (i = 0; i < no_of_attempts; i++) {
    
     if (i > 0) delay(300); 
    GSMserial->println(AT_cmd_string);
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
char SIMCOM::SendATCmdWaitResp(const __FlashStringHelper *AT_cmd_string, uint16_t start_comm_tmout, uint16_t max_interchar_tmout,char const *response_string, byte no_of_attempts)
{
  byte status;
  char ret_val = AT_RESP_ERR_NO_RESP;
  byte i;

  for (i = 0; i < no_of_attempts; i++) {
	
    if (i > 0) delay(300); 
    GSMserial->println(AT_cmd_string);
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



byte SIMCOM::WaitResp(uint16_t start_comm_tmout, uint16_t max_interchar_tmout, char const *expected_resp_string)
{
  byte status;
  byte ret_val;

  RxInit(start_comm_tmout, max_interchar_tmout); 
 // long int strtim=millis();
  do {    
        status = IsRxFinished();
		#ifdef ESP8266
			yield();
		#endif
     } while (status == RX_NOT_FINISHED);
 // Serial.print((millis()-strtim));
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

byte SIMCOM::WaitResp(uint16_t start_comm_tmout, uint16_t max_interchar_tmout)
{
  byte status;

  RxInit(start_comm_tmout, max_interchar_tmout);  // starting initilizations
  do {
    status = IsRxFinished();
  } while (status == RX_NOT_FINISHED);
  return (status);
}


byte SIMCOM::IsRxFinished(void)
{
  byte num_of_bytes;
  byte ret_val = RX_NOT_FINISHED;  // default not finished

  if (rx_state == RX_NOT_STARTED) {
    if (!GSMserial->available()) {
      if ((unsigned long)(millis() - prev_time) >= start_reception_tmout) {
        comm_buf[comm_buf_len] = 0x00;
        ret_val = RX_TMOUT_ERR;
      }  
    }
    else {
      prev_time = millis(); // init tmout for inter-character space
      rx_state = RX_ALREADY_STARTED;
    }
  }

  if (rx_state == RX_ALREADY_STARTED) {
    num_of_bytes = GSMserial->available();
    if (num_of_bytes) prev_time = millis();
    
    while (num_of_bytes) { 
      num_of_bytes--;
      if (comm_buf_len < COMM_BUF_LEN) {
        *p_comm_buf = GSMserial->read();
        p_comm_buf++;
        comm_buf_len++;
        comm_buf[comm_buf_len] = 0x00;   // and finish currently received characters                              
      }
      else {
        GSMserial->read();
      }
    }
    if ((unsigned long)(millis() - prev_time) >= interchar_tmout) {
      comm_buf[comm_buf_len] = 0x00;  // for sure finish string again
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
byte SIMCOM::IsStringReceived(char const *compare_string)
{
  char *ch;
  byte ret_val = 0;

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
	else
	{
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
  GSMserial->flush(); // erase rx circular buffer
}

void SIMCOM::Echo(byte state)
{
	if (state == 0 or state == 1)
	{
	  SetCommLineStatus(CLS_ATCMD);

	  GSMserial->print("ATE");
	  GSMserial->print((int)state);    
	  GSMserial->print("\r");
	  delay(500);
	  SetCommLineStatus(CLS_FREE);   
	}
}


/**********************************************************
Method calls the specific number
number_string: pointer to the phone number string
               e.g. gsm.Call("+420123456789");
**********************************************************/
void SIMCOM::Call(char *number_string)
{
   //  if (CLS_FREE != gsm.GetCommLineStatus()) return;
   ///  gsm.SetCommLineStatus(CLS_ATCMD);
     // ATDxxxxxx;<CR>
     GSMserial->print(F("ATD"));
     GSMserial->print(number_string);
     GSMserial->println(F(";"));
     // 10 sec. for initial comm tmout
     // 50 msec. for inter character timeout
     WaitResp(10000, 50);
   //  gsm.SetCommLineStatus(CLS_FREE);
}

/**********************************************************
Method calls the number stored at the specified SIM position
sim_position: position in the SIM <1...>
              e.g. gsm.Call(1);
**********************************************************/
void SIMCOM::Call(int sim_position)
{     
     //char ret_val = -1;
     //if (CLS_FREE != GetCommLineStatus()) return (ret_val);
    // SetCommLineStatus(CLS_ATCMD);
     // ATD>"SM" 1;<CR>
     GSMserial->print(F("ATD>\"SM\" "));
     GSMserial->print(sim_position);
     GSMserial->println(F(";"));

     // 10 sec. for initial comm tmout
     // 50 msec. for inter character timeout
     WaitResp(10000, 50);

     //SetCommLineStatus(CLS_FREE);
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
  
    GSMserial->print(F("AT+CMGF=1\r\n")); //set sms to text mode
	Serial.println("AT+CMGF");
	delay(50);
    GSMserial->flush();
    GSMserial->print(F("AT+CMGS=\""));  // command to send sms
    GSMserial->print(number_str);  
    GSMserial->println(F("\"\r"));
    Serial.println("AT+CMGS");
    if (RX_FINISHED_STR_RECV == WaitResp(2000, 300, ">")) {
		Serial.println("DEBUG:>");
      GSMserial->print(message_str); 
	  //GSMserial->print((char)26);	  
      GSMserial->println(end);
	  //GSMserial->flush(); // erase rx circular buffer
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
      GSMserial->println(F("AT+CMGL=\"REC UNREAD\",1\r\n"));
      break;
    case SMS_READ:
      GSMserial->println(F("AT+CMGL=\"REC READ\""));
      break;
    case SMS_ALL:
      GSMserial->println(F("AT+CMGL=\"ALL\""));
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
  GSMserial->print(F("AT+CMGR="));
  GSMserial->println((int)position);  

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
  GSMserial->print(F("AT+CMGD="));
  GSMserial->println((int)position);  


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
  
  GSMserial->print(F("AT+CMGDA=\"DELL ALL\"\n\r"));
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
	 
   if(GSMserial->available() > 0){
        
        delay(20); 
     // while  (GSMserial->available() > 0)  Serial.write(GSMserial->read());

		
        String Command=GSMserial->readString();
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
		
		while(GSMserial->available() > 0)  // Clearing all the data
            GSMserial->read();
	 
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
	 GSMserial->print(F("AT+CCLK?\r\n")); //set sms to text mode
	 delay(400); 
   if(GSMserial->available() > 0){
        
        delay(20); 
     // while  (GSMserial->available() > 0)  Serial.write(GSMserial->read());

		
        String Command=GSMserial->readString();
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
		
		while(GSMserial->available() > 0)  // Clearing all the data
                GSMserial->read();
	 
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
        printf("String current_Month |%s|\n", current_Month);
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




/*



void get_sms(char *phone_number, char *SMS_text, byte max_SMS_len) 
{
      char ret_val = -1;
      char *p_char; 
      char *p_char1;
      byte len;

      phone_number[0] = 0;  // end of string for now

        // extract phone number string
      // ---------------------------
      p_char = strchr((char *)(GSMserial->_receive_buffer),':');
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
  GSMserial->print(F("AT+CSQ\r\n"));
  //Serial.println(_readSerial());
}

void getSignalQuality(){

  SIMCOM.signalQuality();
  
}


*/




 //+923106256643   //WHATSAPP
