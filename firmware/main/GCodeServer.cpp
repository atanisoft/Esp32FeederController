#include <asio.hpp>
#include <esp_netif.h>
#include <esp_ota_ops.h>
#include <functional>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>
#include "GCodeServer.hxx"
#include "Utils.hxx"

GCodeServer::GCodeServer(asio::io_context &context,
                         const esp_ip4_addr_t local_addr,
                         const uint16_t port)
    : acceptor_(context, tcp::endpoint(tcp::v4(), port)),
      clientManager_(context)
{
    auto endpoint = acceptor_.local_endpoint();
    ESP_LOGI(TAG, "Waiting for connections on " IPSTR ":%d...",
             IP2STR(&local_addr), endpoint.port());
    do_accept();
}

void GCodeServer::register_command(std::string const &command,
                                   command_handler &&method)
{
    dispatcher_.emplace(command, std::move(method));
}

void GCodeServer::do_accept()
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

GCodeServer::GCodeClientManager::GCodeClientManager(asio::io_context &context)
    : timer_(context, std::chrono::seconds(1))
{
    report_client_count({});
}

void GCodeServer::GCodeClientManager::start(std::shared_ptr<GCodeClient> client)
{
    clients_.insert(client);
    client->start();
}

void GCodeServer::GCodeClientManager::stop(std::shared_ptr<GCodeClient> client)
{
    clients_.erase(client);
    client->stop();
}

void GCodeServer::GCodeClientManager::report_client_count(const asio::error_code &ec)
{
    if (!ec)
    {
        ESP_LOGI(TAG, "%zu clients connected", clients_.size());
        timer_.expires_from_now(std::chrono::seconds(30));
        timer_.async_wait(
            std::bind(&GCodeServer::GCodeClientManager::report_client_count,
                      this, std::placeholders::_1));
    }
}

GCodeServer::GCodeClient::GCodeClient(tcp::socket &&socket,
                                      GCodeClientManager &manager,
                                      dispatcher_type dispatcher)
    : socket_(std::move(socket)),
      manager_(manager), dispatcher_(dispatcher),
      peer_(socket_.remote_endpoint().address().to_string())
{
}

void GCodeServer::GCodeClient::start()
{
    read();
}

void GCodeServer::GCodeClient::stop()
{
    ESP_LOGI(CLIENT_TAG, "Closing connection from %s", peer_.c_str());
    socket_.close();
}

void GCodeServer::GCodeClient::read()
{
    ESP_LOGD(CLIENT_TAG, "[%s] Waiting for data", peer_.c_str());
    asio::async_read_until(socket_, streambuf_, EOL,
                           std::bind(&GCodeClient::on_read, shared_from_this(),
                                     std::placeholders::_1, std::placeholders::_2));
}

void GCodeServer::GCodeClient::write()
{
    ESP_LOGD(CLIENT_TAG, "[%s] Sending:%s", peer_.c_str(), outgoing_.front().c_str());
    asio::async_write(socket_, asio::buffer(outgoing_.front()),
                      std::bind(&GCodeClient::on_write, shared_from_this(),
                                std::placeholders::_1, std::placeholders::_2));
}

void GCodeServer::GCodeClient::on_read(asio::error_code error, std::size_t size)
{
    if (!error)
    {
        std::istream stream(&streambuf_);
        std::string line;
        std::getline(stream, line);
        streambuf_.consume(size);

        string_trim(line);
        if (!line.empty())
        {
            ESP_LOGD(CLIENT_TAG, "[%s] Received:%s", peer_.c_str(), line.c_str());
            process_line(line);
        }
    }
    else if (error != asio::error::operation_aborted)
    {
        ESP_LOGE(CLIENT_TAG, "[%s] Read was unsuccessful: %s (%d)",
                 peer_.c_str(), error.message().c_str(), error.value());
        manager_.stop(shared_from_this());
    }
}

void GCodeServer::GCodeClient::on_write(asio::error_code error, std::size_t size)
{
    if (!error)
    {
        ESP_LOGD(CLIENT_TAG, "[%s] Write successful", peer_.c_str());
        outgoing_.pop();

        if (!outgoing_.empty())
        {
            write();
        }
        else
        {
            read();
        }
    }
    else if (error != asio::error::operation_aborted)
    {
        ESP_LOGE(CLIENT_TAG, "[%s] Write was unsuccessful: %s (%d)",
                 peer_.c_str(), error.message().c_str(), error.value());
        manager_.stop(shared_from_this());
    }
}

void GCodeServer::GCodeClient::process_line(std::string &line)
{
    std::vector<std::string> args;
    // break the string on the first ; character
    tokenize(break_string(line, ";").first, args);
    std::string command = std::move(args[0]);
    args.erase(args.begin());

    std::string reply;

    ESP_LOGI(CLIENT_TAG, "[%s] Command:%s", peer_.c_str(), command.c_str());

    if (auto it = dispatcher_.find(command); it != dispatcher_.cend())
    {
        ESP_LOGD(CLIENT_TAG, "[%s] Found command:%s", peer_.c_str(), command.c_str());
        auto const &method = it->second;

        auto response = method(args);
        reply = response.first ? COMMAND_OK : COMMAND_ERROR;
        reply.reserve(response.second.length() + reply.length() + 2);
        reply.append(" ");
        reply.append(response.second);
    }
    else
    {
        if (command.at(0) == 'G' || command == "M82" ||
            command == "M204" || command == "M400")
        {
            // automatic discard of certain commands that are not implemented
            // or needed with the feeder.
            reply = COMMAND_OK;
            reply.append(" ; not implemented");
        }
        else if (command == "M115")
        {
            const esp_app_desc_t *app_data = esp_ota_get_app_description();
            // If the command is M115 send back firmware details.
            reply = COMMAND_OK;
            reply.reserve(128);
            reply.append(" ");
            reply.append("FIRMWARE_NAME:Esp32SlottedFeeder (");
            reply.append(app_data->version);
            reply.append(")");
        }
        else
        {
            reply = "error invalid command token: ";
            reply.append(command);
        }
    }
    reply.append("\n");
    outgoing_.push(reply);

    // If we have only one entry in the queue, send it now.
    if (outgoing_.size() == 1)
    {
        write();
    }
}