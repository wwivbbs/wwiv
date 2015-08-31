/*
 * Copyright 2001,2004 Frank Reid
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */ 
#if !defined( __NETWORK_H_INCLUDED__ )
#define __NETWORK_H_INCLUDED__ 


#define INI_NETWORK   0x01
#define INI_GENERAL   0x02
#define INI_NEWS      0x04

typedef struct 
{
	unsigned short sysnum;
	char address[60];
} net_address_rec;

const char *version	= "WWIV Internet Network Support (WINS) " VERSION " [" __DATE__ "]";
const char *version_url	= "Please visit http://wwivbbs.com/ and https://github.com/wwivbbs/wwiv for support and more information.";

const char *strings[] = 
{
	"Unable to create",
	"Unable to read",
	"Unknown error",
	"Unable to write CONTACT.NET!",
	"Unable to write NET.LOG!",
	0L,
};


//
// Global Structures
//
configrec syscfg;
configoverrec syscfgovr;
net_networks_rec netcfg;
net_system_list_rec *netsys;

//
// Global Variables
//
int g_nNumAddresses;
char maindir[MAX_PATH];
char net_name[16], net_data[_MAX_PATH];
unsigned short g_nNetworkSystemNumber;

#ifdef DOMAIN
//TODO(rushfan): rename DOMAIN since math.h now defines this.
#undef DOMAIN
#endif  // DOMAIN

//
// NET.INI directive values.
//
char SMTPHOST[40], POPHOST[40], NEWSHOST[40], POPNAME[40], POPPASS[40], PROXY[40], QOTDFILE[121];
char FWDNAME[20], FWDDOM[40], TIMEHOST[40], QOTDHOST[40];
char PRIMENET[25], NODEPASS[20], DOMAIN[40], temp_dir[MAX_PATH];
unsigned int KEEPSENT, CLEANUP, NOLISTNAMES, MULTINEWS;
unsigned int BBSNODE;
unsigned int POPPORT, SMTPPORT;
bool ALLMAIL, PURGE, ONECALL, RASDIAL, MOREINFO;


#endif	// __NETWORK_H_INCLUDED__ 
