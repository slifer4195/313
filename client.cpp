#include "common.h"
#include "BoundedBuffer.h"
#include "Histogram.h"
#include "common.h"
#include "HistogramCollection.h"
#include "TCPRequestChannel.h"

#include <thread>
#include <sys/wait.h>

using namespace std;

struct Response {
    int personVal;
    double ecgVal;
};

void patient_thread_function(int nVal,int pVal, BoundedBuffer* requestBuffer){
    double point1 = 0.00;
    int point2 = 1;
    double incrementD = 0.004;

    DataRequest data(pVal, point1, point2);

    for(int i = 0; i < nVal; i++) {

        requestBuffer -> push((char*) &data, sizeof(DataRequest));
        data.seconds = data.seconds +incrementD;
    }
}

void file_thread_helper(string fileName, int length){
	FILE* fp = fopen(fileName.c_str(), "w");
    fseek(fp, length, SEEK_SET);
    fclose(fp);

}

void file_threader(FileRequest* fileM ,int64_t rem,string fileInput,char* buffer,BoundedBuffer* requestBuffer, int mb ){
    
    while(rem > 0) {

        fileM->length = min(rem, (__int64_t) mb);
        requestBuffer->push(buffer, sizeof(FileRequest) + fileInput.size() + 1);
        fileM->offset = fileM->offset +fileM->length;
        rem = rem - fileM->length;
    }
}

void file_thread_function(BoundedBuffer* requestBuffer, string fileInput,TCPRequestChannel* chan, int mb) {
    
    int buffSize = 1024;
    string fileName = "received/" + fileInput;
    char buffer [buffSize];
    FileRequest file (0,0);

    memcpy(buffer, &file, sizeof(file));
    strcpy(buffer + sizeof(file), fileInput.c_str());

    chan->cwrite(buffer, sizeof(file) + fileInput.size() + 1);
    int64_t length;
    chan->cread(&length, sizeof(length));

    file_thread_helper(fileName, length);

    FileRequest* fileM = (FileRequest*) buffer;
    int64_t rem = length;
    file_threader(fileM ,rem,fileInput,buffer,requestBuffer, mb );
}

void workerQuit(REQUEST_TYPE_PREFIX* mR , TCPRequestChannel* wchannel){
    wchannel->cwrite(mR, sizeof(REQUEST_TYPE_PREFIX));
    delete wchannel;
}
void workerData(DataRequest* dataR, TCPRequestChannel* wchannel, BoundedBuffer* responseBuffer,  char* buffer ){
    wchannel->cwrite(dataR, sizeof(DataRequest));
    double ecgVal;
    wchannel->cread(&ecgVal, sizeof(double));
    Response r{dataR->person, ecgVal}; //this line may need to fix

    responseBuffer->push((char*)&r, sizeof(r));
}

void workerFile(TCPRequestChannel* wchannel, int mb, char* buffer,char* receiveBuffer){
    int adder = 1;
    FileRequest* fileM = (FileRequest*) buffer;
    string fileInput = (char*)(fileM + adder);
    int sizeF = sizeof(FileRequest) + fileInput.size() + adder;

    wchannel->cwrite(buffer, sizeF);
    wchannel->cread(receiveBuffer, mb);

    string fileName = "received/" + fileInput;

    FILE* fileP = fopen(fileName.c_str(), "r+");
    fseek(fileP, fileM->offset, SEEK_SET);

    fwrite(receiveBuffer, 1, fileM->length, fileP);
    fclose(fileP);
}

void worker_thread_function(TCPRequestChannel* wchannel,BoundedBuffer* requestBuffer, BoundedBuffer* responseBuffer, int mb){
    char buffer [1024];
    char receiveBuffer [mb];
    bool run = true;

    while(run) {
        requestBuffer->pop(buffer, sizeof(buffer));
        REQUEST_TYPE_PREFIX* mR = (REQUEST_TYPE_PREFIX* ) buffer;
        if(*mR == QUIT_REQ_TYPE) {
            workerQuit(mR, wchannel);
            break;
        }
        if(*mR == DATA_REQ_TYPE) {
            DataRequest* dataR = (DataRequest*) buffer;
            workerData(dataR,wchannel, responseBuffer, buffer);
        } else if (*mR == FILE_REQ_TYPE) {
            workerFile(wchannel, mb, buffer,receiveBuffer); 
        }
    }
}

void histogram_thread_function (BoundedBuffer* responseBuffer, HistogramCollection* histC) {
    bool run = true;
    char buffer [1024];

    while(run) {
        responseBuffer->pop(buffer,1024);
        Response* resp = (Response*) buffer; 
        if(resp->personVal == -1) {
            break;
        }
        histC->update(resp->personVal, resp->ecgVal); //this needs more
    }
}


int main(int argc, char *argv[]) {
    int opt;
    int n = 1;
    int p = 1;
    int w = 1;
    int b = 1;
	int m = MAX_MESSAGE; 
    int f = 1;
    int h = 10;
    string r = "";
    string host ="";
    srand(time_t(NULL));

    string fileInput = "";
    
    while((opt = getopt(argc, argv, "n:p:w:b:m:f:h:r:")) != -1) {
        switch (opt) {
            case 'n':
                n = atoi(optarg);
                break;
            case 'p':
                p = atoi(optarg);
                break;
            case 'w':
                w = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 'm':
                m = atoi(optarg);
                break;
            case 'f':
                fileInput = optarg;
                break;
            case 'h':
                host = std::string(optarg);
                break;
            case 'r':
                r = std::string(optarg);
                break;
            
        }
    }

    //starer code no work
    
    
    /////////////////////////////////////////////////////
    thread patient [p];
    thread worker [w];
    thread hist [h];
    BoundedBuffer requestBuffer(b);
    BoundedBuffer responseBuffer(b);
	HistogramCollection hc;
	TCPRequestChannel* chan = new TCPRequestChannel(host, r);
    TCPRequestChannel* wchannels [w];

    for(int currW = 0; currW < w; currW++) {
        wchannels [currW] = new TCPRequestChannel(host, r);
    }

    if(fileInput == "") { 
        for(int i =0; i < p; i++) {
            Histogram* hist = new Histogram(10, -2.0, 2.0);
            hc.add(hist);
        }
    }
    
    struct timeval start, end;
    gettimeofday (&start, 0);

    if(fileInput == "") { 
        for(int i = 0; i < p; i++) patient[i] = thread(patient_thread_function, n,i+1, &requestBuffer);
        for(int j = 0; j < w; j++) worker[j] = thread(worker_thread_function, wchannels[j], &requestBuffer, &responseBuffer, m);
        for(int k = 0; k < h; k++) hist[k] = thread(histogram_thread_function, &responseBuffer, &hc);
    }

    else {

        thread filethread(file_thread_function, &requestBuffer,fileInput, chan, m);
        for(int currentW = 0; currentW < w; currentW++) {worker[currentW] = thread(worker_thread_function, wchannels[currentW], &requestBuffer, &responseBuffer, m);}
        
        filethread.join();
    }
    
    
	/* Join all threads here */
    /* Join all threads here */
    if (fileInput == "") {
        for(int currPatient = 0; currPatient < p; currPatient++) patient[currPatient].join();
    }
    //number 2 //number
    for (int i = 0; i<w; i++) {
        REQUEST_TYPE_PREFIX qu = QUIT_REQ_TYPE;
        requestBuffer.push((char*) &qu, sizeof(REQUEST_TYPE_PREFIX));
    }

    for (int currWork = 0; currWork < w; currWork ++)
        worker[currWork].join();

    if (fileInput == "") {
        Response r {-1, 0}; 
        for(int currhist = 0; currhist < h; currhist++) {responseBuffer.push((char*) &r, sizeof(r));}
        for(int currHist = 0; currHist < h; currHist++) {hist[currHist].join();}
    }

    // struct timeVal, start, end    this line useless
    gettimeofday (&end, 0);

	hc.print ();
    int secs = (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)/(int) 1e6;
    int usecs = (int)(end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)%((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

    REQUEST_TYPE_PREFIX q = QUIT_REQ_TYPE;
    chan->cwrite ((char *) &q, sizeof (REQUEST_TYPE_PREFIX));

    cout << "Client process exited" << endl;
   // delete chan;
}
