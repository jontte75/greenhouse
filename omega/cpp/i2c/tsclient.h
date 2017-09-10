/**
    C++ client example using sockets
    lightly modified and simplified from example at:
             http://www.binarytides.com/code-a-simple-socket-client-class-in-c/
*/

#include<string.h>
#include<string>
#include<sys/socket.h>
#include<arpa/inet.h>
namespace tsc
{

class tsClient
{
private:
    int sock;
    std::string address;
    std::string tskey;
    int port;
    struct sockaddr_in server;
     
public:
    tsClient(std::string apikey);
    ~tsClient();
    bool conn(std::string, int);
    bool send_data(std::string data);
    std::string receive(int);
};

}
