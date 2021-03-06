/* SPWFInterface Example
 * Copyright (c) 2015 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "SPWFSA01.h"
#include "mbed_debug.h"

#define SPWFSA01_CONNECT_TIMEOUT    15000
#define SPWFSA01_SEND_TIMEOUT       500
#define SPWFSA01_RECV_TIMEOUT       1500//some commands like AT&F/W takes some time to get the result back!
#define SPWFSA01_MISC_TIMEOUT       500
#define SPWFSA01_SOCKQ_TIMEOUT      3000

SPWFSA01::SPWFSA01(PinName tx, PinName rx, bool debug)
    : _serial(tx, rx, 1024), _parser(_serial),
      _wakeup(D14, PIN_INPUT, PullNone, 0), _reset(D15, PIN_INPUT, PullNone, 1),
      //PC_12->D15, PC_8->D14 (re-wires needed in-case used, currently not used)
      dbg_on(debug)
      //Pin PC_8 is wakeup pin
      //Pin PA_12 is reset pin
{
    _serial.baud(115200);
    _reset.output();
    _wakeup.output();
    _parser.debugOn(debug);
}

bool SPWFSA01::startup(int mode)
{
    _parser.setTimeout(SPWFSA01_MISC_TIMEOUT);    
    /*Test module before reset*/
    waitSPWFReady();
    /*Reset module*/
    reset();
     
    /*set local echo to 0*/
    if(!(_parser.send("AT+S.SCFG=localecho1,%d\r", 0) && _parser.recv("OK"))) 
        {
            debug_if(dbg_on, "SPWF> error local echo set\r\n");
            return false;
        }   
    /*reset factory settings*/
    if(!(_parser.send("AT&F") && _parser.recv("OK"))) 
        {
            debug_if(dbg_on, "SPWF> error AT&F\r\n");
            return false;
        }
        
    /*set Wi-Fi mode and rate to b/g/n*/
    if(!(_parser.send("AT+S.SCFG=wifi_ht_mode,%d\r",1) && _parser.recv("OK")))
        {
            debug_if(dbg_on, "SPWF> error setting ht_mode\r\n");
            return false;
        }
        
    if(!(_parser.send("AT+S.SCFG=wifi_opr_rate_mask,0x003FFFCF\r") && _parser.recv("OK")))
        {
            debug_if(dbg_on, "SPWF> error setting operational rates\r\n");
            return false;
        }

    /*set idle mode (0->idle, 1->STA,3->miniAP, 2->IBSS)*/
    if(!(_parser.send("AT+S.SCFG=wifi_mode,%d\r", mode) && _parser.recv("OK")))
        {
            debug_if(dbg_on, "SPWF> error wifi mode set\r\n");
            return false;
        } 

    /* save current setting in flash */
    if(!(_parser.send("AT&W") && _parser.recv("OK")))
        {
            debug_if(dbg_on, "SPWF> error AT&W\r\n");
            return false;
        }
        
    /*reset again and send AT command and check for result (AT->OK)*/
    reset();
        
    return true;    
}

bool SPWFSA01::hw_reset(void)
{    
    if (_reset.is_connected()) {
    /* reset the pin PC12 */  
    _reset.write(0);
    wait_ms(200);
    _reset.write(1); 
    wait_ms(100);
    return 1;
    } else { return 0; }
}

bool SPWFSA01::reset(void)
{
    if(!_parser.send("AT+CFUN=1")) return false;
    while(1) {
        if (_parser.recv("+WIND:32:WiFi Hardware Started\r")) {
            return true;
        }
    }
}

void SPWFSA01::waitSPWFReady(void)
{
    //wait_ms(200);
    while(1) 
        if(_parser.send("AT") && _parser.recv("OK"))
            //till we get OK from AT command
            //printf("\r\nwaiting for reset to complete..\n");
            return;
                 
}

/* Security Mode
    None          = 0, 
    WEP           = 1,
    WPA_Personal  = 2,
*/
bool SPWFSA01::connect(const char *ap, const char *passPhrase, int securityMode)
{
    uint32_t n1, n2, n3, n4;

    _parser.setTimeout(SPWFSA01_CONNECT_TIMEOUT);       
    //AT+S.SCFG=wifi_wpa_psk_text,%s\r
    if(!(_parser.send("AT+S.SCFG=wifi_wpa_psk_text,%s", passPhrase) && _parser.recv("OK"))) 
        {
            debug_if(dbg_on, "SPWF> error pass set\r\n");
            return false;
        } 
    //AT+S.SSIDTXT=%s\r
    if(!(_parser.send("AT+S.SSIDTXT=%s", ap) && _parser.recv("OK"))) 
        {
            debug_if(dbg_on, "SPWF> error ssid set\r\n");
            return false;
        }
    //AT+S.SCFG=wifi_priv_mode,%d\r
    if(!(_parser.send("AT+S.SCFG=wifi_priv_mode,%d", securityMode) && _parser.recv("OK"))) 
        {
            debug_if(dbg_on, "SPWF> error security mode set\r\n");
            return false;
        } 
    //"AT+S.SCFG=wifi_mode,%d\r"
    /*set idle mode (0->idle, 1->STA,3->miniAP, 2->IBSS)*/
    if(!(_parser.send("AT+S.SCFG=wifi_mode,%d\r", 1) && _parser.recv("OK")))
        {
            debug_if(dbg_on, "SPWF> error wifi mode set\r\n");
            return false;
        }
    //AT&W
    /* save current setting in flash */
    if(!(_parser.send("AT&W") && _parser.recv("OK")))
        {
            debug_if(dbg_on, "SPWF> error AT&W\r\n");
            return false;
        }
    //reset module
    reset();
    
    while(1)
        if((_parser.recv("+WIND:24:WiFi Up:%u.%u.%u.%u",&n1, &n2, &n3, &n4)))
            {
                break;
            }            
        
    return true;
}

bool SPWFSA01::disconnect(void)
{
    //"AT+S.SCFG=wifi_mode,%d\r"
    /*set idle mode (0->idle, 1->STA,3->miniAP, 2->IBSS)*/
    if(!(_parser.send("AT+S.SCFG=wifi_mode,%d\r", 0) && _parser.recv("OK")))
        {
            debug_if(dbg_on, "SPWF> error wifi mode set\r\n");
            return false;
        }
    //AT&W
    /* save current setting in flash */
    if(!(_parser.send("AT&W") && _parser.recv("OK")))
        {
            debug_if(dbg_on, "SPWF> error AT&W\r\n");
            return false;
        }
    //reset module
    reset();
    return true;
}

bool SPWFSA01::dhcp(int mode)
{
    //only 3 valid modes
    //0->off(ip_addr must be set by user), 1->on(auto set by AP), 2->on&customize(miniAP ip_addr can be set by user)
    if(mode < 0 || mode > 2) {
        return false;
    }
        
    return _parser.send("AT+S.SCFG=ip_use_dhcp,%d\r", mode)
        && _parser.recv("OK");
}


const char *SPWFSA01::getIPAddress(void)
{
    uint32_t n1, n2, n3, n4;
    
    if (!(_parser.send("AT+S.STS=ip_ipaddr")
        && _parser.recv("#  ip_ipaddr = %u.%u.%u.%u", &n1, &n2, &n3, &n4)
        && _parser.recv("OK"))) {
            debug_if(dbg_on, "SPWF> getIPAddress error\r\n");
        return 0;
    }

    sprintf((char*)_ip_buffer,"%u.%u.%u.%u", n1, n2, n3, n4);

    return _ip_buffer;
}

const char *SPWFSA01::getMACAddress(void)
{
    uint32_t n1, n2, n3, n4, n5, n6;
    
    if (!(_parser.send("AT+S.GCFG=nv_wifi_macaddr")
        && _parser.recv("#  nv_wifi_macaddr = %x:%x:%x:%x:%x:%x", &n1, &n2, &n3, &n4, &n5, &n6)
        && _parser.recv("OK"))) {
            debug_if(dbg_on, "SPWF> getMACAddress error\r\n");
        return 0;
    }

    sprintf((char*)_mac_buffer,"%02X:%02X:%02X:%02X:%02X:%02X", n1, n2, n3, n4, n5, n6);

    return _mac_buffer;
}

bool SPWFSA01::isConnected(void)
{
    return getIPAddress() != 0; 
}

bool SPWFSA01::open(const char *type, int* id, const char* addr, int port)
{
    Timer timer;
    timer.start();
    socket_closed = 0; 
       
	if (!_parser.send("AT+S.SOCKON=%s,%d,%s", addr, port, type))
// RSR if (!_parser.send("AT+S.SOCKON=%s,%d,%s,ind", addr, port, type))
		{
            debug_if(dbg_on, "SPWF> error opening socket\r\n");
            return false;
        }
        
    while(1)
        {
            if( _parser.recv(" ID: %d", id)
                && _parser.recv("OK"))
                break;
            
            if (timer.read_ms() > SPWFSA01_CONNECT_TIMEOUT) {
                return false;
            }
        
            //TODO:implement time-out functionality in case of no response
            //if(timeout) return false;
            //TODO: deal with errors like "ERROR: Failed to resolve name"
            //TODO: deal with errors like "ERROR: Data mode not available"
        }

    return true;
}

bool SPWFSA01::send(int id, const void *data, uint32_t amount)
{    
    char _buf[18];
    _parser.setTimeout(SPWFSA01_SEND_TIMEOUT);
    
    sprintf((char*)_buf,"AT+S.SOCKW=%d,%d\r", id, amount);   
	debug_if(dbg_on, "AT+S.SOCKW=%d,%d\r", id, amount);  // DEBUG

    //May take a second try if device is busy
    for (unsigned i = 0; i < 2; i++) {
        if (_parser.write((char*)_buf, strlen(_buf)) >=0
            && _parser.write((char*)data, (int)amount) >= 0
            && _parser.recv("OK")) {
            return true;
        }     
    }
    return false;
}


int32_t SPWFSA01::recv(int id, void *data, uint32_t amount)
{
    uint32_t recv_amount=0;
    int wind_id;    
	int reply;

	_parser.setTimeout(SPWFSA01_RECV_TIMEOUT); // RSR

	if (socket_closed) {
        socket_closed = 0;
        return -3;
    }
	reply = 50;
	do {
#if 1
		if (!(_parser.send("AT+S.SOCKQ=%d", id)  //send a query (will be required for secure sockets)
			&& _parser.recv(" DATALEN: %u", &recv_amount)
			&& _parser.recv("OK"))) {
			return -2;
		}
#else
		if (!(_parser.recv(" +WIND:55:Pending Data:0:%d", &recv_amount)))
			return -2;
#endif
	} while (recv_amount == 0 && reply-- > 0 );
    if (recv_amount==0) { return -1; } 
    if(recv_amount > amount)
        recv_amount = amount;
#if 1
    int par_timeout = _parser.getTimeout();        
    _parser.setTimeout(0);
    
     while(_parser.recv("+WIND:%d:", &wind_id)) {
//        printf("Wind received: %d\n\r", wind_id);
        if (wind_id == 58) {
            socket_closed = 1;
            _parser.flush();            
        }
    }       
    _parser.setTimeout(par_timeout);
	// RSR _parser.flush();
#endif    
    if(!(_parser.send("AT+S.SOCKR=%d,%d", id, recv_amount))){
        return -2;    
    }
    if(!((_parser.read((char*)data, recv_amount) >0)
            && _parser.recv("OK"))) {
        return -2;
    }    
    return recv_amount;
}

bool SPWFSA01::close(int id)
{
    uint32_t recv_amount=0;    
    void * data = NULL;    

    _parser.setTimeout(SPWFSA01_MISC_TIMEOUT);    
    _parser.flush();
    /* socket flush */
    if(!(_parser.send("AT+S.SOCKQ=%d", id)  //send a query (will be required for secure sockets)
        && _parser.recv(" DATALEN: %u", &recv_amount)
        && _parser.recv("OK"))) {
            return -2;
    } 
    if (recv_amount>0) {
        data = malloc (recv_amount+4);
        if(!(_parser.send("AT+S.SOCKR=%d,%d", id, recv_amount))) { 
            free (data); 
            return -2; 
        }
 //       printf ("--->>>Close flushing recv_amount: %d  \n\r",recv_amount);            
        if(!((_parser.read((char*)data, recv_amount) >0)
            && _parser.recv("OK"))) {
             free (data);
             return -2;
        }
        free (data);                            
    }     
    
    //May take a second try if device is busy or error is returned
    for (unsigned i = 0; i < 2; i++) {
        if (_parser.send("AT+S.SOCKC=%d", id)
            && _parser.recv("OK")) {
            socket_closed = 1;     
            return true;
        }
        else
        {
            if(_parser.recv("ERROR: Pending data")) {
                    debug_if(dbg_on, "SPWF> ERROR!!!!\r\n");
                    return false;
            }
        }
        //TODO: Deal with "ERROR: Pending data" (Closing a socket with pending data)
    }
    return false;
}


bool SPWFSA01::readable()
{
    return _serial.readable();
}

bool SPWFSA01::writeable()
{
    return _serial.writeable();
}
