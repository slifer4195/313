#include <string>
#include "TCPRequestChannel.h"


#include "common.h"
#include <sys/socket.h>
#include <stdio.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <iostream>
using namespace std;


void connection_handler (int client_socket){
    char buf [1024];
    while (true){
        if (recv (client_socket, buf, sizeof (buf), 0) < 0){
            perror ("server: Receive failure");
            exit (0);
        }
        int num = *(int *)buf;
        num *= 2;
        if (num == 0)
            break;
        if (send(client_socket, &num, sizeof (num), 0) == -1){
            perror("send");
            break;
        }
    }
    cout << "Closing client socket" << endl;
    close(client_socket);
}

TCPRequestChannel::TCPRequestChannel (const string host_name, const string port_no){

    if (host_name == ""){
        struct addrinfo hints, *serv;
        struct sockaddr_storage their_addr; // connector's address information
        socklen_t sin_size;
        char s[INET6_ADDRSTRLEN];
        int rv;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE; // use my IP

        if ((rv = getaddrinfo(NULL, port_no.c_str(), &hints, &serv)) != 0) {
            cerr  << "getaddrinfo: " << gai_strerror(rv) << endl;
            exit(-1);
        }
        if ((sockfd = socket(serv->ai_family, serv->ai_socktype, serv->ai_protocol)) == -1) {
            perror("server: socket");
            exit(-1);
        }
        if (bind(sockfd, serv->ai_addr, serv->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            exit(-1);
        }
       freeaddrinfo(serv);
    if (listen(sockfd, 20) == -1) {
        perror("listen");
        exit(1);
    }
    cout << "Server is ready in port " << port_no << endl;

    cout << "server: waiting for connections..." << endl;
    char buf[1024];
    while(1) {
        sin_size = sizeof(their_addr);
        int client_socket = accept(sockfd, (struct sockaddr*) &their_addr, &sin_size);
        if (client_socket == -1) {
            perror("accept");
            continue;
        }
        thread t (connection_handler, client_socket);
        t.detach();
    } 

    }
    else{
        struct addrinfo hints, *res;

        // first, load up address structs with getaddrinfo():
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        int status;
        
        //getaddrinfo("www.example.com", "3490", &hints, &res);
        if ((status = getaddrinfo (host_name.c_str(), port_no.c_str(), &hints, &res)) != 0) {
            cerr << "getaddrinfo: " << gai_strerror(status) << endl;
            exit(-1);
        }

        // make a socket:
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd < 0){
            perror ("Cannot create socket");	
            exit(-1);
        }

        // connect!
        if (connect(sockfd, res->ai_addr, res->ai_addrlen)<0){
            perror ("Cannot Connect");
            exit(-1);
        }
        //
        cout << "Connected " << endl;
        // now it is time to free the memory dynamically allocated onto the "res" pointer by the getaddrinfo function
        freeaddrinfo (res); 
        // return 0   
    }
}

TCPRequestChannel::TCPRequestChannel (int sung){
    sockfd = sung;
}

TCPRequestChannel::~TCPRequestChannel (){
    close(sockfd);
}

int TCPRequestChannel::cread (void* msgbuf, int buflen){
    return recv(sockfd,msgbuf, buflen,0);
}

int TCPRequestChannel::cwrite(void* msgbuf, int msglen){
    return send(sockfd,msgbuf, msglen,0);
}

int TCPRequestChannel::getfd(){
    return sockfd;
}


