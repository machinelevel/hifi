//
//  AgentList.cpp
//  hifi
//
//  Created by Stephen Birarda on 2/15/13.
//  Copyright (c) 2013 High Fidelity, Inc. All rights reserved.
//

#include <pthread.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#include "AgentList.h"
#include "AgentTypes.h"
#include "PacketHeaders.h"
#include "SharedUtil.h"
#include "Log.h"

#ifdef _WIN32
#include "Syssocket.h"
#else
#include <arpa/inet.h>
#endif

const char SOLO_AGENT_TYPES[3] = {
    AGENT_TYPE_AVATAR_MIXER,
    AGENT_TYPE_AUDIO_MIXER,
    AGENT_TYPE_VOXEL_SERVER
};

char DOMAIN_HOSTNAME[] = "highfidelity.below92.com";
char DOMAIN_IP[100] = "";    //  IP Address will be re-set by lookup on startup
const int DOMAINSERVER_PORT = 40102;

bool silentAgentThreadStopFlag = false;
bool pingUnknownAgentThreadStopFlag = false;

AgentList* AgentList::_sharedInstance = NULL;

AgentList* AgentList::createInstance(char ownerType, unsigned int socketListenPort) {
    if (!_sharedInstance) {
        _sharedInstance = new AgentList(ownerType, socketListenPort);
    } else {
        printLog("AgentList createInstance called with existing instance.\n");
    }
    
    return _sharedInstance;
}

AgentList* AgentList::getInstance() {
    if (!_sharedInstance) {
        printLog("AgentList getInstance called before call to createInstance. Returning NULL pointer.\n");
    }
    
    return _sharedInstance;
}

AgentList::AgentList(char newOwnerType, unsigned int newSocketListenPort) :
    _agentBuckets(),
    _numAgents(0),
    _agentSocket(newSocketListenPort),
    _ownerType(newOwnerType),
    _agentTypesOfInterest(NULL),
    _ownerID(UNKNOWN_AGENT_ID),
    _lastAgentID(0) {
    pthread_mutex_init(&mutex, 0);
}

AgentList::~AgentList() {
    delete _agentTypesOfInterest;
    
    // stop the spawned threads, if they were started
    stopSilentAgentRemovalThread();
    stopPingUnknownAgentsThread();
    
    pthread_mutex_destroy(&mutex);
}

void AgentList::processAgentData(sockaddr *senderAddress, unsigned char *packetData, size_t dataBytes) {
    switch (((char *)packetData)[0]) {
        case PACKET_HEADER_DOMAIN: {
            processDomainServerList(packetData, dataBytes);
            break;
        }
        case PACKET_HEADER_PING: {
            _agentSocket.send(senderAddress, &PACKET_HEADER_PING_REPLY, 1);
            break;
        }
        case PACKET_HEADER_PING_REPLY: {
            handlePingReply(senderAddress);
            break;
        }
    }
}

void AgentList::processBulkAgentData(sockaddr *senderAddress, unsigned char *packetData, int numTotalBytes) {
    lock();
    
    // find the avatar mixer in our agent list and update the lastRecvTime from it
    Agent* bulkSendAgent = agentWithAddress(senderAddress);

    if (bulkSendAgent) {
        bulkSendAgent->setLastHeardMicrostamp(usecTimestampNow());
        bulkSendAgent->recordBytesReceived(numTotalBytes);
    }

    unsigned char *startPosition = packetData;
    unsigned char *currentPosition = startPosition + 1;
    unsigned char packetHolder[numTotalBytes];
    
    packetHolder[0] = PACKET_HEADER_HEAD_DATA;
    
    uint16_t agentID = -1;
    
    while ((currentPosition - startPosition) < numTotalBytes) {
        unpackAgentId(currentPosition, &agentID);
        memcpy(packetHolder + 1, currentPosition, numTotalBytes - (currentPosition - startPosition));
        
        Agent* matchingAgent = agentWithID(agentID);
        
        if (!matchingAgent) {
            // we're missing this agent, we need to add it to the list
            matchingAgent = addOrUpdateAgent(NULL, NULL, AGENT_TYPE_AVATAR, agentID);
        }
        
        currentPosition += updateAgentWithData(matchingAgent,
                                               packetHolder,
                                               numTotalBytes - (currentPosition - startPosition));
    }
    
    unlock();
}

int AgentList::updateAgentWithData(sockaddr *senderAddress, unsigned char *packetData, size_t dataBytes) {
    // find the agent by the sockaddr
    Agent* matchingAgent = agentWithAddress(senderAddress);
    
    if (matchingAgent) {
        return updateAgentWithData(matchingAgent, packetData, dataBytes);
    } else {
        return 0;
    }
}

int AgentList::updateAgentWithData(Agent *agent, unsigned char *packetData, int dataBytes) {
    agent->setLastHeardMicrostamp(usecTimestampNow());
    
    if (agent->getActiveSocket()) {
        agent->recordBytesReceived(dataBytes);
    }
    
    if (!agent->getLinkedData() && linkedDataCreateCallback) {
        linkedDataCreateCallback(agent);
    }
    
    return agent->getLinkedData()->parseData(packetData, dataBytes);
}

Agent* AgentList::agentWithAddress(sockaddr *senderAddress) {
    for(AgentList::iterator agent = begin(); agent != end(); agent++) {
        if (agent->getActiveSocket() && socketMatch(agent->getActiveSocket(), senderAddress)) {
            return &(*agent);
        }
    }
    
    return NULL;
}

Agent* AgentList::agentWithID(uint16_t agentID) {
    for(AgentList::iterator agent = begin(); agent != end(); agent++) {
        if (agent->getAgentID() == agentID) {
            return &(*agent);
        }
    }

    return NULL;
}

void AgentList::setAgentTypesOfInterest(const char* agentTypesOfInterest, int numAgentTypesOfInterest) {
    delete _agentTypesOfInterest;
    
    _agentTypesOfInterest = new char[numAgentTypesOfInterest + sizeof(char)];
    memcpy(_agentTypesOfInterest, agentTypesOfInterest, numAgentTypesOfInterest);
    _agentTypesOfInterest[numAgentTypesOfInterest] = '\0';
}

void AgentList::sendDomainServerCheckIn() {
    static bool printedDomainServerIP = false;
    //  Lookup the IP address of the domain server if we need to
    if (atoi(DOMAIN_IP) == 0) {
        struct hostent* pHostInfo;
        if ((pHostInfo = gethostbyname(DOMAIN_HOSTNAME)) != NULL) {
            sockaddr_in tempAddress;
            memcpy(&tempAddress.sin_addr, pHostInfo->h_addr_list[0], pHostInfo->h_length);
            strcpy(DOMAIN_IP, inet_ntoa(tempAddress.sin_addr));
            printLog("Domain Server: %s \n", DOMAIN_HOSTNAME);
        } else {
            printLog("Failed domain server lookup\n");
        }
    } else if (!printedDomainServerIP) {
        printLog("Domain Server IP: %s\n", DOMAIN_IP);
        printedDomainServerIP = true;
    }
    
    // construct the DS check in packet if we need to
    static unsigned char* checkInPacket = NULL;
    static int checkInPacketSize;

    if (!checkInPacket) {
        int numBytesAgentsOfInterest = _agentTypesOfInterest ? strlen((char*) _agentTypesOfInterest) : 0;
        
        // check in packet has header, agent type, port, IP, agent types of interest, null termination
        int numPacketBytes = sizeof(PACKET_HEADER) + sizeof(AGENT_TYPE) + sizeof(uint16_t) + (sizeof(char) * 4) +
            numBytesAgentsOfInterest + sizeof(unsigned char);
        
        checkInPacket = new unsigned char[numPacketBytes];
        unsigned char* packetPosition = checkInPacket;
        
        *(packetPosition++) = (memchr(SOLO_AGENT_TYPES, _ownerType, sizeof(SOLO_AGENT_TYPES)))
                ? PACKET_HEADER_DOMAIN_REPORT_FOR_DUTY
                : PACKET_HEADER_DOMAIN_LIST_REQUEST;
        *(packetPosition++) = _ownerType;
        
        packetPosition += packSocket(checkInPacket + sizeof(PACKET_HEADER) + sizeof(AGENT_TYPE),
                                     getLocalAddress(),
                                     htons(_agentSocket.getListeningPort()));
        
        // add the number of bytes for agent types of interest
        *(packetPosition++) = numBytesAgentsOfInterest;
                
        // copy over the bytes for agent types of interest, if required
        if (numBytesAgentsOfInterest > 0) {
            memcpy(packetPosition,
                   _agentTypesOfInterest,
                   numBytesAgentsOfInterest);
            packetPosition += numBytesAgentsOfInterest;
        }
        
        checkInPacketSize = packetPosition - checkInPacket;
    }
    
    _agentSocket.send(DOMAIN_IP, DOMAINSERVER_PORT, checkInPacket, checkInPacketSize);
}

int AgentList::processDomainServerList(unsigned char *packetData, size_t dataBytes) {
    int readAgents = 0;

    char agentType;
    uint16_t agentId;
    
    // assumes only IPv4 addresses
    sockaddr_in agentPublicSocket;
    agentPublicSocket.sin_family = AF_INET;
    sockaddr_in agentLocalSocket;
    agentLocalSocket.sin_family = AF_INET;
    
    unsigned char *readPtr = packetData + 1;
    unsigned char *startPtr = packetData;
    
    while((readPtr - startPtr) < dataBytes - sizeof(uint16_t)) {
        agentType = *readPtr++;
        readPtr += unpackAgentId(readPtr, (uint16_t *)&agentId);
        readPtr += unpackSocket(readPtr, (sockaddr *)&agentPublicSocket);
        readPtr += unpackSocket(readPtr, (sockaddr *)&agentLocalSocket);
        
        addOrUpdateAgent((sockaddr *)&agentPublicSocket, (sockaddr *)&agentLocalSocket, agentType, agentId);
    }
    
    // read out our ID from the packet
    unpackAgentId(readPtr, &_ownerID);

    return readAgents;
}

Agent* AgentList::addOrUpdateAgent(sockaddr* publicSocket, sockaddr* localSocket, char agentType, uint16_t agentId) {
    AgentList::iterator agent = end();
    
    if (publicSocket) {
        for (agent = begin(); agent != end(); agent++) {
            if (agent->matches(publicSocket, localSocket, agentType)) {
                // we already have this agent, stop checking
                break;
            }
        }
    } 
    
    if (agent == end()) {
        // we didn't have this agent, so add them
        Agent* newAgent = new Agent(publicSocket, localSocket, agentType, agentId);
        
        if (socketMatch(publicSocket, localSocket)) {
            // likely debugging scenario with two agents on local network
            // set the agent active right away
            newAgent->activatePublicSocket();
        }
   
        if (newAgent->getType() == AGENT_TYPE_VOXEL_SERVER ||
            newAgent->getType() == AGENT_TYPE_AVATAR_MIXER ||
            newAgent->getType() == AGENT_TYPE_AUDIO_MIXER) {
            // this is currently the cheat we use to talk directly to our test servers on EC2
            // to be removed when we have a proper identification strategy
            newAgent->activatePublicSocket();
        }
        
        addAgentToList(newAgent);
        
        return newAgent;
    } else {
        
        if (agent->getType() == AGENT_TYPE_AUDIO_MIXER ||
            agent->getType() == AGENT_TYPE_VOXEL_SERVER) {
            // until the Audio class also uses our agentList, we need to update
            // the lastRecvTimeUsecs for the audio mixer so it doesn't get killed and re-added continously
            agent->setLastHeardMicrostamp(usecTimestampNow());
        }
        
        // we had this agent already, do nothing for now
        return &*agent;
    }    
}

void AgentList::addAgentToList(Agent* newAgent) {
    // find the correct array to add this agent to
    int bucketIndex = _numAgents / AGENTS_PER_BUCKET;
    
    if (!_agentBuckets[bucketIndex]) {
        _agentBuckets[bucketIndex] = new Agent*[AGENTS_PER_BUCKET]();
    }
    
    _agentBuckets[bucketIndex][_numAgents % AGENTS_PER_BUCKET] = newAgent;
    
    ++_numAgents;
    
    printLog("Added ");
    Agent::printLog(*newAgent);
}

void AgentList::broadcastToAgents(unsigned char *broadcastData, size_t dataBytes, const char* agentTypes, int numAgentTypes) {
    for(AgentList::iterator agent = begin(); agent != end(); agent++) {
        // only send to the AgentTypes we are asked to send to.
        if (agent->getActiveSocket() != NULL && memchr(agentTypes, agent->getType(), numAgentTypes)) {
            // we know which socket is good for this agent, send there
            _agentSocket.send(agent->getActiveSocket(), broadcastData, dataBytes);
        }
    }
}

void AgentList::handlePingReply(sockaddr *agentAddress) {
    for(AgentList::iterator agent = begin(); agent != end(); agent++) {
        // check both the public and local addresses for each agent to see if we find a match
        // prioritize the private address so that we prune erroneous local matches
        if (socketMatch(agent->getPublicSocket(), agentAddress)) {
            agent->activatePublicSocket();
            break;
        } else if (socketMatch(agent->getLocalSocket(), agentAddress)) {
            agent->activateLocalSocket();
            break;
        }
    }
}

Agent* AgentList::soloAgentOfType(char agentType) {
    if (memchr(SOLO_AGENT_TYPES, agentType, sizeof(SOLO_AGENT_TYPES)) != NULL) {
        for(AgentList::iterator agent = begin(); agent != end(); agent++) {
            if (agent->getType() == agentType) {
                return &(*agent);
            }
        }
    }
    
    return NULL;
}

void *pingUnknownAgents(void *args) {
    
    AgentList* agentList = (AgentList*) args;
    const int PING_INTERVAL_USECS = 1 * 1000000;
    
    timeval lastSend;
    
    while (!pingUnknownAgentThreadStopFlag) {
        gettimeofday(&lastSend, NULL);
        
        for(AgentList::iterator agent = agentList->begin();
            agent != agentList->end();
            agent++) {
            if (!agent->getActiveSocket() && agent->getPublicSocket() && agent->getLocalSocket()) {
                // ping both of the sockets for the agent so we can figure out
                // which socket we can use
                agentList->getAgentSocket()->send(agent->getPublicSocket(), &PACKET_HEADER_PING, 1);
                agentList->getAgentSocket()->send(agent->getLocalSocket(), &PACKET_HEADER_PING, 1);
            }
        }
        
        long long usecToSleep = PING_INTERVAL_USECS - (usecTimestampNow() - usecTimestamp(&lastSend));
        
        if (usecToSleep > 0) {
            usleep(usecToSleep);
        }
    }
    
    return NULL;
}

void AgentList::startPingUnknownAgentsThread() {
    pthread_create(&pingUnknownAgentsThread, NULL, pingUnknownAgents, (void *)this);
}

void AgentList::stopPingUnknownAgentsThread() {
    pingUnknownAgentThreadStopFlag = true;
    pthread_join(pingUnknownAgentsThread, NULL);
}

void *removeSilentAgents(void *args) {
    AgentList* agentList = (AgentList*) args;
    long long checkTimeUSecs, sleepTime;
    
    while (!silentAgentThreadStopFlag) {
        checkTimeUSecs = usecTimestampNow();
        
        for(AgentList::iterator agent = agentList->begin(); agent != agentList->end(); ++agent) {
            
            if ((checkTimeUSecs - agent->getLastHeardMicrostamp()) > AGENT_SILENCE_THRESHOLD_USECS
            	&& agent->getType() != AGENT_TYPE_VOXEL_SERVER) {
            
                printLog("Killed ");
                Agent::printLog(*agent);
                
                agent->setAlive(false);
            }
        }
        
        sleepTime = AGENT_SILENCE_THRESHOLD_USECS - (usecTimestampNow() - checkTimeUSecs);
        #ifdef _WIN32
        Sleep( static_cast<int>(1000.0f*sleepTime) );
        #else
        usleep(sleepTime);
        #endif
    }
    
    pthread_exit(0);
    return NULL;
}

void AgentList::startSilentAgentRemovalThread() {
    pthread_create(&removeSilentAgentsThread, NULL, removeSilentAgents, (void*) this);
}

void AgentList::stopSilentAgentRemovalThread() {
    silentAgentThreadStopFlag = true;
    pthread_join(removeSilentAgentsThread, NULL);
}

AgentList::iterator AgentList::begin() const {
    Agent** agentBucket = NULL;
    
    for (int i = 0; i < _numAgents; i++) {
        if (i % AGENTS_PER_BUCKET == 0) {
            agentBucket =  _agentBuckets[i / AGENTS_PER_BUCKET];
        }
        
        if (agentBucket[i % AGENTS_PER_BUCKET]->isAlive()) {
            return AgentListIterator(this, i);
        }
    }
    
    // there's no alive agent to start from - return the end
    return end();
}

AgentList::iterator AgentList::end() const {
    return AgentListIterator(this, _numAgents);
}

AgentListIterator::AgentListIterator(const AgentList* agentList, int agentIndex) :
    _agentIndex(agentIndex) {
    _agentList = agentList;
}

AgentListIterator& AgentListIterator::operator=(const AgentListIterator& otherValue) {
    _agentList = otherValue._agentList;
    _agentIndex = otherValue._agentIndex;
    return *this;
}

bool AgentListIterator::operator==(const AgentListIterator &otherValue) {
    return _agentIndex == otherValue._agentIndex;
}

bool AgentListIterator::operator!=(const AgentListIterator &otherValue) {
    return !(*this == otherValue);
}

Agent& AgentListIterator::operator*() {
    Agent** agentBucket = _agentList->_agentBuckets[_agentIndex / AGENTS_PER_BUCKET];
    return *agentBucket[_agentIndex % AGENTS_PER_BUCKET];
}

Agent* AgentListIterator::operator->() {
    Agent** agentBucket = _agentList->_agentBuckets[_agentIndex / AGENTS_PER_BUCKET];
    return agentBucket[_agentIndex % AGENTS_PER_BUCKET];
}

AgentListIterator& AgentListIterator::operator++() {
    skipDeadAndStopIncrement();
    return *this;
}

AgentList::iterator AgentListIterator::operator++(int) {
    AgentListIterator newIterator = AgentListIterator(*this);
    skipDeadAndStopIncrement();
    return newIterator;
}

void AgentListIterator::skipDeadAndStopIncrement() {
    while (_agentIndex != _agentList->_numAgents) {
        ++_agentIndex;
        
        if (_agentIndex == _agentList->_numAgents) {
            break;
        } else if ((*(*this)).isAlive()) {
            // skip over the dead agents
            break;
        }
    }
}
