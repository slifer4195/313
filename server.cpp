#include <cassert>
#include <cstring>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <sys/types.h>
#include <sys/stat.h>

#include <thread>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <vector>
#include <math.h>
#include <unistd.h>
#include "TCPRequestChannel.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <thread>

using namespace std;


int buffercapacity = MAX_MESSAGE;
char* buffer = NULL; // buffer used by the server, allocated in the main


int nchannels = 0;
pthread_mutex_t newchannel_lock;
void handle_process_loop(TCPRequestChannel *_channel);
char ival;
vector<string> all_data [NUM_PERSONS];
vector<thread*> channel_threads;



void populate_file_data (int person){
	//cout << "populating for person " << person << endl;
	string filename = "BIMDC/" + to_string(person) + ".csv";
	char line[100];
	ifstream ifs (filename.c_str());
	if (ifs.fail()){
		EXITONERROR ("Data file: " + filename + " does not exist in the BIMDC/ directory");
	}
	int count = 0;
	while (!ifs.eof()){
		line[0] = 0;
		ifs.getline(line, 100);
		if (ifs.eof())
			break;
		double seconds = stod (split (string(line), ',')[0]);
		if (line [0])
			all_data [person-1].push_back(string(line));
	}
}

double get_data_from_memory (int person, double seconds, int ecgno){
	int index = (int)round (seconds / 0.004);
	string line = all_data [person-1][index]; 
	vector<string> parts = split (line, ',');
	double sec = stod(parts [0]);
	double ecg1 = stod (parts [1]);
	double ecg2 = stod (parts [2]); 
	if (ecgno == 1)
		return ecg1;
	else
		return ecg2;
}

void process_file_request (TCPRequestChannel* rc, Request* request){
	
	FileRequest f = *(FileRequest *) request;
	string filename = (char*) request + sizeof (FileRequest);
	if (filename.empty()){
		Request r (UNKNOWN_REQ_TYPE);
		rc->cwrite (&r, sizeof (r));
		return;
	}

	filename = "BIMDC/" + filename; 
	int fd = open (filename.c_str(), O_RDONLY);
	if (fd < 0){
		cerr << "Server received request for file: " << filename << " which cannot be opened" << endl;
		Request r (UNKNOWN_REQ_TYPE);
		rc->cwrite (&r, sizeof (r));
		return;
	}

	if (f.offset == 0 && f.length == 0){ // means that the client is asking for file size
		int64 fs = lseek (fd, 0, SEEK_END);
		rc->cwrite ((char *)&fs, sizeof (int64));
		close (fd);
		return;
	}

	/* request buffer can be used for response buffer, because everything necessary have
	been copied over to filemsg f and filename*/
	char* response = (char*) request; 

	// make sure that client is not requesting too big a chunk
	if (f.length > buffercapacity){
		cerr << "Client is requesting a chunk bigger than server's capacity" << endl;
		Request r (UNKNOWN_REQ_TYPE);
		rc->cwrite (&r, sizeof (r));
		return;
	}
	
	lseek (fd, f.offset, SEEK_SET);
	int nbytes = read (fd, response, f.length);
	/* making sure that the client is asking for the right # of bytes,
	this is especially imp for the last chunk of a file when the 
	remaining lenght is < buffercap of the client*/
	if (nbytes != f.length){
		cerr << "The server received an incorrect length in the filemsg field" << endl;
		Request r (UNKNOWN_REQ_TYPE);
		rc->cwrite (&r, sizeof (r));
		close (fd);
		return;
	}
	rc->cwrite (response, nbytes);
	close (fd);
}

void process_data_request (TCPRequestChannel* rc, Request* r){
	DataRequest* d = (DataRequest* ) r;
	
	if (d->person < 1 || d->person > 15 || d->seconds < 0 || d->seconds >= 60.0 || d->ecgno <1 || d->ecgno > 2){
		cerr << "Incorrectly formatted data request" << endl;
		Request resp (UNKNOWN_REQ_TYPE);
		rc->cwrite (&resp, sizeof (Request));
		return;
	}
	double data = get_data_from_memory (d->person, d->seconds, d->ecgno);
	rc->cwrite(&data, sizeof (double));
}

void process_unknown_request(TCPRequestChannel *rc){
	Request resp (UNKNOWN_REQ_TYPE);
	rc->cwrite (&resp, sizeof (Request));
}


void process_request(TCPRequestChannel *rc, Request* r){
	if (r->getType() == DATA_REQ_TYPE){
		usleep (rand () % 5000);
		process_data_request (rc, r);
	}
	else if (r->getType() == FILE_REQ_TYPE){
		process_file_request (rc, r);
	}
	else{
		process_unknown_request(rc);
	}
}

void handle_process_loop(TCPRequestChannel *channel){
	/* creating a buffer per client to process incoming requests
	and prepare a response */
	char* buffer = new char [buffercapacity];
	if (!buffer){
		EXITONERROR ("Cannot allocate memory for server buffer");
	}
	while (true){
		int nbytes = channel->cread (buffer, buffercapacity);
		if (nbytes < 0){
			cerr << "Client-side terminated abnormally" << endl;
			break;
		}else if (nbytes == 0){
			cerr << "ERROR: Server could not read anything... closing connection" << endl;
			break;
		}
		Request* r = (Request *) buffer;
		if (r->getType() == QUIT_REQ_TYPE){
			//cout << "Connection closed by client" << endl;
			break;
		}
		process_request(channel, r);
	}
	delete [] buffer;
	delete channel;
}

int main(int argc, char *argv[]){
	
	buffercapacity = MAX_MESSAGE;

	int opt;
	string port = "";

	while ((opt = getopt(argc, argv, "m:r:")) != -1) {
		switch (opt) {
			case 'm':
				buffercapacity = atoi (optarg);
				break;
			case 'r':
				port = std::string(optarg);
				break;
			// case
		}
	}
	srand(time_t(NULL));
	for (int i=0; i<NUM_PERSONS; i++){
		populate_file_data(i+1);
	}
	int sockfd, new_fd;
    struct addrinfo hints, *serv;
    struct sockaddr_storage client_addr;
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, port.c_str(), &hints, &serv)) != 0) {
        cerr << "getaddrinfo: " << gai_strerror(rv) << endl;
    }
    if ((sockfd = socket(serv->ai_family, serv->ai_socktype, serv->ai_protocol)) == -1) {
        perror("server: socket");
        return -1;
    }
    if (bind(sockfd, serv->ai_addr, serv->ai_addrlen) == -1) {
        close(sockfd);
        perror("server: bind");
        return -1;
    }
    freeaddrinfo(serv);
    if (listen(sockfd, 20) == -1) {
        perror("listen");
        exit(1);
    }

	// needs more work
	cout << "waiting for connection..." << endl;
    // char buf[1024];//never used but in the slide
    while (1) {
        sin_size = sizeof(client_addr);
		
        int client_socket = accept(sockfd, (struct sockaddr*)&client_addr, &sin_size);
        if (client_socket == -1) {
            perror("accept");
            continue;
        }

        TCPRequestChannel* control = new TCPRequestChannel(client_socket); //may not need
        thread *t = new thread(handle_process_loop, control);

        channel_threads.push_back(t);
        t->detach();

        cout << "Connection with " << control->getfd() << " opened by client " << endl;
    }

}