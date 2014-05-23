/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2004, WWIV Software Services             */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <signal.h>

#include "../wwiv.h"

using namespace std;

#define MAX_NODES 500
#define BBSHOME "/home/bbs/"

configrec cfgRec;
configoverrec *cfgOverlayRec = NULL;
int maxNodes = 1;
int usedNodes = 0;

void loadConfigDat();
void loadNodeData();
void loadUsedNodeData();

void launchNode(int nodeNumber);
void huphandler(int mysignal);
void huphandler2(int mysignal);

/**
 *  This program is the manager of the nodes for the WWIV BBS software
 *  on UNIX platforms.
 */
int main(int argc, char *argv[])
{
  bool foundNode = false;
  int newNodeNumber = 1;
  chdir(BBSHOME); // hardcoded home dir of BBS. config.dat/config.ovr should be here.
  loadConfigDat();
  loadNodeData();
  loadUsedNodeData();

  signal (SIGQUIT, SIG_DFL);
  signal (SIGTERM, SIG_DFL);
  signal (SIGALRM, SIG_DFL);
  signal (SIGINT, SIG_DFL); 
  signal (SIGHUP, huphandler);

  if(maxNodes == 0)
  {
    maxNodes = 1;
  }

  printf("WWIV 5.0 UNIX Node Manager Bootstrap.\n");
  printf("Please wait while node data is parsed.\n");
  printf("Found %u/%u Nodes in use.\n", usedNodes, maxNodes);

  if(usedNodes == maxNodes)
  {
    printf("There are no available nodes.  Please try again later.\n");
  }
  else if(usedNodes == 0)
  {
    launchNode(1);
  }
  else
  {
    // Find open node number.
    //
    for(newNodeNumber = 1; newNodeNumber <= maxNodes && !foundNode; newNodeNumber++)
    {
      char nodeFile[256];
      sprintf((char *)nodeFile, "%snodeinuse.%u", cfgRec.datadir, newNodeNumber);
      struct stat buf;
      if(stat(nodeFile, &buf))
      {
        foundNode = true;
        launchNode(newNodeNumber);
      }
    }
  }
  
  if(cfgOverlayRec != NULL)
  {
    delete [] cfgOverlayRec;
  }
  return(0);
}

void loadConfigDat()
{
  FILE *fp = fopen(CONFIG_DAT, "rb");
  if(fp == NULL)
  {
    fprintf(stderr, "%s not found!  BBS not initialized.\n", CONFIG_DAT);
    return;
  }
  fread(&cfgRec, 1, sizeof(configrec), fp);
  fclose(fp);  
}

void loadNodeData()
{
  FILE *fp = fopen(CONFIG_OVR, "rb");
  if(fp != NULL)
  {
    fseek(fp, 0L, SEEK_END);
    unsigned long len = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    maxNodes = len/sizeof(configoverrec);
    cfgOverlayRec = new configoverrec[maxNodes];
    fread(cfgOverlayRec, maxNodes, sizeof(configoverrec), fp);
    fclose(fp);
  }
  else
  {
    printf("There was an error reading the node data files.\n");
    printf("Please report this to the sysop.\n");
    exit(-1);
  }
}

void loadUsedNodeData()
{
  for(int counter = 1; counter <= maxNodes; counter++)
  {
    struct stat buf;
    char nodeFile[256];
    sprintf((char *)nodeFile, "%snodeinuse.%u", cfgRec.datadir, counter);
    if(!stat(nodeFile, &buf))
    {
      usedNodes++;
    }
  }
}

void launchNode(int nodeNumber)
{
  FILE *fp;
  char nodeFile[256];
  sprintf((char *)nodeFile, "%snodeinuse.%u", cfgRec.datadir, nodeNumber);
  fp = fopen(nodeFile, "wb");
  fclose(fp);
  char sysCmd[512];
  sprintf((char *)sysCmd, "./wwiv /N%u /I%u", nodeNumber, nodeNumber);

  printf("Invoking WWIV with cmd line:\n%s\n", sysCmd);
  system(sysCmd);
  unlink(nodeFile);
}

void huphandler(int mysignal) {
	signal (SIGHUP, huphandler2); // catch the SIGHUP we send below
	printf("\nSending SIGHUP to BBS after receiving %d...\n", mysignal);
	kill(0,SIGHUP); // send SIGHUP to process group
}

void huphandler2(int mysignal) {
	signal (SIGHUP, SIG_DFL); // reset to default handler
	printf("\nWaiting for BBS to die after signal %d...\n", mysignal);
}

