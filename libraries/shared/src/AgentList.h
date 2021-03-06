//
//  AgentList.h
//  hifi
//
//  Created by Stephen Birarda on 2/15/13.
//  Copyright (c) 2013 High Fidelity, Inc. All rights reserved.
//

#ifndef __hifi__AgentList__
#define __hifi__AgentList__

#include <stdint.h>
#include <iterator>

#include "Agent.h"
#include "UDPSocket.h"

#ifdef _WIN32
#include "pthread.h"
#endif

const int MAX_NUM_AGENTS = 10000;
const int AGENTS_PER_BUCKET = 100;

const int MAX_PACKET_SIZE = 1500;
const unsigned int AGENT_SOCKET_LISTEN_PORT = 40103;

const int AGENT_SILENCE_THRESHOLD_USECS = 2 * 1000000;
const int DOMAIN_SERVER_CHECK_IN_USECS = 1 * 1000000;

extern const char SOLO_AGENT_TYPES[3];

extern char DOMAIN_HOSTNAME[];
extern char DOMAIN_IP[100];    //  IP Address will be re-set by lookup on startup
extern const int DOMAINSERVER_PORT;

const int UNKNOWN_AGENT_ID = -1;

class AgentListIterator;

class AgentList {
public:
    static AgentList* createInstance(char ownerType, unsigned int socketListenPort = AGENT_SOCKET_LISTEN_PORT);
    static AgentList* getInstance();
    
    typedef AgentListIterator iterator;
  
    AgentListIterator begin() const;
    AgentListIterator end() const;
    
    char getOwnerType() const { return _ownerType; }
    
    uint16_t getLastAgentID() const { return _lastAgentID; }
    void increaseAgentID() { ++_lastAgentID; }
    
    uint16_t getOwnerID() const { return _ownerID; }
    void setOwnerID(uint16_t ownerID) { _ownerID = ownerID; }
    
    UDPSocket* getAgentSocket() { return &_agentSocket; }
    
    unsigned int getSocketListenPort() const { return _agentSocket.getListeningPort(); };
    
    void(*linkedDataCreateCallback)(Agent *);
    
    int size() { return _numAgents; }
    
    void lock() { pthread_mutex_lock(&mutex); }
    void unlock() { pthread_mutex_unlock(&mutex); }
    
    void setAgentTypesOfInterest(const char* agentTypesOfInterest, int numAgentTypesOfInterest);
    void sendDomainServerCheckIn();
    int processDomainServerList(unsigned char *packetData, size_t dataBytes);
    
    Agent* agentWithAddress(sockaddr *senderAddress);
    Agent* agentWithID(uint16_t agentID);
    
    Agent* addOrUpdateAgent(sockaddr* publicSocket, sockaddr* localSocket, char agentType, uint16_t agentId);
    
    void processAgentData(sockaddr *senderAddress, unsigned char *packetData, size_t dataBytes);
    void processBulkAgentData(sockaddr *senderAddress, unsigned char *packetData, int numTotalBytes);
   
    int updateAgentWithData(sockaddr *senderAddress, unsigned char *packetData, size_t dataBytes);
    int updateAgentWithData(Agent *agent, unsigned char *packetData, int dataBytes);
    
    void broadcastToAgents(unsigned char *broadcastData, size_t dataBytes, const char* agentTypes, int numAgentTypes);
    
    Agent* soloAgentOfType(char agentType);
    
    void startSilentAgentRemovalThread();
    void stopSilentAgentRemovalThread();
    void startPingUnknownAgentsThread();
    void stopPingUnknownAgentsThread();
    
    friend class AgentListIterator;
private:
    static AgentList* _sharedInstance;
    
    AgentList(char ownerType, unsigned int socketListenPort);
    ~AgentList();
    AgentList(AgentList const&); // Don't implement, needed to avoid copies of singleton
    void operator=(AgentList const&); // Don't implement, needed to avoid copies of singleton
    
    void addAgentToList(Agent* newAgent);
    
    Agent** _agentBuckets[MAX_NUM_AGENTS / AGENTS_PER_BUCKET];
    int _numAgents;
    UDPSocket _agentSocket;
    char _ownerType;
    char* _agentTypesOfInterest;
    unsigned int _socketListenPort;
    uint16_t _ownerID;
    uint16_t _lastAgentID;
    pthread_t removeSilentAgentsThread;
    pthread_t checkInWithDomainServerThread;
    pthread_t pingUnknownAgentsThread;
    pthread_mutex_t mutex;
    
    void handlePingReply(sockaddr *agentAddress);
};

class AgentListIterator : public std::iterator<std::input_iterator_tag, Agent> {
public:
    AgentListIterator(const AgentList* agentList, int agentIndex);
    ~AgentListIterator() {};
    
    int getAgentIndex() { return _agentIndex; };
    
	AgentListIterator& operator=(const AgentListIterator& otherValue);
    
    bool operator==(const AgentListIterator& otherValue);
	bool operator!= (const AgentListIterator& otherValue);
    
    Agent& operator*();
    Agent* operator->();
    
	AgentListIterator& operator++();
    AgentListIterator operator++(int);
private:
    void skipDeadAndStopIncrement();
    
    const AgentList* _agentList;
    int _agentIndex;
};

#endif /* defined(__hifi__AgentList__) */
