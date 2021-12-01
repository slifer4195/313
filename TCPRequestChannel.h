#include <string>
#include "common.h"


class TCPRequestChannel{
private:
     
    int sockfd;
public:
    
   TCPRequestChannel (const string host_name, const string port_no);

   TCPRequestChannel (int);

   ~TCPRequestChannel();
   int cread (void* msgbuf, int buflen);
   int cwrite(void* msgbuf, int msglen);
   int getfd();
};
