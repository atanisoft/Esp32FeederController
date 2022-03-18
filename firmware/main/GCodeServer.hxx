#pragma once

#include <asio.hpp>
#include <functional>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>
#include "Utils.hxx"

class GCodeServer
{
    using tcp = asio::ip::tcp;

    /// Type for the command dispatcher collection.
    using dispatcher_type =
        std::map<std::string, std::function<std::string(std::vector<std::string> const &)>>;

public:
    GCodeServer(const GCodeServer&) = delete;
    GCodeServer& operator=(const GCodeServer&) = delete;

    GCodeServer(asio::io_context &io_context, uint16_t port = DEFAULT_PORT)
        : clientManager_(), acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        do_accept();
    }

    template <typename F>
    void register_command(std::string const &command, F &&method)
    {
        dispatcher_.emplace(command, std::move(method));
    }

private:
    /// Log tag to use for this class.
    static constexpr const char *TAG = "gcode_server";

    /// Port number to listen on for incoming connections from OpenPnP.
    static constexpr uint16_t DEFAULT_PORT = 8989;

    class GCodeClient;

    class ClientManager
    {
        public:
            ClientManager(const ClientManager&) = delete;
            ClientManager& operator=(const ClientManager&) = delete;

            /// Construct a connection manager.
            ClientManager()
            {
            }

            void start(std::shared_ptr<GCodeClient> client)
            {
                clients_.insert(client);
                client->start();
            }
            void stop(std::shared_ptr<GCodeClient> client)
            {
                clients_.erase(client);
                client->stop();
            }
        private:
            /// Collection of connected clients.
            std::set<std::shared_ptr<GCodeClient>> clients_;
    } clientManager_;

    class GCodeClient
        : public std::enable_shared_from_this<GCodeClient>
    {
        using tcp = asio::ip::tcp;

    public:
        GCodeClient(const GCodeClient&) = delete;
        GCodeClient& operator=(const GCodeClient&) = delete;

        GCodeClient(tcp::socket &&socket,
                    ClientManager &manager,
                    dispatcher_type dispatcher)
            : socket_(std::move(socket)),
              manager_(manager), dispatcher_(dispatcher),
              peer_(socket_.remote_endpoint().address().to_string())
        {
        }

        void start()
        {
            read();
        }

        void stop()
        {
            ESP_LOGI(CLIENT_TAG, "Closing connection from %s", peer_.c_str());
            socket_.close();
        }

    private:
        /// Log tag to use for this class.
        static constexpr const char *CLIENT_TAG = "gcode_client";

        /// End of line character to look for when receiving data.
        static constexpr const char EOL = '\n';

        /// Socket connected to the remote peer.
        tcp::socket socket_;

        /// Client Manager.
        ClientManager &manager_;

        /// Collection of supported commands.
        dispatcher_type dispatcher_;

        /// String format of the remote peer's address.
        std::string peer_;

        /// Buffer to use for receiving data from the peer.
        asio::streambuf streambuf_;

        /// Queue used for outbound responses to the peer.
        std::queue<std::string> outgoing_;

        void read()
        {
            ESP_LOGD(CLIENT_TAG, "[%s] Waiting for data", peer_.c_str());
            asio::async_read_until(socket_, streambuf_, EOL,
                                   std::bind(&GCodeClient::on_read, shared_from_this(),
                                             std::placeholders::_1, std::placeholders::_2));
        }

        void write()
        {
            ESP_LOGD(CLIENT_TAG, "[%s] Sending:%s", peer_.c_str(), outgoing_.front().c_str());
            asio::async_write(socket_, asio::buffer(outgoing_.front()),
                              std::bind(&GCodeClient::on_write, shared_from_this(),
                                        std::placeholders::_1, std::placeholders::_2));
        }

        void on_read(asio::error_code error, std::size_t received_size)
        {
            if (!error)
            {
                std::istream stream(&streambuf_);
                std::string line;
                std::getline(stream, line);
                streambuf_.consume(received_size);

                string_trim(line);
                if (!line.empty())
                {
                    ESP_LOGD(CLIENT_TAG, "[%s] Received:%s", peer_.c_str(), line.c_str());
                    process_line(line);
                }
            }
            else if(error != asio::error::operation_aborted)
            {
                manager_.stop(shared_from_this());
            }
        }

        void on_write(asio::error_code error, std::size_t bytes_transferred)
        {
            if (!error)
            {
                outgoing_.pop();

                if (!outgoing_.empty())
                {
                    write();
                }
            }
            else if (error != asio::error::operation_aborted)
            {
                manager_.stop(shared_from_this());
            }
        }

        void process_line(std::string &line)
        {
            std::vector<std::string> args;
            // break the string on the first ; character
            tokenize(break_string(line, ";").first, args);
            std::string command = std::move(args[0]);
            args.erase(args.begin());

            if (auto it = dispatcher_.find(command); it != dispatcher_.cend())
            {
                ESP_LOGD(CLIENT_TAG, "[%s] Found command:%s", peer_.c_str(), command.c_str());
                auto const &method = it->second;

                outgoing_.push(method(args));
            }
            else
            {
                ESP_LOGD(CLIENT_TAG, "[%s] Command:%s not found", peer_.c_str(), command.c_str());
                outgoing_.push("error: invalid command token: " + command);
            }

            // If we have only one entry in the queue, send it now.
            if (outgoing_.size() == 1)
            {
                write();
            }
        }
    };

    /// TCP/IP listener that handles accepting new clients.
    tcp::acceptor acceptor_;

    /// Collection of registered commands.
    dispatcher_type dispatcher_;

    /// Starts accepting client connections in an asynchronous manner.
    void do_accept()
    {
        acceptor_.async_accept(
            [&](asio::error_code error, tcp::socket socket)
            {
                if (!error)
                {
                    auto peer = socket.remote_endpoint().address().to_string();
                    ESP_LOGI(TAG, "New client:%s", peer.c_str());
                    clientManager_.start(
                        std::make_shared<GCodeClient>(std::move(socket),
                                                      clientManager_,
                                                      dispatcher_));
                }

                // Wait for another connection
                do_accept();
            });
    }
};