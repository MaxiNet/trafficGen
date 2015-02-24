//
//  main.cpp
//  trafficServer2
//
//  Created by Arne Schwabe on 22.01.15.
//  Copyright (c) 2015 Uni Paderborn. All rights reserved.
//

#include <iostream>
#include <boost/asio.hpp>

class tcp_connection
: public std::enable_shared_from_this<tcp_connection>
{
public:
    typedef std::shared_ptr<tcp_connection> pointer;

    tcp_connection(boost::asio::ip::tcp::socket socket)
    : socket_(std::move(socket))
    {
    }


    void start()
    {
        auto self(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(data_, max_length),
                                [this, self](boost::system::error_code ec, std::size_t length)
                                {
                                    if (data_[length-1]=='1')
                                        socket_.close();
                                    else
                                        start();
                                });
        
    }


    void handle_read(boost::system::error_code ec, std::size_t length)
    {

    }

private:


    void handle_write(const boost::system::error_code& /*error*/,
                      size_t /*bytes_transferred*/)
    {

    }
    
    boost::asio::ip::tcp::socket socket_;
    std::string message_;

    enum { max_length = 64*1024 };
    char data_[max_length];
};



class traffgen_server
{
public:
    traffgen_server(boost::asio::io_service& io_service, const int port)
    : acceptor_(io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
     socket_(io_service)
    {
        do_accept();
    }

private:
    void do_accept()
    {
        acceptor_.async_accept(socket_,
                               [this](boost::system::error_code ec)
                               {
                                   if (!ec)
                                   {
                                       std::make_shared<tcp_connection>(std::move(socket_))->start();
                                   }
                                   
                                   do_accept();
                               });
    }

    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::ip::tcp::socket socket_;
};


int main(int argc, const char * argv[]) {
    try
    {
        boost::asio::io_service io_service;
        traffgen_server server_leet(io_service, 13373);
        traffgen_server server_moreleet(io_service, 13378);
        io_service.run();
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
    
    return 0;
}
