/****************** Client CODE ****************/

#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#define max_buffer 1024
extern int errno;

char *ip_addres;
char buffer[max_buffer];
char Message[max_buffer];
//Global variable for details of character user in a game.
char username[7];
char room;
char answer;
//counter of series messages for synchronization with the type messages.
int counter = -1;
int typeMessage = -1;
//Global variable for TCP communication.
int ServertSocket;
int portTCP;
//Global variable for multicast communication.
in_addr_t mcast_addr;
struct sockaddr_in m_addr;
struct ip_mreq mreq;
int mcastSocket;
fd_set setCopy, fdSet;
unsigned int len_mcast_addr;
int mcast_conn=0;
int mcastPORT;
//Global variable for Timer
pthread_t timer;
int current_time;
int exit_timeout = 0; //flag for closing the timer.
//Fake message
int trudy;
int trudytSocket;

/*This function create TCP connection to the Server.*/
int connectToTCP(int* newSocket){
    struct sockaddr_in serverAddr;
    int con,i,opt=1;
    socklen_t addr_size;
    *newSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (*newSocket == -1) {
        perror("open client socket");
        exit(1);
    }
    if (setsockopt(*newSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,&opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    // AF_INET - internet domain (IPV4)
    //SOCK_STREAM - enable TCP connection
    // config the settings for the serverAddr
    /* Address family = Internet */
    bzero(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    /* Set IP address to localhost */
    serverAddr.sin_addr.s_addr = inet_addr(ip_addres);//argv[1] is IP address of Serer
    /* Set port number, using htons function to use proper byte order */
    serverAddr.sin_port = htons(portTCP);
    /* Set all bits of the padding field to 0 */
    memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);

    //Connect the socket to the server using the address struct.
    addr_size = sizeof(serverAddr);
    for(i=0; i<5;i++) {
        con = connect(*newSocket, (struct sockaddr *) &serverAddr, addr_size);
        if(con == 0) break;
        sleep(0.1); //wait 100ms for next trying.
    }

    return con;
}

/*This function receive username from the user and check valid characters*/
void inputFromUser() {
    int valid = 0, flag = 0;
    int i,num;
    char string[2];
    //Input username include check.
    if (typeMessage == 0) {
        do {
            valid = 0;
            //Receive characters from stdin.
            if (flag == 1)
                printf("Error username!\tPlease enter username(up to  6 char, first letter is CAP, other small letter)\n\tUSERNAME:\t");
            else printf("Please enter username(up to  6 char, first letter is CAP, others small letter)\n USERNAME:\t");
            fflush(stdin);
            scanf("%6s", username);
            fflush(stdin);
            //Check the input.
            for (i = 0; i < strlen(username); i++) {
                if (i == 0) {
                    if (username[0] >= 'A' && username[0] <= 'Z') valid = 1;
                    else {
                        valid = 0;
                        break;
                    }
                }
                else {
                    if (username[i] >= 'a' && username[i] <= 'z') valid = 1;
                    else {
                        valid = 0;
                        break;
                    }
                }
            }
            //enter timer ///////////////////////////////////////////////////
            flag = 1; //If the input isn't valid. use for message display.
        } while (!valid);
        username[strlen(username)] = '\0';
    }
    //Input Room number include check.
    if (typeMessage == 1) {
        system("clear");
        printf("\tTrivia Game Loby:\n\tROOM %c: %c/4\n\tROOM %c: %c/4\n\tROOM %c: %c/4\n",Message[0],Message[1],Message[2],Message[3],Message[4],Message[5]);
        do {
            //Receive room number from user.
            if (flag == 0) printf("\tPlease enter Room number (Room1(1),Room2(2),Room3(3))\n\tRoom:\t");
            else printf("\t  ~~~Error Room!~~~\nPlease enter Room number (Room1(1),Room2(2),Room3(3))\n Room:\t");
            fflush(stdin);
            scanf("%1s",string);
            room = *string;
            fflush(stdin);
            //Check the input.
            if ((room >= '1') && (room <= '3')) valid = 1;
            flag = 1; //If the input isn't valid. using for message display.
        } while (!valid);
    }
    if (typeMessage > 4) {
        do {
            valid = 0;
            //Receive characters from stdin.
            if (flag == 1)
                printf("Error answer!\tPlease enter your answer (a/b/c/d)\n\t Your Answer:\t");
            else printf("Please enter your answer (a/b/c/d)\n\t Your Answer:\t");
            fflush(stdin);
            scanf("%2s",string);
            answer = *string;
            fflush(stdin);
            //Check the input.
            if ((answer >= 'a') && (answer <= 'd') ) valid = 1;
            flag = 1; //If the input isn't valid. using for message display.
        } while (!valid);
    }
}

/*This function send the message to Server sockepingt. Message structure by type message. */
int sendToServer() {
    char messageToSend[max_buffer];
    int sizeOfMessage = sizeof(messageToSend);
    //Send response message about request username.
    //create the buffer will send.
    bzero(messageToSend, max_buffer);
    *messageToSend = typeMessage + '0'; //convert type message -> int to ascii
    //Type message 0 - send username.
    if (typeMessage == 0){
        strcpy(messageToSend + 1, username);
        sizeOfMessage = 1+sizeof(username);
    }
    //Type message 1 - send selected room.
    else if (typeMessage == 1) {
        strcpy(messageToSend + 1, &room);
        if(trudy != 1)sizeOfMessage = 2;
        //Fake message for test iff trudy = '1'
        if(trudy == 1) strcpy(messageToSend, "I'm a fake message");

    }
    //Type message 5-9 - send selected answer for question.
    else if(typeMessage > 4){
        strcpy(messageToSend + 1, &answer);
        sizeOfMessage = 2;
        if(trudy == 2)  strcpy(messageToSend, "I'm a DoS attack");
    }
    //send the message!
    if (send(ServertSocket, messageToSend, sizeOfMessage, 0) == -1) {
        perror("send");
        exit(1);
    }
}

/*This function receiving payload from the server and split the data.*/
void receiveFromServer() {
    int freeze = 0;//Variable to freeze the counter when another clients may join to game.
    int startGame = 0,select_status;//flag of start game.
    struct timeval fdTimer = {45,0};
    if(typeMessage < 2) {//For typeMessage 0,1,2 that will receive.
        if ((recv(ServertSocket, buffer, max_buffer, 0)) == -1) {
            perror("recv");
            exit(1);
        }
        counter++;//After receive message we increment counter for synchronization.
        //Split the message: |type|length|message|
        typeMessage = (int) (*(buffer) - '0');
        if(typeMessage < 0 || typeMessage > 2){//TypeMessage for closing a game.
            printf("%s\n",buffer);
            printf(". . .Disconnect Server!\n");//Other typeMessage is '*', case where the server stopped my participation.
            return;
        }
        if (typeMessage == 2) {
            exit_timeout = 1; //flag that cause closing the timer for timeout of connection.
            char tempMcast_addr[16];
            char tempMcast_PORT[6];
            char timer[3] = {0, 0, 0};
            //split the received message |Type|Timer|Port mcast|mcast Group|
            strncpy(timer, buffer + 1, 2);
            strncpy(tempMcast_PORT, buffer + 3, 5);
            mcastPORT = atoi(tempMcast_PORT);
            system("clear");
            //Message for user about game start.
            printf("\t\t~~Welcome to ROOM %c~~\nTime for start game is: %s seconds\n", room,timer);
            strcpy(tempMcast_addr, buffer + 8);
            mcast_addr = inet_addr(tempMcast_addr);
        } else {
            strcpy(Message, buffer + 1);
        }
        //print the message ///////////////////////////////////////
        bzero(buffer, max_buffer);
    }//end if(typeMessage+1 < 3)
    while(mcast_conn){// mcast_conn is flag of multicast connection.
        printf("_________________________________________________________\n");
        if(startGame) {
            system("clear");
            printf("\t\t\t~~~~Guy&Yaya's Trivia Game~~~~\nPlease wait for the next question . . .\n");
            //Open select for timer to receive message
            fdSet = setCopy;
            bzero(buffer, max_buffer);
            select_status = select(FD_SETSIZE, &fdSet, NULL, NULL, &fdTimer);
            if(select_status > 0){
                if (recvfrom(mcastSocket, buffer, sizeof(buffer), 0,(struct sockaddr *) &m_addr, &len_mcast_addr) == -1) {
                    perror("recvfrom Multicast");
                    exit(1);
                }
            }else if(select_status == -1){
                perror("select error");
                exit(1);
            }else{
                printf("_________________________________________________________\n");
                printf("\n\n. . .Don't receive message from server\n. . .Disconnect the server\n");
                exit(1);
            }
        }else {
            if (recvfrom(mcastSocket, buffer, sizeof(buffer), 0,(struct sockaddr *) &m_addr, &len_mcast_addr) == -1) {
                perror("recvfrom Multicast");
                exit(1);
            }
        }

        //When everything ok and the game start.
        if(startGame) {
            system("clear");
            printf("\t\t\t~~~~Guy&Yaya's Trivia Game~~~~\n");
        }
        if(!freeze) counter++; //counter of series messages. After receive message we increment counter for synchronization.
        typeMessage = (int) (*(buffer) - '0');
        if(typeMessage == 3) freeze = 1;
        else freeze = 0;
        if (typeMessage == 4) counter++; //Calibration for unfreeze counter.
        if(typeMessage < 0){//'*' or '#' is TypeMessage for closing a game.
            printf("%s\n",buffer+1);
            if(typeMessage == (35-48)) {//If typeMessage is '#' the game ended properly. ASCII: '#'=35,'0'=48.
                printf("\t\t~~~The game is over!~~~\n");
            }
            printf("\n\n. . . Disconnect Server!\n");//Other typeMessage is '*', case where the server stopped my participation.
            return;
        }
        if(typeMessage != counter){
            //close connect from server and close the program.
            perror("Error Type message");
            return;
        }
        printf("%s\n",buffer+1);//The game is not begin, state of Joining other clients.
        if(startGame) {
            inputFromUser();//Receive answer from user.
            sendToServer();
        }
        if(typeMessage == 4) {//the game is starting here.
            startGame = 1;
        }
    }
}

/*This function create connection to multicast group according to the server message.*/
void mcast_connection(){
    int opt =1;
    len_mcast_addr = sizeof(mcast_addr);
    //In message received Multicast IP and PORT.
    //Open new socket for multicast group.
    mcastSocket = socket(AF_INET, SOCK_DGRAM,0);
    if(mcastSocket == -1){
        perror("multicast socket");
        exit(1);
    }
    bzero((char *)&m_addr, sizeof(m_addr));
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = htons(mcastPORT);
    m_addr.sin_addr.s_addr = mcast_addr;

    if (setsockopt(mcastSocket, SOL_SOCKET, SO_REUSEPORT|SO_REUSEADDR,&opt,sizeof(opt)) == -1) {
        perror("setsockopt mreq");
        exit(1);
    }

    //bind the socket and multicast address.
    if (bind(mcastSocket, (struct sockaddr *) &m_addr, sizeof(m_addr)) < 0) {
        perror("bind");
        exit(1);
    }

    mreq.imr_multiaddr.s_addr = mcast_addr;
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    socklen_t len_mreq = sizeof(mreq);
    if (setsockopt(mcastSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP,&mreq,len_mreq) == -1) {
        perror("setsockopt mreq");
        exit(1);
    }
    mcast_conn = 1;
    FD_ZERO(&setCopy);
    FD_SET(mcastSocket,&setCopy);
}

int main(int argc, char *argv[]) {
    int j,connect_flag = 1;
    ip_addres = argv[1]; // ip address of the sender
    char *port = argv[2]; // port number to connect
    if(argc == 4) trudy = atoi(argv[3]);
    portTCP = atoi(port);
    system("clear");
    if(trudy == 2){//DoS attack of 10 connections.
        for(j = 0; j<10;j++){
            printf("Create DoS attack: connect number %d\n",j+1);
            if(j==0) connectToTCP(&ServertSocket);
            else connectToTCP(&trudytSocket);
        }
        while(1);
    }
    //Open TCP socket for welcome to Game.
    connect_flag = connectToTCP(&ServertSocket);
    if (connect_flag == 0) {
        printf("#connect is success!\n");
        while (1) {
            //Receive massage from server.
            receiveFromServer();
            if(typeMessage < 0) {//close the multicast socket and close the program.
                if(mcast_conn){//check if executed multicast connection.
                    if (close(mcastSocket) == -1) {
                        perror("close");
                        exit(1);
                    }
                }
                break;
            }
            //check synchronization with server messages.
            if (typeMessage != counter) {
                //close connect from server and close the program.
                perror("Error Type message");
                break;
            }
            //From Type message 2 is join to room and connect to multicast group.
            if(typeMessage == 2){
                mcast_connection();
            }
            if(typeMessage < 2) {
                inputFromUser();    //input username from user.
                sendToServer();
            }
        }
    //if unsuccessful connection then else:
    } else {
        printf("#connection isn't succeed\n");
    }
    //close the socket for Server.
    if (close(ServertSocket) == -1) {
        perror("close");
        exit(1);
    }
    return 0;
}