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

bool debugMode = false;

const char clientHelpText[] =
    "|---------------------------------------------------|\n"
    "|====================== Help =======================|\n"
    "|    -h - prints out program arg contents           |\n"
    "|    -p - defines port number other then defualt    |\n"
    "|         9838                                      |\n"
    "|    -d - changes dest address, the defualt is      |\n"
    "|         127.0.0.1                                 |\n"
    "|    -l - enables printing debug statements         |\n"
    "|---------------------------------------------------|\n";

const char defualtPortName[] = "9838";
const char defualtAddrName[] = "127.0.0.1";

struct argHolder
{

    argHolder() = default;

    bool helpArg = false;
    bool redefinePort = false;
    bool redefineAddress = false;

    char *newPortAddress = nullptr;
    char *newAddress = nullptr;
};

struct clientInfo
{
    clientInfo() = default;

    char *addressToSend = nullptr;
    char *portUsed = nullptr;

    // infomation for communicating to server
    struct addrinfo basedInfo;
    struct addrinfo *serverAddrInfo = nullptr;
    struct addrinfo *addList = nullptr;

    // storage for address regardless of protocal
    struct sockaddr_storage sockaddrIpInfo;

    // socket address length
    socklen_t addressLength = sizeof(struct sockaddr_storage);

    // file descriptors

    int clientSock = 0;
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
        case 'd':

            argHolderInput->redefineAddress = true;

            argHolderInput->newAddress = new char(strlen(optarg));

            if (argHolderInput->newAddress == nullptr)
            {

                std::cerr << "argHandler() - error acquiring memory with new for addressName" << std::endl;
                argHandlerRetVal = 1;
                break;
            }

            strcpy(argHolderInput->newAddress, optarg);

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

    size_t defualtAddressLength = sizeof(defualtAddrName);
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

        if (!argHolderInput->redefineAddress)
        {

            argHolderInput->newAddress = new char(defualtAddressLength);

            if (argHolderInput->newAddress == nullptr)
            {

                std::cerr << "processArgs() - failure in memory allocation: Line: " << (__LINE__ - 5) << std::endl;

                argProcessRetVal = 1;
            }
            else
            {

                strncpy(argHolderInput->newAddress, defualtAddrName, defualtAddressLength);
            }
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

inline void programArgCleanUp(struct argHolder *argHolderInput)
{

    free(argHolderInput->newAddress);

    if (!argHolderInput->redefinePort)
    {

        free(argHolderInput->newPortAddress);
    }
}

int initClient(struct argHolder *argHolderInput, struct clientInfo *clientInfoInput)
{

    int clientInitRetVal = 0;

    int getAddressReturnVal = 0;

    // move address and port data over
    clientInfoInput->addressToSend = argHolderInput->newAddress;
    clientInfoInput->portUsed = argHolderInput->newPortAddress;

    memset(&clientInfoInput->basedInfo, 0, sizeof(struct addrinfo));

    // set socket for tcp and dont care about ip version
    clientInfoInput->basedInfo.ai_family = AF_UNSPEC;
    clientInfoInput->basedInfo.ai_socktype = SOCK_STREAM;

    if ((getAddressReturnVal = getaddrinfo(clientInfoInput->addressToSend, clientInfoInput->portUsed, &clientInfoInput->basedInfo, &clientInfoInput->serverAddrInfo)) != 0)
    {
        std::cerr << "initClient() - problem with getaddrinfo(): Line: " << (__LINE__ - 2) << "| return code: " << gai_strerror(getAddressReturnVal) << std::endl;
        clientInitRetVal = 1;
    }

    // interate through list to find address that can be connected too
    if (!clientInitRetVal)
    {
        for (clientInfoInput->addList = clientInfoInput->serverAddrInfo; clientInfoInput->addList != NULL; clientInfoInput->addList = clientInfoInput->addList->ai_next)
        {
            if ((clientInfoInput->clientSock = socket(clientInfoInput->addList->ai_family, clientInfoInput->addList->ai_socktype, clientInfoInput->addList->ai_protocol)) == -1)
            {
                perror("initClient() - problem with socket(): ");
                continue;
            }

            if (connect(clientInfoInput->clientSock, clientInfoInput->addList->ai_addr, clientInfoInput->addList->ai_addrlen) == -1)
            {
                if (close(clientInfoInput->clientSock))
                {

                    perror("initClient() - problem with close(): ");

                    std::cerr << "Line: " << (__LINE__ - 5) << std::endl;

                    clientInitRetVal = 2;

                    break;
                }
                perror("initClient() - problem with connect(): ");
                continue;
            }

            break;
        }
    }

    if (!clientInitRetVal)
    {
        if (clientInfoInput->addList == NULL)
        {
            std::cerr << "initClient() - failed to connect to server" << std::endl;
            clientInitRetVal = 2;
        }
    }

    if (clientInitRetVal != 1)
    {

        freeaddrinfo(clientInfoInput->serverAddrInfo);
    }

    return clientInitRetVal;
}

// looks for words that can be seen as codes and returns and int for executer to use
inline int stringInterpret(std::string userStringInput)
{

    int interpretResults = 0;

    const char quitCommand[] = "QUIT"; // code 1
    const char overCommand[] = "OVER"; // code 2
    // check for EOF

    if (std::cin.eof())
    {

        interpretResults = 1;
    }

    // start to check for commands
    if (!interpretResults)
    {
        for (size_t i = 0; i < userStringInput.size(); i++)
        {

            if (userStringInput[i] == quitCommand[0])
            {

                if (!strncmp(userStringInput.substr(i).data(), quitCommand, 4))
                {

                    interpretResults = 1;

                    break;
                }
            }
            else if (userStringInput[i] == overCommand[0])
            {

                if (!strncmp(userStringInput.substr(i).data(), overCommand, 4))
                {

                    interpretResults = 2;

                    break;
                }
            }
        }
    }

    return interpretResults;
}

inline void stringWordRemove(char *stringSrcInput, size_t srcLength, char *wordToRemove, size_t wordToRemoveLength)
{

    for (size_t i = 0; i < srcLength; i++)
    {

        if (stringSrcInput[i] == wordToRemove[0])
        {

            if (!strncmp(stringSrcInput + i, wordToRemove, wordToRemoveLength))
            {

                for (size_t x = i; x < srcLength; x++)
                {

                    stringSrcInput[x] = 0;
                }

                break;
            }
        }
    }
}

inline int mainTcpLoop(struct clientInfo *clientInfoInput)
{

    int mainLoopRetVal = 0;

    bool loopAliveStatus = true;

    int bytesSent = 0;
    int bytesRecv = 0;

    int userInputCommandStat = 0;

    std::string userInput;

    char recvBuffer[100];

    char overCommand[] = "OVER";

    memset(recvBuffer, 0, sizeof(recvBuffer));

    while (loopAliveStatus)
    {

        // get user input
        std::getline(std::cin, userInput);

        // check the user input for any commands like QUIT or OVER and see if the recv function should be ignored
        // if no over was stated

        if ((userInputCommandStat = stringInterpret(userInput)) == 1)
        {

            break;
        }

        // send the line and then recv server's response

        if ((bytesSent = send(clientInfoInput->clientSock, userInput.data(), userInput.size(), 0)) == -1)
        {

            perror("mainTcpLoop() - problem with send: ");

            std::cerr << "Line: " << (__LINE__ - 5) << std::endl;

            loopAliveStatus = false;

            mainLoopRetVal = 1;

            break;
        }

        if (debugMode)
        {

            std::cout << "sent " << bytesSent << " bytes to " << clientInfoInput->addressToSend << std::endl;
        }

        if (userInputCommandStat == 2)
        {

            if ((bytesRecv = recv(clientInfoInput->clientSock, recvBuffer, sizeof(recvBuffer), 0)) == -1)
            {

                perror("mainTcpLoop() - problem with recv: ");

                std::cerr << "Line: " << (__LINE__ - 5) << std::endl;

                loopAliveStatus = false;

                mainLoopRetVal = 1;

                break;
            }

            if (debugMode)
            {

                std::cout << "recv " << bytesRecv << " bytes from " << clientInfoInput->addressToSend << std::endl;
            }

            stringWordRemove(recvBuffer, sizeof(recvBuffer), overCommand, sizeof(overCommand));

            std::cout << "Server sent: " << recvBuffer << std::endl;
        }
    }

    return mainLoopRetVal;
}

int main(int argc, char *argv[])
{

    int mainReturnVal = 0;

    struct argHolder programArgHolder;
    struct clientInfo mainClientInfo;

    if ((mainReturnVal = processArgs(argc, argv, &programArgHolder)))
    {

        if (mainReturnVal != -2)
        {
            std::cerr << "main() - problem with processArgs: Line: " << (__LINE__ - 3) << std::endl;
        }
    }

    if (!mainReturnVal)
    {

        if ((mainReturnVal = initClient(&programArgHolder, &mainClientInfo)))
        {

            std::cerr << "main() - problem with initClient(): Line: " << (__LINE__ - 3) << std::endl;
        }
    }

    if (!mainReturnVal)
    {

        if ((mainReturnVal = mainTcpLoop(&mainClientInfo)))
        {

            std::cerr << "main() - problem with mainTcpLoop(): Line: " << (__LINE__ - 3) << std::endl;
        }
    }

    programArgCleanUp(&programArgHolder);

    if (!mainReturnVal || mainReturnVal == -2)
    {

        std::cout << "Client exiting normally" << std::endl;

        mainReturnVal = 0;
    }

    return mainReturnVal;
}