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

// 客户端发送给服务端的报文格式： 4 字节的整数代表函数映射表，
//                             4 字节的整数表示 msgpack 的长度，后面不定长的部分为 msgpack 包。
// 服务端发送给客户端的报文格式：1 个整数序列化之后的 msgpack 包


// 客户端类
class client : public boost::enable_shared_from_this<client> {
public:
    client(boost::asio::io_service& io_service, tcp::endpoint& endpoint)
        : io_service_(io_service), socket_(io_service), endpoint_(endpoint)
    {
        buffer = std::make_shared<std::array<char, MAXPACKSIZE>>(); // 初始化 buffer
        result = 0;
    }

    int add(int a,int b)        // 调用 add 和调用 start 的区别在于少传入一个代表函数映射的参数 opt
    {
        return start(ADD,a,b);
    }
    int minus(int a, int b)
    {
        return start(MINUS, a, b);
    }
    int multi(int a, int b)
    {
        return start(MULTI, a, b);
    }
    int div(int a, int b)
    {
        return start(DIV, a, b);
    }
    
    int start(int opt, int a, int b) { // 开始 RPC
        boost::system::error_code ec;
        socket_.connect(endpoint_, ec); // 连接服务器
        if (!ec)
        {
            static tcp::no_delay option(true);
            socket_.set_option(option); // 设置 socket 为无延时 socket

            construct_rpc_data(opt, a , b); // 构造 RPC 包，并将包存入 buffer 成员变量中
            send_recive_rpc_data(ec); // 发送需求包，并且接受客户端的结果
            std::cout << "send_recive_rpc_data return value: " << result << std::endl;
            return result; // 将结果返回
        }

        else
        {
            std::cerr << boost::system::system_error(ec).what() << std::endl;
        }
        return NOTAPPLICATED;
    }

private:
    void construct_rpc_data(size_t opt, int a, int b)
    {
        std::cout<< " opt " << opt << std::endl; // 根据协议首先需要传输的是函数映射
        size_t opt_bigend = boost::asio::detail::socket_ops::host_to_network_long(opt);
        memcpy(buffer->data(), &opt_bigend, 4); // 将函数映射存储在 buffer 的最前面

        std::tuple<int, int>  src(a,b); // 将需要发送给服务端的参数封装在 truple 里面
        std::stringstream sbuffer;
        msgpack::pack(sbuffer, src); // 序列化
        std::string strbuf(sbuffer.str()); // 转化为 std::string 类型方便传输

        std::cout << " len " << strbuf.size() << std::endl;
        size_t len_bigend = boost::asio::detail::socket_ops::host_to_network_long(strbuf.size());
        memcpy(buffer->data()+4, &len_bigend, 4); // 将 msgpack 包长度写到函数映射之后
        memcpy(buffer->data() + 8, strbuf.data(), strbuf.size()); // 将 msgpack 包写入 buffer
    }
    void send_recive_rpc_data(const boost::system::error_code& error) {

        auto self = this->shared_from_this();
        auto async_buffer = buffer;


        boost::asio::async_write(socket_, boost::asio::buffer(*async_buffer, MAXPACKSIZE), // 异步将信息发送给服务端
            [this,self, async_buffer](const boost::system::error_code& ec, std::size_t size)
            {
                recive_rpc_data(ec);  // 当执行这一句的时候，数据以及发送给服务端了，调用本函数来接受服务端的数据

                io_service_.stop();
            });
        io_service_.run();

    }

    void recive_rpc_data(const boost::system::error_code& error) {
        std::cout << "sending finished, start receiving data" << std::endl;
        auto self = this->shared_from_this();
        auto async_buffer = buffer;

        boost::asio::async_read(socket_, boost::asio::buffer(*async_buffer, async_buffer->size()),// 异步读取数据
            [this, self, async_buffer](const boost::system::error_code& ec, std::size_t size)
            {

                std::cout << "data reading completed" << std::endl;
                handle_rpc_data(ec);
                io_service_.stop(); // 手动停止事件循环

            });
        io_service_.run();
        
    }

    void handle_rpc_data(const boost::system::error_code& error) {

        std::cout << "data read: " << buffer->data() << std::endl;
        msgpack::object_handle  msg = msgpack::unpack(buffer->data(), buffer->size());
        auto tp = msg.get().as<std::tuple<int>>();
        std::cout << " msgpack " << std::get<0>(tp) << std::endl;
        result = std::get<0>(tp); // 将结果写入成员变量 result 里面
    }

private:
    boost::asio::io_service& io_service_;
    tcp::socket socket_;
    tcp::endpoint& endpoint_;
    std::shared_ptr<std::array<char, MAXPACKSIZE>> buffer;
    int result;
};

