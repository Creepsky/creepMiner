//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#include "Miner.h"

void Burst::MinerSocket::setRemote(const std::string ip, size_t port,size_t defaultTimeout)
{
	inet_pton(AF_INET, ip.c_str(), &this->remoteAddr.sin_addr);
    this->remoteAddr.sin_family = AF_INET;
    this->remoteAddr.sin_port = htons( (u_short)port );
    
    this->socketTimeout.tv_sec  = (long)defaultTimeout;
	this->socketTimeout.tv_usec = (long)defaultTimeout*1000;
}

std::string Burst::MinerSocket::httpRequest(const std::string method,
                                     const std::string url,
                                     const std::string body,
                                     const std::string header)
{
    std::string response = "";
    memset((void*)this->readBuffer,0,readBufferSize);
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#ifdef WIN32
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&this->socketTimeout.tv_usec, sizeof(this->socketTimeout.tv_usec));
#else
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&this->socketTimeout, sizeof(struct timeval));
#endif
    if(connect(sock, (struct sockaddr*)&this->remoteAddr, sizeof(struct sockaddr_in)) == -1)
    {
        MinerLogger::write("unable to connect to remote host");
    }
    else
    {
        std::string request = method+" ";
        request += url+" HTTP/1.0\r\n";
        request += header+"\r\n\r\n";
        request += body;
        
        if(send(sock, request.c_str(), (int)request.length(),MSG_NOSIGNAL) > 0)
        {
            //shutdown(sock,SHUT_WR);
            
            
            int totalBytesRead = 0;
            int bytesRead = 0;
            do
            {
                bytesRead = (int)recv(sock, &this->readBuffer[totalBytesRead],
                                      readBufferSize-totalBytesRead-1,0);
                if(bytesRead > 0)
                {
                    totalBytesRead+=bytesRead;
                    this->readBuffer[totalBytesRead] = 0;
                    response = std::string((char*)this->readBuffer);
                    totalBytesRead = 0;
                    this->readBuffer[totalBytesRead] = 0;
                }
            }while( (bytesRead > 0) && (totalBytesRead < (int)readBufferSize-1) );
            
            shutdown(sock,SHUT_RDWR);
			closesocket(sock);
            sock = 0;
            
            if(response.length() > 0)
            {
                size_t httpBodyPos = response.find("\r\n\r\n");
                response = response.substr(httpBodyPos+4,response.length()-(httpBodyPos+4));
            }
        }
    }
    
    return response;
}

std::string Burst::MinerSocket::httpPost(const std::string url, const std::string body)
{
    std::string //header = "Content-Type: application/x-www-form-urlencoded\r\n";
                //header = "Content-Length: "+std::to_string(body.length())+"\r\n";
                header = "Connection: close";
    return this->httpRequest("POST",url,body,header);
}

void Burst::MinerSocket::httpRequestAsync(const std::string method,
                                          const std::string url,
                                          const std::string body,
                                          const std::string header,
                                          std::function< void ( std::string ) > responseCallback )
{
    std::string response = httpRequest(method, url, body, header);
    responseCallback(response);
}

void Burst::MinerSocket::httpPostAsync(const std::string url, const std::string body,
                                       std::function< void ( std::string ) > responseCallback)
{
    std::string //header = "Content-Type: application/x-www-form-urlencoded\r\n";
                //header = "Content-Length: "+std::to_string(body.length())+"\r\n";
                header = "Connection: close";
    std::thread requestThread(&MinerSocket::httpRequestAsync,this,"POST",url,body,header,responseCallback);
    requestThread.detach();
}

void Burst::MinerSocket::httpGetAsync(const std::string url,
                                      std::function< void ( std::string ) > responseCallback)
{
    std::string header = "Connection: close";
    std::thread requestThread(&MinerSocket::httpRequestAsync,this,"GET",url,"",header,responseCallback);
    requestThread.detach();
}

std::string Burst::MinerSocket::httpGet(const std::string url)
{
    std::string header = "Connection: close";
    return httpRequest("GET", url, "", header);
}

Burst::MinerProtocol::MinerProtocol()
{
    this->running = false;
    this->currentBaseTarget = 0;
    this->currentBlockHeight = 0;
}

Burst::MinerProtocol::~MinerProtocol()
{
    this->stop();
}

void Burst::MinerProtocol::stop()
{
    this->running = false;
}

bool Burst::MinerProtocol::run(Miner* miner)
{
    const MinerConfig* config = miner->getConfig();
    this->miner = miner;
    std::string remoteIP = MinerProtocol::resolveHostname(config->poolHost);
    if(remoteIP != "")
    {
        this->running = true;
        this->miningInfoSocket.setRemote(remoteIP, config->poolPort);
        this->nonceSubmitterSocket.setRemote(remoteIP, config->poolPort);
        while(this->running)
        {
            this->getMiningInfo();
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
    }
    return false;
}

uint64_t Burst::MinerProtocol::submitNonce(uint64_t nonce, uint64_t accountId)
{
    NxtAddress addr(accountId);
    MinerLogger::write("submitting nonce "+std::to_string(nonce)+" for "+addr.to_string());
    std::string request = "requestType=submitNonce&nonce="+std::to_string(nonce)+"&accountId="+std::to_string(accountId)+"&secretPhrase=cryptoport";
    std::string url = "/burst?"+request;
    
    size_t tryCount = 0;
    std::string response = "";
    do{
        response = this->nonceSubmitterSocket.httpPost(url,"");
        tryCount++;
    }
    while(response.empty() && tryCount < this->miner->getConfig()->submissionMaxRetry);
          
    MinerLogger::write(response);
    rapidjson::Document body;
    body.Parse<0>(response.c_str());
    
    if(body.GetParseError() == nullptr)
    {
        if(body.HasMember("deadline"))
        {
            return body["deadline"].GetUint64();
        }
    }
    return (uint64_t)(-1);
}

void Burst::MinerProtocol::getMiningInfo()
{
    std::string response = this->miningInfoSocket.httpPost("/burst?requestType=getMiningInfo","");
    
    if(response.length() > 0)
    {
        rapidjson::Document body;
        body.Parse<0>(response.c_str());
        
        if(body.GetParseError() == nullptr)
        {
            if(body.HasMember("baseTarget"))
            {
                std::string baseTargetStr = body["baseTarget"].GetString();
                this->currentBaseTarget = std::stoull(baseTargetStr);
            }
            
            if(body.HasMember("generationSignature"))
            {
                this->gensig = body["generationSignature"].GetString();
            }
            
            if(body.HasMember("height"))
            {
                std::string newBlockHeightStr = body["height"].GetString();
                uint64_t newBlockHeight = std::stoull(newBlockHeightStr);
                if(newBlockHeight > this->currentBlockHeight)
                {
                    this->miner->updateGensig(this->gensig, newBlockHeight, this->currentBaseTarget);
                }
                this->currentBlockHeight = newBlockHeight;
            }
        }
    }
}

std::string Burst::MinerProtocol::resolveHostname(const std::string host)
{
    struct hostent *he;
    struct in_addr **addr_list;
    int i;
    
    MinerLogger::write("resolving hostname "+host);
    
    if ( (he = gethostbyname( host.c_str() ) ) == NULL)
    {
        MinerLogger::write("error while resolving hostname");
        return "";
    }
    
    addr_list = (struct in_addr **) he->h_addr_list;
    
	char inetAddrStr[64];
    for(i = 0; addr_list[i] != NULL; i++)
    {
		std::string ip = std::string(inet_ntop(AF_INET, addr_list[i], inetAddrStr,64));
        MinerLogger::write("Remote IP "+ip);
        return ip;
    }
    
    MinerLogger::write("can not resolve hostname "+host);
    return "";
}