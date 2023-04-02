#include <iostream>
#include <iomanip>
#include <string>
#include <set>
#include <cstdlib>

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <memory.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>

#include <vector>

#define serverBufferSize 80

bool debugMode = false;

const char clientHelpText[] =
    "|---------------------------------------------------|\n"
    "|====================== Help =======================|\n"
    "|    -h - prints out program arg contents           |\n"
    "|    -p - defines port number other then defualt    |\n"
    "|         9838                                      |\n"
    "|    -l - enables printing debug statements         |\n"
    "|---------------------------------------------------|\n";

const char defualtPortName[] = "9838";
const char defualtAddrName[] = "127.0.0.1";

struct argHolder
{

    argHolder() = default;

    bool helpArg = false;
    bool redefinePort = false;

    char *newPortAddress = nullptr;
};

struct serverInfo
{
    serverInfo() = default;

    char *portUsed = nullptr;

    // infomation for communicating to server
    struct addrinfo basedInfo;
    struct addrinfo *serverAddrInfo = nullptr;
    struct addrinfo *addList = nullptr;

    // storage for address regardless of protocal
    struct sockaddr_storage connectionAddr;

    // connector address in string format
    char connectionAddressString[INET6_ADDRSTRLEN];

    // socket address length
    socklen_t addressLength = sizeof(struct sockaddr_storage);

    // file descriptors

    int listenSock = 0;
    int connectionSock = 0;
};

inline int handleArgs(int argcInput, char *argvInput[], struct argHolder *argHolderInput)
{

    int argHandlerRetVal = 0;
    int optRetVal = 0;

    while ((optRetVal = getopt(argcInput, argvInput, "hld:p:")) != -1)
    {

        switch (optRetVal)
        {
        case 'h':

            argHolderInput->helpArg = true;

            break;
        case 'l':

            debugMode = true;

            break;
        case 'p':

            argHolderInput->redefinePort = true;

            argHolderInput->newPortAddress = optarg;

            break;
        default:
            // if any defualt case occurs it is an error so set return value to one end loop and return
            argHandlerRetVal = 1;

            std::cerr << "argHandler() - ran into defualt case: " << std::endl;

            break;
        }
    }

    return argHandlerRetVal;
}

inline int processArgs(int argcInput, char *argvInput[], struct argHolder *argHolderInput)
{

    int argProcessRetVal = 0;

    size_t defualtPortLength = sizeof(defualtPortName);

    if ((argProcessRetVal = handleArgs(argcInput, argvInput, argHolderInput)))
    {

        std::cerr << "processArgs() - problem with HandleArgs(): Line: " << (__LINE__ - 3) << std::endl;
    }

    // any work that needs to be done on the args in done past this point

    if (!argProcessRetVal)
    {

        if (argHolderInput->helpArg)
        {

            std::cerr << clientHelpText << std::endl;

            argProcessRetVal = -2;
        }
    }

    if (!argProcessRetVal)
    {

        if (!argHolderInput->redefinePort)
        {

            argHolderInput->newPortAddress = new char(defualtPortLength);

            if (argHolderInput->newPortAddress == nullptr)
            {

                std::cerr << "processArgs() - failure in memory allocation: Line: " << (__LINE__ - 5) << std::endl;

                argProcessRetVal = 1;
            }
            else
            {

                strncpy(argHolderInput->newPortAddress, defualtPortName, defualtPortLength);
            }
        }
    }

    return argProcessRetVal;
}

inline void *getInAddr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

//returns portnumber from sockaddr struct
inline in_port_t getInPort(struct sockaddr *sa)
{

    if (sa->sa_family == AF_INET)
    {
        return (((struct sockaddr_in *)sa)->sin_port);
    }

    return (((struct sockaddr_in6 *)sa)->sin6_port);
}

inline int setUpServer(struct argHolder *argHolderInput, struct serverInfo *serverInfoInput)
{

    int serverInitRetVal = 0;

    int getAddressReturnVal = 0;

    const int optionalValue = 1;

    // move port data over
    serverInfoInput->portUsed = argHolderInput->newPortAddress;

    memset(&serverInfoInput->connectionAddr, 0, sizeof(struct sockaddr_storage));
    memset(&serverInfoInput->basedInfo, 0, sizeof(struct addrinfo));

    // set socket for tcp and dont care about ip version
    serverInfoInput->basedInfo.ai_family = AF_UNSPEC;
    serverInfoInput->basedInfo.ai_socktype = SOCK_STREAM;
    serverInfoInput->basedInfo.ai_flags = AI_PASSIVE;

    if (debugMode)
    {

        std::cout << "server using port: " << serverInfoInput->portUsed << std::endl;
    }

    if ((getAddressReturnVal = getaddrinfo(NULL, serverInfoInput->portUsed, &serverInfoInput->basedInfo, &serverInfoInput->serverAddrInfo)) != 0)
    {
        std::cerr << "setUpServer() - problem with getaddrinfo(): Line: " << (__LINE__ - 2) << " | return code: " << gai_strerror(getAddressReturnVal) << std::endl;
        serverInitRetVal = 1;
    }

    //create the socket that will be used for listening and then bind to it
    if (!serverInitRetVal)
    {
        for (serverInfoInput->addList = serverInfoInput->serverAddrInfo; serverInfoInput->addList != NULL; serverInfoInput->addList = serverInfoInput->addList->ai_next)
        {
            if ((serverInfoInput->listenSock = socket(serverInfoInput->addList->ai_family, serverInfoInput->addList->ai_socktype, serverInfoInput->addList->ai_protocol)) == -1)
            {
                continue;
            }

            if (setsockopt(serverInfoInput->listenSock, SOL_SOCKET, SO_REUSEADDR, &optionalValue, sizeof(int)) == -1)
            {
                perror("setUpServer() - problem with setsockopt(): ");
                serverInitRetVal = 2;
                break;
            }

            if (bind(serverInfoInput->listenSock, serverInfoInput->addList->ai_addr, serverInfoInput->addList->ai_addrlen) == -1)
            {
                close(serverInfoInput->listenSock);
                perror("setUpServer() - problem with bind(): ");
                continue;
            }

            break;
        }
    }

    if (!serverInitRetVal)
    {
        if (!serverInfoInput->addList)
        {
            std::cerr << "setUpServer() - failed to bind server address" << std::endl;
            serverInitRetVal = 2;
        }
    }

    if (serverInitRetVal != 1)
    {

        freeaddrinfo(serverInfoInput->serverAddrInfo);
    }

    return serverInitRetVal;
}

inline int stringInterpret(char *bufferInput, int bufferLengthInput, int streamCurrentPosInput)
{

    int truBufferLengthRetVal = -1;

    char overCommand[] = "OVER";

    // iterate through bufferInput starting at the new stream position
    // looking for the first character of overCommand
    // if it finds it then and uses strncmp to check all four character if they are the same as overCommand
    // then return true string length before OVER
    for (size_t i = streamCurrentPosInput; i < (size_t)bufferLengthInput && bufferInput[i] != 0; i++)
    {

        if (bufferInput[i] == overCommand[0])
        {

            if (!strncmp((bufferInput + i), overCommand, 4))
            {
                
                if(i){

                    truBufferLengthRetVal = (i - 1);

                }else{
                    //covers case where negative one would be returned
                    //if OVER was the first word in the string
                    truBufferLengthRetVal = 0;

                }
                 
                

                

                break;
            }
        }
    }

    

    return truBufferLengthRetVal;
}

inline size_t numericalCount(int intInput)
{

    size_t countRetVal = 0;

    bool loopStatus = true;

    while (loopStatus)
    {

        if ((intInput /= 10) == 0)
        {

            loopStatus = false;
        }

        countRetVal++;
    }

    return countRetVal;
}

//creates the string that will be sent to connected client
inline int sendStringFormat(char *stringInput, size_t stringLength, int streamLengthInput)
{

    int formatRetVal = 0;

    size_t numericalLength = numericalCount(streamLengthInput);

    const char prefixString[] = "Line length: ";

    const char commandSuffix[] = "OVER";

    memset(stringInput, 0, stringLength);

    sprintf(stringInput, "%s%d %s", prefixString, streamLengthInput, commandSuffix);

    formatRetVal = sizeof(prefixString) + (numericalLength + 1) + sizeof(commandSuffix);

    return formatRetVal;
}

inline int serverTalkLoop(struct serverInfo *serverInfoInput)
{

    int mainServerTalkRetVal = 0;

    // sending Buffer variables

    int bytesSent = 0;

    int bytesSend = 0;

    char sendBuffer[serverBufferSize];

    // varaibles for intermediate buffer

    ssize_t recvBytes = 0;

    char recvBuffer[serverBufferSize];

    // variable related to the main string buffer

    int trueSteamLength = 0;

    int currentStreamPos = 0;

    int phraseBufferCurrentLength = serverBufferSize;

    char *phraseBuffer = (char *)calloc(1, serverBufferSize);

    if (phraseBuffer == NULL)
    {

        perror("serverTalkLoop() - problem with calloc(): ");
        mainServerTalkRetVal = 1;
    }

    if (!mainServerTalkRetVal)
    {
        memset(recvBuffer, 0, sizeof(recvBuffer));
        memset(sendBuffer, 0, sizeof(sendBuffer));
    }

    // main routine loop

    while (!mainServerTalkRetVal)
    {

        if ((recvBytes = recv(serverInfoInput->connectionSock, recvBuffer, serverBufferSize, 0)) == -1)
        {

            perror("serverTalkLoop() - problem with recv(): ");
            mainServerTalkRetVal = 1;
            break;
        }
        else if (!recvBytes)
        {//handles client disconnect

            std::cout << "Client closed its connection." << std::endl;

            if(debugMode){

                mainServerTalkRetVal = 1;

            }

            break;
        }
        else
        {

            if (debugMode)
            {

                std::cout << "received " << recvBytes << " bytes from: " << serverInfoInput->connectionAddressString << std::endl;
            }

            // handle increasing of the main buffer if needed
            if ((currentStreamPos + recvBytes) >= phraseBufferCurrentLength)
            {

                phraseBufferCurrentLength += serverBufferSize;

                phraseBuffer = (char *)realloc((void *)phraseBuffer, (size_t)phraseBufferCurrentLength);

                if (phraseBuffer == NULL)
                {

                    perror("serverTalkLoop() - problem with realloc(): ");
                    mainServerTalkRetVal = 1;
                    break;
                }
            }

            // copy contents of the intermediate buffer into main and parse new section for OVER

            strncpy((phraseBuffer + currentStreamPos), recvBuffer, recvBytes);
            
            //look over the new recv string to see if OVER is in it
            //it begins to read from phraseBuffer pointed at by currentStreamPos
            if ((trueSteamLength = stringInterpret(phraseBuffer, phraseBufferCurrentLength, currentStreamPos)) != -1)
            {

                std::cout << "Server sending: Line length: " << trueSteamLength << " OVER" << std::endl;

                std::cout << "Server received: ";
                std::cout.write(phraseBuffer, (trueSteamLength + 1));
                std::cout << std::endl;

                //format data to send to client
                bytesSend = sendStringFormat(sendBuffer, sizeof(sendBuffer), trueSteamLength);

                if ((bytesSent = send(serverInfoInput->connectionSock, sendBuffer, bytesSend, 0)) == -1)
                {

                    perror("serverTalkLoop() - problem with send(): ");
                    mainServerTalkRetVal = 1;
                    break;
                }

                //reset buffer and stream postion tracker

                memset(phraseBuffer, 0, phraseBufferCurrentLength);
                currentStreamPos = 0;
            }else{
                
                //increament the stream postion tracker so new strings are appened to the old
                
                currentStreamPos += (int)recvBytes;
                std::cout << "Server received: " << phraseBuffer << std::endl;
            }


            //reset intermediate buffer
            memset(recvBuffer, 0, sizeof(recvBuffer));
        }
    }

    free(phraseBuffer);

    return mainServerTalkRetVal;
}

inline int serverLoop(struct serverInfo *serverInfoInput)
{

    int serverLoopRetVal = 0;

    const int queueSize = 1;

    // select port to listen on and acceptance loop

    if ((serverLoopRetVal = listen(serverInfoInput->listenSock, queueSize)) == -1)
    {

        perror("serverLoop(): problem with listen() on socket: ");
    }

    if (!serverLoopRetVal)
    {
        // the acceptance loop
        // when client connects print their address to the terminal
        while (!serverLoopRetVal)
        {
            memset(serverInfoInput->connectionAddressString, 0, sizeof(serverInfoInput->connectionAddressString));

            if ((serverInfoInput->connectionSock = accept(serverInfoInput->listenSock, (struct sockaddr *)&serverInfoInput->connectionAddr, &serverInfoInput->addressLength)) == -1)
            {

                perror("serverLoop() - failed to accept() client: ");
                serverLoopRetVal = 1;
                break;
            }

            inet_ntop(serverInfoInput->connectionAddr.ss_family, getInAddr((struct sockaddr *)&serverInfoInput->connectionAddr), serverInfoInput->connectionAddressString, sizeof(serverInfoInput->connectionAddressString));
            std::cout << "Server: got connection from: " << serverInfoInput->connectionAddressString << " on port: " << getInPort((struct sockaddr *)&serverInfoInput->connectionAddr) << std::endl;

            if ((serverLoopRetVal = serverTalkLoop(serverInfoInput)))
            {

                std::cerr << "serverLoop() - problem in serverTalkLoop(): LINE: " << (__LINE__ - 3) << std::endl;
                break;
            }

            if ((serverLoopRetVal = close(serverInfoInput->connectionSock)) == -1)
            {
                perror("serverLoop() - problem with close(): ");
                break;
            }
        }
    }

    return serverLoopRetVal;
}

int main(int argc, char *argv[])
{

    int mainReturnVal = 0;

    struct argHolder programArgHolder;

    struct serverInfo serverInfoHolder;

    if ((mainReturnVal = processArgs(argc, argv, &programArgHolder)))
    {

        if (mainReturnVal != -2)
        {
            std::cerr << "main() - problem in processArgs(): LINE: " << (__LINE__ - 5) << std::endl;
        }
    }

    if (!mainReturnVal)
    {

        if ((mainReturnVal = setUpServer(&programArgHolder, &serverInfoHolder)))
        {

            std::cerr << "main() - problem in setUpServer(): LINE: " << (__LINE__ - 3) << std::endl;
        }
    }

    if (!mainReturnVal)
    {

        if ((mainReturnVal = serverLoop(&serverInfoHolder)))
        {

            std::cerr << "main() - problem in serverLoop(): LINE: " << (__LINE__ - 3) << std::endl;
        }
    }

    if (!programArgHolder.redefinePort)
    {

        free(programArgHolder.newPortAddress);
    }

    if (mainReturnVal == -2)
    {

        mainReturnVal = 0;
    }

    return mainReturnVal;
}