#ifndef __INTERFACE_HPP__
#define __INTERFACE_HPP__

// 与 socket 和 msgpack 相关的头文件
#include<boost/asio/io_service.hpp>
#include<boost/asio/ip/tcp.hpp>
#include<boost/bind.hpp>
#include<boost/shared_ptr.hpp>
#include<boost/enable_shared_from_this.hpp>

#include<boost/asio/streambuf.hpp>

#include<boost/asio/placeholders.hpp>
#include<boost/asio.hpp>
using boost::asio::ip::tcp;
using boost::asio::ip::address;
#include <msgpack.hpp>


#include<string>
#include<iostream>

// 双方需要约定的特殊常量
#define NOTAPPLICATED -3000
#define MAXPACKSIZE 1024

// 函数映射表
#define ADD 1
#define MINUS 2
#define MULTI 3
#define DIV 4


// 服务端发送给客户端的报文格式：1 个整数序列化之后的 msgpack 包

// 服务端类
class session
    :   public boost::enable_shared_from_this<session> {
public:
    session(boost::asio::io_service &io_service) : io_service_(io_service),socket_(io_service)
    {
        buffer = std::make_shared<std::array<char, MAXPACKSIZE>>();         // 初始化 buffer
        *len_ = '\0';
        *opt_ = '\0';
    }

    void start() {
        static tcp::no_delay option(true);      // 设置 socket 为无时延模式
        socket_.set_option(option);
        start_chains();                         // 开始 读函数映射 -> 读 msgpack 长度 -> 读 msgpack -> 读函数映射 的循环
    }

    tcp::socket& socket() {
        return socket_;
    }

private:
    void start_chains()
    {
        read_opt();
    }
    void read_opt() // 读函数映射
    {
        auto self = this->shared_from_this();
        auto async_buffer = buffer;

        boost::asio::async_read(socket_, boost::asio::buffer(opt_, 4),
            [this, self, async_buffer](const boost::system::error_code& ec, std::size_t size)
            {
                std::cout << socket_.remote_endpoint().address() << ":" << socket_.remote_endpoint().port() << " opt data received" << std::endl;
                std::cout << socket_.remote_endpoint().address() << ":" << socket_.remote_endpoint().port() << " original data " << opt_ << std::endl;

                opt = boost::asio::detail::socket_ops::network_to_host_long(int(*(int *)opt_));
                std::cout << socket_.remote_endpoint().address() << ":" << socket_.remote_endpoint().port() << " opt " << opt <<std::endl;

                read_msgpack_len();
            });

    }

    void read_msgpack_len() // 读 msgpack 长度
    {
        auto self = this->shared_from_this();
        auto async_buffer = buffer;

        boost::asio::async_read(socket_, boost::asio::buffer(len_, 4),
            [this, self, async_buffer](const boost::system::error_code& ec, std::size_t size)
            {


                std::cout << socket_.remote_endpoint().address() << ":" << socket_.remote_endpoint().port() << " len data received" << std::endl;
                std::cout << socket_.remote_endpoint().address() << ":" << socket_.remote_endpoint().port() << " original data " << len_ << std::endl;

                len = boost::asio::detail::socket_ops::network_to_host_long(int(*(int*)len_));
                std::cout << socket_.remote_endpoint().address() << ":" << socket_.remote_endpoint().port() << " len " << opt << std::endl;

                read_msgpack();
            });
        //io_service_.run();

    }

    void read_msgpack() // 读 msgpack 包
    {
        auto self = this->shared_from_this();
        auto async_buffer = buffer;

        boost::asio::async_read(socket_, boost::asio::buffer(async_buffer->data(), len),
            [this, self, async_buffer](const boost::system::error_code& ec, std::size_t size)
            {
                if (ec)
                {
                    std::cout << ec.message() << std::endl;
                    return;
                }

                std::cout << socket_.remote_endpoint().address() << ":" << socket_.remote_endpoint().port() << " magpack data received" << std::endl;
                std::cout << socket_.remote_endpoint().address() << ":" << socket_.remote_endpoint().port() << " original data " << async_buffer->data() << std::endl;


                msg = msgpack::unpack(async_buffer->data(), len);

                rpc_caculate_return();
            });

        //io_service_.run();
    }

    void rpc_caculate_return()          // 将计算结果返回
    {
        std::cout << "enter rpc_caculate_return" << std::endl;
        auto tp = msg.get().as<std::tuple<int, int> >();
        std::cout << socket_.remote_endpoint().address() << ":" << socket_.remote_endpoint().port() << " magpack " << std::get<0>(tp) << " " << std::get<1>(tp) << std::endl;


        int result = 0;
        switch (opt)
        {
        case ADD:
            result = std::get<0>(tp) + std::get<1>(tp);
            break;

        case MINUS:
            result = std::get<0>(tp) - std::get<1>(tp);
            break;

        case MULTI:
            result = std::get<0>(tp) * std::get<1>(tp);
            break;
        case DIV:
            if (std::get<1>(tp) == 0)
                result = NOTAPPLICATED;
            result = std::get<0>(tp) / std::get<1>(tp);
            break;

        }
        auto self = this->shared_from_this();
        auto async_buffer = buffer;

        std::tuple<int>  src(result);
        std::stringstream sbuffer;
        msgpack::pack(sbuffer, src);

        std::string strbuff(sbuffer.str());

        memcpy(async_buffer->data(), strbuff.data(), strbuff.size());

        std::cout << socket_.remote_endpoint().address() << ":" << socket_.remote_endpoint().port() << " rpc calculation completed" << std::endl;
        boost::asio::async_write(socket_, boost::asio::buffer(async_buffer->data(), async_buffer->size()),//
            [this, self, async_buffer](const boost::system::error_code& ec, std::size_t size)
            {
                if (ec)
                {
                    std::cout << ec.message() << std::endl;
                    return;
                }
                std::cout << socket_.remote_endpoint().address()<< ":" <<socket_.remote_endpoint().port()<<" rpc success" << std::endl;

                //io_service_.stop();
            });
        //io_service_.run();

    }

private:
    boost::asio::io_service& io_service_;
    tcp::socket socket_;
    boost::asio::streambuf sbuf_;
    std::shared_ptr<std::array<char, MAXPACKSIZE>> buffer;
    char len_[4];
    int len;
    char opt_[4];
    int opt;
    msgpack::object_handle  msg;
};

typedef boost::shared_ptr<session> session_ptr;
class server {
public:
    server(boost::asio::io_service& io_service, tcp::endpoint& endpoint)
        : io_service_(io_service), acceptor_(io_service, endpoint)
    {
        session_ptr new_session(new session(io_service_));
        acceptor_.async_accept(new_session->socket(),           // 异步接受连接
            boost::bind(&server::handle_accept,                 // 若有连接进入就调用成员函数 handle_accept()
                this,
                new_session,
                boost::asio::placeholders::error));
    }

    void handle_accept(session_ptr new_session, const boost::system::error_code& error) {
        if (error) {
            return;
        }

        new_session->start();       // 处理本次连接

        new_session.reset(new session(io_service_));            // 重置 io_service
        acceptor_.async_accept(new_session->socket(), boost::bind(&server::handle_accept, this, new_session,
            boost::asio::placeholders::error));                 // 异步接受连接

        io_service_.run();
    }

    void run() {
        io_service_.run();
    }

private:
    boost::asio::io_service& io_service_;
    tcp::acceptor acceptor_;
};
#endif