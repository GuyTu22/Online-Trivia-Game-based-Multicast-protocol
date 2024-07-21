/******************game server********************/
#include <time.h>
#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
//#include <errno.h>
#define timer 40
#define num_of_questions 2
#define max_players 10

//------function declaration--------//
//register thread for new player,this func receive from the user: name and room number and send him room status and multicast group details for the game.//
void* new_register(void* player_number);
// game thread for the trivia game, after finish register process the player join this thread and waits for the game start
// the thread multicast is messages to the room players and receive answers with udp connection
void* game_thread(void* room_number);
// thread counter for 60 seconds , activate when the first player enter the room and wait for other players
void* openTimer(void* arg);
//function for sending tcp message for the player
void send_TCP_message(int socket_number,char *message);
//function for reset the room for next game
void close_room_resource(int i);
//function for reset user resource in case of a problem
void close_user_resource(int i);
// function for return ip addres player , from tcp connection
void return_player_addres(int newSocket,char*client_ip);



//------player struct --------//
typedef struct player{
    int socket_num;
    int room_num;
    char name[7];
}player;

//------global arr of players--------//
player players[10];
//------counter for current players--------//
int total_players = 0;
//------global arr for room players--------//
int rooms_count[3] = {0,0,0};
//------global arr for saving the players inside the room--------//
int room_players[3][4] = {{0,0,0,0},{0,0,0,0},{0,0,0,0},};
//------global arr for register threads, allow 10 players to register in the same time--------//
pthread_t register_thread[10] = {0,0,0,0,0,0,0,0,0,0};
//------global arr for active games, max 3 active game in the same time--------//
pthread_t  game[3] = {0,0,0} ;
//------global arr for multicast address for the rooms--------//
char multicast_address[3][10] = {"225.4.7.1","225.4.7.2","225.4.7.3"};
//------global arr for multicast ports for the rooms--------//
int multicast_port[3] = {12345,12346,12347};
//------global arr for room timer --------//
int room_timer[3] ={0,0,0};
//------global arr for thread timer --------//
pthread_t timer_thread[3] = {0,0,0};
//------global arr for join flag --------//
int join_flag[3] = {0,0,0};
//------global arr for trivia questions --------//
char question_1[] = "5question 1:\nWho was the first president of Israel?\n\n\ta.Eyal Golan\t\t\tb.David Ben Gurion\n\n\tc.Moshe Sharett\t\t\td.Haim Weizmann\n";
char question_2[] = "6question 2:\nWhat is the name of the largest ocean on earth?\n\n\ta.Pacific Ocean\t\t\tb.Indian Ocean\n\n\tc.Arctic Ocean\t\t\td.Southern Ocean\n";
//char question_3[] = "7question 3:\nWho won the FIFA world cup in 2002?\n\n\ta.Brazil\t\t\tb.Israel\n\n\tc.Italy\t\t\t\td.France\n";
//char question_4[] = "8question 4:\nWhich planet is closest to the sun?\n\n\ta.Mars\t\t\tb.Mercury\n\n\tc.Jupiter\t\td.Venus\n";
//char question_5[] = "9question 2:\nWho is the best lab instructor??\n\n\ta.Raz\t\t\tb.Daniel\n\n\tc.Chen\t\t\td.Big Efi\n";
//-----global pointer arr the trivia questions --------//
char *questions[num_of_questions] = {question_1,question_2};//,question_3,question_4,question_5};
//------global arr for the trivia answers --------//
char answers[num_of_questions] = {'d','a'};//,'a','b','d'};
//------global arr for room sockets--------//
int room_sockets[3][4] = {{0,0,0,0},{0,0,0,0},{0,0,0,0}};
//------global arr for room flags--------//
//when shutdown the server,  power off room flags for closing room threads!
int room_flags[3] = {1,1,1};
//global var for face server test
int fake;
//global arr for saving clients IP
char ip_connections[10][20];

int main(int argc, char* argv[]){
    int i = 0,room_1 =0,room_2 = 1,room_3 = 2;
    char* ip_address = argv[1]; // ip address of the sender
    char* port = argv[2]; // port number to connect
    if(argc == 4){ // in case we want to run fake server test
        fake = 1;
    }
    int port_convert = atoi(port); // ascii to int
    int welcomeSocket,newSocket,opt =1,found_problem=0,test;
    char client_ip[20];
    struct sockaddr_in serverAddr;
    struct sockaddr_storage serverStorage;
    socklen_t addr_size;
    char buffer[1024];
    bzero(buffer,1024); // reset the buffer
    //------reset the  players socket filed--------////
    for( i =0;i<10;i++){
        players[i].socket_num =0;
    }
    //------open 3 game threads--------//
    pthread_create(&game[0],NULL, game_thread,&room_1);
    pthread_create(&game[1],NULL, game_thread,&room_2);
    pthread_create(&game[2],NULL, game_thread,&room_3);
    //create the socket
    welcomeSocket = socket(AF_INET, SOCK_STREAM, 0);
    // AF_INET - internet domain (IPV4)
    //SOCK_STREAM - communication type, in our case TCP

    if (setsockopt(welcomeSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // config the settings for the serverAddr
    /* Address family = Internet */
    serverAddr.sin_family = AF_INET;
    /* Set port number, using htons function to use proper byte order */
    serverAddr.sin_port = htons(port_convert);
    /* Set IP address to localhost */
    serverAddr.sin_addr.s_addr = inet_addr(ip_address);
    /* Set all bits of the padding field to 0 */
    memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);

    // bind the address struct to the socket
    if( bind(welcomeSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) == -1){
        perror("problem in bind function");
        exit(EXIT_FAILURE);
    }


    /*---- Listen on the socket, with 10 max connection requests queued ----*/
    if(listen(welcomeSocket,max_players)==0) { // n indicates the queue size for waiting connections
        printf("Server Listening for new connections...\n");
    }
    else {
        perror("problem in listen function");
        exit(EXIT_FAILURE);
    }
    /*---- Accept call creates a new socket for the incoming connection ----*/
    addr_size = sizeof (serverStorage);
    while(1) {
        newSocket = accept(welcomeSocket, (struct sockaddr *) &serverStorage, &addr_size);
        //// wait until new connection request enter the queue////
        if (newSocket == -1) {
            perror("problem in accept function");
            exit(EXIT_FAILURE);
        }

        printf("accept new connection,socket number is %d.\n",newSocket);
        //---prevent DOS ATTACK , saving each IP of new player,check if this IP tried to connect more than one time---//
        //found the new connection IP address
        return_player_addres(newSocket,client_ip);
        for (i = 0; i < 10; i++) {
            test = strcmp(ip_connections[i],client_ip);
            if (  test == 0){ // the string are equal
                //same IP try to connect more than one time
                printf("****ALERT!!! new connection is already in the system.****\n");
                found_problem = 1; //
            }
        }
        if(found_problem ==1){
            //close connection with client
            if( close(newSocket == -1)) {
                perror("problem in close  ALERT socket");
                exit(EXIT_FAILURE);
            }
            found_problem = 0;
        }else{
            //------enter the new connection to the players arr--------//
            for (i = 0; i < 10; i++) {
                if (players[i].socket_num == 0 && register_thread[i] == 0) { //empty place//
                    //enter the ip of the player
                    strcpy(ip_connections[i],client_ip);
                    printf("the new player ip: %s\n",ip_connections[i]);
                    //enter new socket
                    players[i].socket_num = newSocket;
                    break;
                }
            }
            total_players++; // update the counter for total players
            //------register the new player--------//
            pthread_create(&register_thread[i],NULL, new_register,&i);
        }
    }
}

 void* new_register(void* player_number){
     int i = *(int*) player_number,message_counter =0;
     int finish_flag = 1,select_status,pass_time = 0,timer_index =0;
     clock_t new_player_time; // timer var the new player
     struct timeval time_out = {30,0}; //initialize the time out for select function
     fd_set client_fd_copy,client_fd; // will hold the file descriptor for select function
     char room_time[3]; // will hold the counter for the game start
     char message[100];
     char port[5];
     message[0] = '0';
     char hello[] = "~~~~~~ welcome to the Trivia game of Guy&Yaya ~~~~~~";
     char buffer[1024];
     bzero(buffer,1024); // reset the buffer
     strcat(message,hello); // build the send message to the client
     //------send welcome message to the new client--------//
     // for check in client what happen in case of fake server, happened only in second register and if 4 argv enterd
    if(fake == 1 &&  total_players == 2){
        printf("send fake server message for client\n");
        strcpy(message,"I'm a fake server hahahah\n");
        send_TCP_message(players[i].socket_num,message);
        fake =0;
        close_user_resource(i);
        finish_flag = 0;
    }
        else {
        send_TCP_message(players[i].socket_num, message);

        FD_ZERO(&client_fd_copy); // clear fd
        FD_SET(players[i].socket_num, &client_fd_copy); // add the client socket the fd arr to check
        while (finish_flag) {
            client_fd = client_fd_copy; // copy for fd arr
            bzero(buffer, 1024); // reset the buffer
            //------wait for the client to answer for 30 seconds--------//
            select_status = select(FD_SETSIZE, &client_fd, NULL, NULL, &time_out);
            if (select_status < 0) { // error in select function
                perror("error occurred in select function/n");
                exit(EXIT_FAILURE);
            } else if (select_status == 0) { //time interval from client passed
                //------handle in case user didn't answer--------//
                printf("user number %d didn't answer for 30 seconds,connection closed.\n", players[i].socket_num);
                //reset resources
                close_user_resource(i);
                finish_flag = 0;
            } else { // client returned answer in the time interval
                //------receive client name from the buffer--------//
                if (recv(players[i].socket_num, buffer, 1024, 0) < 0) {
                   printf("connection problem++\n");
                   close_user_resource(i);
                   finish_flag = 0;
                }

                if (buffer[0] == '0') { // client send is name
                    //------enter the name to the player struct--------//
                    strncpy(players[i].name, buffer + 1, strlen(buffer) - 1);
                    //------send to client to room status--------//
                    message[0] = '1';
                    message[1] = '1';
                    message[2] = '0' + rooms_count[0];
                    message[3] = '2';
                    message[4] = '0' + rooms_count[1];
                    message[5] = '3';
                    message[6] = '0' + rooms_count[2];
                    message[7] = '\n';
                    //------send to client room information--------//
                    send_TCP_message(players[i].socket_num, message);

                }
                else if (buffer[0] == '1') { // client send message with the room is chosen
                    //------enter the room number to the client struct--------//
                    players[i].room_num = (int) (buffer[1] - '0');
                    //------update the room arr--------//
                    rooms_count[players[i].room_num - 1]++;
                    //------add the socket to the room arr-------//
                    room_sockets[players[i].room_num - 1][rooms_count[(players[i].room_num) - 1] -
                                                          1] = players[i].socket_num;
                    // convert the port number to string
                    snprintf(port, 6, "%d", multicast_port[players[i].room_num - 1]);
                    // add port,multicast,timer to the message
                    sprintf(room_time, "%d", timer);
                    sprintf(message, "2%s%s%s",room_time, port, multicast_address[players[i].room_num - 1]);
                    //------check if the player is the first one in the room--------//
                    if (rooms_count[players[i].room_num - 1] == 1) { // the player is the first player in the room
                        //------send the player the room timer, room multicast group and room udp port--------//
                        send_TCP_message(players[i].socket_num, message);
                        //------start the room timer, the timer is set according to the room number------//
                        timer_index = players[i].room_num - 1;
                        pthread_create(&timer_thread[players[i].room_num - 1], NULL, &openTimer, &timer_index);
                        //------enter the player as an active player in the room--------//
                        room_players[players[i].room_num - 1][rooms_count[players[i].room_num - 1] - 1] = i;
                        printf("~~~register new player finish~~~:\n");
                        printf("Name: %s\n", players[i].name);
                        printf("Room: %d\n", players[i].room_num);
                        printf("Players in room %d: %d\n", players[i].room_num, rooms_count[players[i].room_num - 1]);
                        printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
                        //------activate the join flag--------//
                        sleep(1); // sleep for giving time for client set the multicast port
                        join_flag[players[i].room_num - 1] = 1;
                    } else {// the player isn't the first one in the room
                        //save the time pass from the room open
                        pass_time = timer -
                                    room_timer[players[i].room_num - 1]; // 60 -  time, for make it easy for client side
                        //convert the time pass to char
                        sprintf(room_time, "%d", pass_time);
                        //add to the message room timer of new player time
                        strncpy(message + 1, room_time, 2);
                        //send the message
                        send_TCP_message(players[i].socket_num, message);
                        //------enter the player as an active player in the room--------//
                        room_players[players[i].room_num - 1][rooms_count[players[i].room_num - 1] - 1] = i;
                        printf("register new player finish:\n");
                        printf("Name: %s\n", players[i].name);
                        printf("Room: %d\n", players[i].room_num);
                        printf("Players in room %d: %d\n", players[i].room_num, rooms_count[players[i].room_num - 1]);
                        printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
                        //------activate the join flag--------//
                        sleep(1); // sleep, for giving time for client set the multicast port
                        join_flag[players[i].room_num - 1] = 1;
                    }
                    //------finish register process--------//
                    finish_flag = 0;

                } else { // problem ih the format answer of the client
                    //disconnect client
                    printf("Alert!!! receive from user message not in the right format:\n%s\n", buffer);
                    printf("disconnect user\n");
                    sprintf(message, "*Message not in the correct format!");
                    send_TCP_message(players[i].socket_num, message);
                    close_user_resource(i);
                    finish_flag = 0;

                }
            }
            bzero(buffer, 1024); // reset the buffer
        }

        //reset the thread arr before exit//
        register_thread[i] = 0;
        //exit the thread//
        pthread_exit(NULL);
    }
 }

// game thread for the trivia game, after finish register process the player join this thread and waits for the game start
// the thread multicast is messages to the room players and receive answers with udp connection
void* game_thread(void* room_number){
    int i = *(int*) room_number,multicast_socket,j=0,select_status=0,ans_counter = 0, k=0;
    struct timeval time_out = {30,0}; //initialize the time out for select function
    int points_counter[4] = {0,0,0,0},next_question = 1; // arr for counting players points
    struct sockaddr_in multicastAddr;
    unsigned char ttl = 32;
    char message[1024],buffer[1024];
    bzero(buffer,1024); // reset the buffer
    fd_set client_fd_copy,client_fd; // will hold the file descriptor for select function
    //------open udp socket for the multicast messages--------//
    if ( (multicast_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("multicast socket creation failed");
        exit(EXIT_FAILURE);
    }
    //------set the socket as a multycast socket--------//
    if(setsockopt(multicast_socket,IPPROTO_IP,IP_MULTICAST_TTL,(void*)&ttl,sizeof(ttl)) < 0){
        perror("set multicast socket error\n");
        exit(EXIT_FAILURE);
    }
    //set ttl for the IP protocol
    if(setsockopt(multicast_socket,IPPROTO_IP,IP_TTL,(void*)&ttl,sizeof(ttl)) < 0){
        perror("set multicast socket error\n");
        exit(EXIT_FAILURE);
    }
    /* Construct local address structure */
    memset(&multicastAddr, 0, sizeof(multicastAddr));   /* Zero out structure */
    multicastAddr.sin_family = AF_INET;                 /* Internet address family */
    multicastAddr.sin_addr.s_addr = inet_addr(multicast_address[i]);/* Multicast IP address */
    multicastAddr.sin_port = htons(multicast_port[i]); /* Multicast port */

        while(room_flags[i]) {
            do {
                //------send join message in the multicast group--------//
                if (join_flag[i] == 1) {
                    sprintf(message, "3%s joined room number %d", players[room_players[i][rooms_count[i] - 1]].name,players[room_players[i][rooms_count[i] - 1]].room_num);
                    //------send join message in the multicast group--------//
                    if (sendto(multicast_socket, message, strlen(message), 0, (struct sockaddr *) &multicastAddr,
                               sizeof(multicastAddr)) != strlen(message)) {
                        perror("error in sending multicast error\n");
                        exit(EXIT_FAILURE);
                    }
                    //reset the flag for next player to join
                    join_flag[i] = 0;
                }

            } while (room_timer[i] < timer ); //&& rooms_count[i] != 4); // run until timer closed or room is full (4 players)

            printf("time for players registration in room %d finished.\n",i+1);

            //------timer finished and only one player is inside the room--------//
            if (rooms_count[i] == 1) { //
                // message the player and out from the thread
                printf("only one player inside room %d,close room.\n",i+1);
                sprintf(message, "*No other players joined room number %d :( , try connect later...",i+1);
                if (sendto(multicast_socket, message, strlen(message), 0, (struct sockaddr *) &multicastAddr,
                           sizeof(multicastAddr)) != strlen(message)) {
                    perror("error in sending multicast error\n");
                    exit(EXIT_FAILURE);
                }
                //-----close all the game resource----//
                close_room_resource(i);

            } else { //more than one player is inside the room , game start!
                //------send start game message-------//
                printf("--------------------------------\n");
                printf("~~~  game in room %d start(: ~~~\n",i+1);
                printf("________________________________\n");
                sprintf(message, "4****Game start Prepare yourselves****\n");
                if (sendto(multicast_socket, message, strlen(message), 0, (struct sockaddr *) &multicastAddr,
                           sizeof(multicastAddr)) != strlen(message)) {
                    perror("error in sending multicast error\n");
                    exit(EXIT_FAILURE);
                }

                FD_ZERO(&client_fd_copy); // clear fd
                //------add the players sockets to the the FD SET--------//
                for (j = 0; j < rooms_count[i]; j++) {
                   // printf("add socket number %d to the file descriptor\n", room_sockets[i][j]);
                    FD_SET(room_sockets[i][j], &client_fd_copy); // add the client socket the fd arr to check
                }
                for (j = 0; j < num_of_questions; j++) {//run as count of the question number
                    next_question = 1;
                    strcpy(message, questions[j]);
                    //------send the question to the players in multicast--------//
                    if (sendto(multicast_socket, message, strlen(message), 0, (struct sockaddr *) &multicastAddr,
                               sizeof(multicastAddr)) != strlen(message)) {
                        perror("error in sending multicast error\n");
                        exit(EXIT_FAILURE);
                    }

                    do {
                        client_fd = client_fd_copy;
                        select_status = select(FD_SETSIZE, &client_fd, NULL, NULL, &time_out);
                        if (select_status < 0) { // error in select function
                            perror("error occurred in select function/n");
                            exit(EXIT_FAILURE);
                        }
                        else if(select_status == 0) { // time for answer the question pass
                            printf("time to answer finished!\n");
                            next_question = 0;
                            time_out.tv_sec = 60;
                            time_out.tv_usec = 0;

                        }
                        else { // receive answer from one of the players
                            for (k = 0; k < rooms_count[i]; k++) {
                                if (FD_ISSET(room_sockets[i][k], &client_fd)) { // find the player that send ans
                                    //update the counter answer
                                    ans_counter++;
                                    if (recv(room_sockets[i][k], buffer, 1024, 0) < 0) {
                                        perror("problem in read function");
                                        exit(EXIT_FAILURE);
                                    }
                                    if (buffer[1] == answers[j]) { //player ans correct
                                        points_counter[k]++;
                                    } else {//player answer wrong
                                        //no points!
                                    }
                                    break;
                                }
                            }
                        }

                    } while (ans_counter != rooms_count[i] && next_question); //wait for player answers for timer sec or exit when all players answerd
                    //reset the timer ans for next round
                    ans_counter = 0;
                    //move forward to next question
                }
                //-----game finish-----//
                //send the game score for the players
                printf("--------------------------------\n");
                printf("~~~  game in room %d finish! ~~~\n",i+1);
                printf("_______________________________\n");
                printf("print the score of the players:\n");
                for (j = 0; j < rooms_count[i]; j++) {
                    printf("%s got: %d points\n", players[room_players[i][j]].name, points_counter[j]);
                }
                //-----send game results-----//
                sprintf(message, "#room number %d results are:\n\n",i+1);
                for (j = 0; j < rooms_count[i]; j++) {
                    sprintf(buffer, "%s got %d points.\n", players[room_players[i][j]].name, points_counter[j]);
                    strcat(message, buffer);
                    //reset local points counter for next round
                    points_counter[j] = 0;
                }
                if (sendto(multicast_socket, message, strlen(message), 0, (struct sockaddr *) &multicastAddr,
                           sizeof(multicastAddr)) != strlen(message)) {
                    perror("error in sending multicast error\n");
                    exit(EXIT_FAILURE);
                }
                //-----close all the game resource----//
                close_room_resource(i);
                printf("room %d waiting for next game to open...\n",i+1);
                printf("________________________________\n");
            }

        }
}


 //function for sending the player a message
void send_TCP_message(int socket_number,char *message){
     if (send(socket_number, message, strlen(message), 0) < 0) {
         perror("problem in send function");
         exit(EXIT_FAILURE);
     }
}


void* openTimer(void* arg){
    int i = *((int*)arg); // i point the room index
    int second = timer * 3; //mul by 3 because each thread,burn through CPU time much faster
    int differ_sec;
    int milliseconds_current;
    room_timer[i] = 0;
    int milliseconds = second*1000;
    // a current time of milliseconds
    int milliseconds_since = clock() * 1000 / CLOCKS_PER_SEC;
    // needed count milliseconds of return from this timeout
    int end = milliseconds_since + milliseconds;
    // wait while until needed time comes
    do {
        milliseconds_current = clock() * 1000 / CLOCKS_PER_SEC;
        differ_sec = (milliseconds_current - milliseconds_since) / 1000;
        if(differ_sec > 3 * room_timer[i]) {
            room_timer[i] = differ_sec/3;
        }

    } while ( milliseconds_current <= end);
    pthread_exit(NULL);
}

void close_room_resource(int i){
    int  j =0;
    for(j=0;j<rooms_count[i];j++){
        //close the player socket//
        if( close(room_sockets[i][j]) == -1) {
            perror("problem in close socket");
            exit(EXIT_FAILURE);
        }
        //reset the socket by room
        room_sockets[i][j] = 0;
        //reset the player struct
        players[room_players[i][j]].socket_num =0;
        players[room_players[i][j]].room_num =0;
        bzero(players[room_players[i][j]].name,7);
        //reset the register thread
        register_thread[room_players[i][j]] = 0;
        //reset the room timer
        room_timer[i] = 0;
        //reset the player ip
        bzero(ip_connections[room_players[i][j]],20);
        //reset the room players index
        room_players[i][j] = 0;
        timer_thread[i] = 0;
        total_players--;
    }
    //reset the room counter//
    rooms_count[i] =0;

}

void close_user_resource(int i){
    if( close(players[i].socket_num) == -1) {
        perror("problem in close player socket");
        exit(EXIT_FAILURE);
    }
    bzero(ip_connections[i],20);
    players[i].socket_num = 0;
    register_thread[i] = 0;
    players[i].room_num= 0 ;
    bzero(players[i].name,7);
    total_players--;
}


void return_player_addres(int newSocket,char*client_ip){
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    int res = getpeername(newSocket, (struct sockaddr *)&addr, &addr_size);
    strcpy(client_ip, inet_ntoa(addr.sin_addr));
}

