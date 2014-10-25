//
//  AsyncTCPXMLClient.hpp
//  DXMPP
//
//  Created by Stefan Karlsson 2014
//  Copyright (c) 2014 Deus ex Machinae. All rights reserved.
//

#ifndef DXMPP_AsyncTCPClient_hpp
#define DXMPP_AsyncTCPClient_hpp

#include <DXMPP/pugixml/pugixml.hpp>
#include <DXMPP/Debug/DebugOutputTreshold.hpp>
#include <DXMPP/TLSVerification.hpp>

#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <sstream>

namespace DXMPP
{
    namespace Network
    {
        class AsyncTCPXMLClient
        {
            TLSVerification *TLSConfig;

            DebugOutputTreshold DebugTreshold;
            bool HasUnFetchedXML;

            static const int ReadDataBufferSize = 1024;
            char ReadDataBufferNonSSL[ReadDataBufferSize];
            std::stringstream ReadDataStreamNonSSL;

            char ReadDataBufferSSL[ReadDataBufferSize];
            std::stringstream ReadDataStreamSSL;

            boost::asio::mutable_buffers_1 SSLBuffer;
            boost::asio::mutable_buffers_1 NonSSLBuffer;
        public:

            std::stringstream *ReadDataStream;
            char * ReadDataBuffer;

            enum class ConnectionState
            {
                Connected,
                Disconnected,
                Error
            };



            std::string Hostname;
            int Portnumber;

            bool SSLConnection;
            ConnectionState CurrentConnectionState;

            boost::scoped_ptr<boost::thread> IOThread;
            boost::asio::io_service io_service;
            boost::asio::ip::tcp::socket tcp_socket;
            boost::asio::ssl::context ssl_context;
            boost::asio::ssl::stream<boost::asio::ip::tcp::socket&> ssl_socket;


            void HandleWrite(std::shared_ptr<std::string> Data,
                             const boost::system::error_code &error);

            bool ConnectTLSSocket();
            bool ConnectSocket();

            bool VerifyCertificate(bool preverified,
                                    boost::asio::ssl::verify_context& ctx);

            void WriteXMLToSocket(pugi::xml_document *Doc);
            void WriteTextToSocket(const std::string &Data);

            void HandleRead(char *ActiveDataBuffer,
                            const boost::system::error_code &error,
                            std::size_t bytes_transferred);
            void AsyncRead();

            pugi::xml_document *IncomingDocument;

            bool LoadXML();
            pugi::xml_node SelectSingleXMLNode(const char* xpath);
            pugi::xpath_node_set SelectXMLNodes(const char* xpath);

            std::unique_ptr<pugi::xml_document>  FetchDocument();

            void ClearReadDataStream();

            void ForkIO();


            virtual ~AsyncTCPXMLClient()
            {
                io_service.stop();
                while(!io_service.stopped())
                    boost::this_thread::sleep(boost::posix_time::milliseconds(10));

                if(IOThread != nullptr)
                    IOThread->join();
            }


            typedef boost::function<void (void)> ErrorCallbackFunction;
            typedef boost::function<void (void)> GotDataCallbackFunction;

            ErrorCallbackFunction ErrorCallback;
            GotDataCallbackFunction GotDataCallback;

            AsyncTCPXMLClient( TLSVerification *TLSConfig,
                               const std::string &Hostname,
                               int Portnumber,                               
                               const ErrorCallbackFunction &ErrorCallback,
                               const GotDataCallbackFunction &GotDataCallback,
                               DebugOutputTreshold DebugTreshold = DebugOutputTreshold::Error)
                :
                  TLSConfig(TLSConfig),
                  DebugTreshold(DebugTreshold),
                  HasUnFetchedXML(false),
                  SSLBuffer( boost::asio::buffer(ReadDataBufferSSL, ReadDataBufferSize) ),
                  NonSSLBuffer( boost::asio::buffer(ReadDataBufferNonSSL, ReadDataBufferSize) ),
                  io_service(),
                  tcp_socket(io_service),
                  ssl_context(boost::asio::ssl::context::sslv23),
                  ssl_socket(tcp_socket, ssl_context),
                  IncomingDocument(nullptr),
                  ErrorCallback(ErrorCallback),
                  GotDataCallback(GotDataCallback)

            {                
                ReadDataStream = &ReadDataStreamNonSSL;
                ReadDataBuffer = ReadDataBufferNonSSL;

                memset(ReadDataBufferSSL, 0, ReadDataBufferSize);
                memset(ReadDataBufferNonSSL, 0, ReadDataBufferSize);

                SSLConnection = false;
                CurrentConnectionState = ConnectionState::Disconnected;                

                ssl_socket.set_verify_mode(boost::asio::ssl::verify_peer);
                switch(TLSConfig->Mode)
                {
                case TLSVerificationMode::RFC2818_Hostname:
                    ssl_socket.set_verify_callback(
                        boost::asio::ssl::rfc2818_verification("host.name"));
                    break;
                case TLSVerificationMode::Custom:
                case TLSVerificationMode::None:
                    ssl_socket.set_verify_callback(
                        boost::bind(&AsyncTCPXMLClient::VerifyCertificate,
                                    this,
                                    _1,
                                    _2));
                    break;
                }

                this->Hostname = Hostname;
                this->Portnumber = Portnumber;
            }
        };
    }
}

#endif // DXMPP_AsyncTCPClient_hpp
