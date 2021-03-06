//
//  main.cpp
//  Domain Server 
//
//  Created by Philip Rosedale on 11/20/12.
//  Copyright (c) 2012 High Fidelity, Inc. All rights reserved.
//
//  The Domain Server keeps a list of agents that have connected to it, and echoes that list of
//  agents out to agents when they check in.
//
//  The connection is stateless... the domain server will set you inactive if it does not hear from
//  you in LOGOFF_CHECK_INTERVAL milliseconds, meaning your info will not be sent to other users.
//
//  Each packet from an agent has as first character the type of server:
//
//  I - Interactive Agent
//  M - Audio Mixer
//

#include <iostream>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <map>
#include "AgentList.h"
#include "AgentTypes.h"
#include <PacketHeaders.h>
#include "SharedUtil.h"

#ifdef _WIN32
#include "Syssocket.h"
#include "Systime.h"
#else
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif


const int DOMAIN_LISTEN_PORT = 40102;
unsigned char packetData[MAX_PACKET_SIZE];

const int LOGOFF_CHECK_INTERVAL = 5000;

int lastActiveCount = 0;

unsigned char* addAgentToBroadcastPacket(unsigned char* currentPosition, Agent* agentToAdd) {
    *currentPosition++ = agentToAdd->getType();
    
    currentPosition += packAgentId(currentPosition, agentToAdd->getAgentID());
    currentPosition += packSocket(currentPosition, agentToAdd->getPublicSocket());
    currentPosition += packSocket(currentPosition, agentToAdd->getLocalSocket());
    
    // return the new unsigned char * for broadcast packet
    return currentPosition;
}

int main(int argc, const char * argv[])
{
    AgentList* agentList = AgentList::createInstance(AGENT_TYPE_DOMAIN, DOMAIN_LISTEN_PORT);
	// If user asks to run in "local" mode then we do NOT replace the IP
	// with the EC2 IP. Otherwise, we will replace the IP like we used to
	// this allows developers to run a local domain without recompiling the
	// domain server
	bool isLocalMode = cmdOptionExists(argc, argv, "--local");
	if (isLocalMode) {
		printf("NOTE: Running in Local Mode!\n");
	} else {
		printf("--------------------------------------------------\n");
		printf("NOTE: Running in EC2 Mode. \n");
		printf("If you're a developer testing a local system, you\n");
		printf("probably want to include --local on command line.\n");
		printf("--------------------------------------------------\n");
	}

    setvbuf(stdout, NULL, _IOLBF, 0);
    
    ssize_t receivedBytes = 0;
    char agentType = '\0';
    
    unsigned char broadcastPacket[MAX_PACKET_SIZE];
    broadcastPacket[0] = PACKET_HEADER_DOMAIN;
    
    unsigned char* currentBufferPos;
    unsigned char* startPointer;
    
    sockaddr_in agentPublicAddress, agentLocalAddress;
    agentLocalAddress.sin_family = AF_INET;
    
    in_addr_t serverLocalAddress = getLocalAddress();
    
    agentList->startSilentAgentRemovalThread();
    
    while (true) {
        if (agentList->getAgentSocket()->receive((sockaddr *)&agentPublicAddress, packetData, &receivedBytes) &&
            (packetData[0] == PACKET_HEADER_DOMAIN_REPORT_FOR_DUTY || packetData[0] == PACKET_HEADER_DOMAIN_LIST_REQUEST)) {
            std::map<char, Agent *> newestSoloAgents;
            
            agentType = packetData[1];
            int numBytesSocket = unpackSocket(packetData + sizeof(PACKET_HEADER) + sizeof(AGENT_TYPE),
                                              (sockaddr*) &agentLocalAddress);
            
            sockaddr* destinationSocket = (sockaddr*) &agentPublicAddress;
            
            // check the agent public address
            // if it matches our local address we're on the same box
            // so hardcode the EC2 public address for now
            if (agentPublicAddress.sin_addr.s_addr == serverLocalAddress) {
            	// If we're not running "local" then we do replace the IP
            	// with the EC2 IP. Otherwise, we use our normal public IP
            	if (!isLocalMode) {
	                agentPublicAddress.sin_addr.s_addr = 895283510; // local IP in this format...
                    destinationSocket = (sockaddr*) &agentLocalAddress;
	            }
            }
            
            Agent* newAgent = agentList->addOrUpdateAgent((sockaddr*) &agentPublicAddress,
                                                          (sockaddr*) &agentLocalAddress,
                                                          agentType,
                                                          agentList->getLastAgentID());
            
            if (newAgent->getAgentID() == agentList->getLastAgentID()) {
                agentList->increaseAgentID();
            }
            
            currentBufferPos = broadcastPacket + sizeof(PACKET_HEADER);
            startPointer = currentBufferPos;
            
            unsigned char* agentTypesOfInterest = packetData + sizeof(PACKET_HEADER) + sizeof(AGENT_TYPE)
                + numBytesSocket + sizeof(unsigned char);
            int numInterestTypes = *(agentTypesOfInterest - 1);
            
            if (numInterestTypes > 0) {
                // if the agent has sent no types of interest, assume they want nothing but their own ID back
                for (AgentList::iterator agent = agentList->begin(); agent != agentList->end(); agent++) {
                    if (!agent->matches((sockaddr*) &agentPublicAddress, (sockaddr*) &agentLocalAddress, agentType) &&
                            memchr(agentTypesOfInterest, agent->getType(), numInterestTypes)) {
                        // this is not the agent themselves
                        // and this is an agent of a type in the passed agent types of interest
                        // or the agent did not pass us any specific types they are interested in
                    
                        if (memchr(SOLO_AGENT_TYPES, agent->getType(), sizeof(SOLO_AGENT_TYPES)) == NULL) {
                            // this is an agent of which there can be multiple, just add them to the packet
                            // don't send avatar agents to other avatars, that will come from avatar mixer
                            if (agentType != AGENT_TYPE_AVATAR || agent->getType() != AGENT_TYPE_AVATAR) {
                                currentBufferPos = addAgentToBroadcastPacket(currentBufferPos, &(*agent));
                            }
                        
                        } else {
                            // solo agent, we need to only send newest
                            if (newestSoloAgents[agent->getType()] == NULL ||
                                newestSoloAgents[agent->getType()]->getWakeMicrostamp() < agent->getWakeMicrostamp()) {
                                // we have to set the newer solo agent to add it to the broadcast later
                                newestSoloAgents[agent->getType()] = &(*agent);
                            }
                        }
                    }
                }
                
                for (std::map<char, Agent *>::iterator soloAgent = newestSoloAgents.begin();
                     soloAgent != newestSoloAgents.end();
                     soloAgent++) {
                    // this is the newest alive solo agent, add them to the packet
                    currentBufferPos = addAgentToBroadcastPacket(currentBufferPos, soloAgent->second);
                }
            }
                        
            // update last receive to now
            long long timeNow = usecTimestampNow();
            newAgent->setLastHeardMicrostamp(timeNow);
            
            if (packetData[0] == PACKET_HEADER_DOMAIN_REPORT_FOR_DUTY
                && memchr(SOLO_AGENT_TYPES, agentType, sizeof(SOLO_AGENT_TYPES))) {
                newAgent->setWakeMicrostamp(timeNow);
            }
            
            // add the agent ID to the end of the pointer
            currentBufferPos += packAgentId(currentBufferPos, newAgent->getAgentID());
            
            // send the constructed list back to this agent
            agentList->getAgentSocket()->send(destinationSocket,
                                              broadcastPacket,
                                              (currentBufferPos - startPointer) + 1);
        }
    }

    return 0;
}

