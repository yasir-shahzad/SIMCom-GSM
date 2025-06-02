#include <GSMCore.h>


#ifdef ESP8266
#define D0 0
#define TxPin 4
#define RxPin 5
#endif
#define TxPin 11
#define RxPin 10
// Instantiate the library with TxPin, RxPin.
GSMCore modem(TxPin,RxPin);


        //  Get sms:
        char pos;  
        char phone_num[20]; // array for the phone number string
        char sms_text[196];

        bool gsm_started=false;


// Read sms:

        char* text;     // to storage the text of the sms
        char* number;
        
       
 
void setup() {
    
  Serial.begin(115200); // only for debug the results . 
  Serial.println("\nInitilizing GSM, Please wait....");
  if (modem.begin(19200)){
    Serial.println("\nstatus=READY");
    gsm_started=true;  
  }
  else {
    Serial.print("Error: GSM initilization failed");
  }
   text="Hi its the latest Yasir shahah test";  //text for the message. 
   number="+923106256643"; 

  if(gsm_started){
   if(modem.DeleteAll()) 
    Serial.println("All SMS deleted");
  }

}
void loop(){

  if( modem.IsSMSPresent(phone_num, sms_text, 196)){
     Serial.print("Phone num: ");
     Serial.println(phone_num);
     Serial.print("Sms text: ");
     Serial.println(sms_text);
  }

 
}

