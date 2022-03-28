/*
 * SPDX-FileCopyrightText: 2022 Mike Dunston (atanisoft)
 *
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#pragma once

#include <asio.hpp>
#include <esp_netif.h>
#include <functional>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>

class GCodeServer
{
public:

    using command_args = std::vector<std::string> const &;
    using command_return_type = std::pair<bool, std::string>;

private:
    using tcp = asio::ip::tcp;

    /// Type used for the command handlers
    using command_handler = std::function<command_return_type(command_args)>;

    /// Type for the command dispatcher collection.
    using dispatcher_type = std::map<std::string, command_handler>;

public:

    GCodeServer(asio::io_context &context,
                const esp_ip4_addr_t local_addr,
                const uint16_t port = DEFAULT_PORT);

    void register_command(std::string const &command,
                          command_handler &&method);

private:
    /// Log tag to use for this class.
    static constexpr const char * const TAG = "gcode_server";

    /// Port number to listen on for incoming connections from OpenPnP.
    static constexpr uint16_t DEFAULT_PORT = 8989;

    /// Prefix for responses that are successful.
    static constexpr const char * const COMMAND_OK = "ok";

    /// Prefix for responses that contain a failure.
    static constexpr const char * const COMMAND_ERROR = "error";

    GCodeServer(const GCodeServer &) = delete;
    GCodeServer &operator=(const GCodeServer &) = delete;

    /// TCP/IP listener that handles accepting new clients.
    tcp::acceptor acceptor_;

    /// Collection of registered commands.
    dispatcher_type dispatcher_;

    /// Starts accepting client connections in an asynchronous manner.
    void do_accept();

    // Forward declaration of client class
    class GCodeClient;

    /// Lifecycle manager for @ref GCodeClient.
    class GCodeClientManager
    {
    public:
        /// Constructor.
        GCodeClientManager(asio::io_context &context);

        /// Registers and starts a @ref GCodeClient.
        ///
        /// @param client @ref GCodeClient to register and start.
        void start(std::shared_ptr<GCodeClient> client);

        /// Stops a @ref GCodeClient and cleans up resources.
        ///
        /// @param client @ref GCodeClient to stop and cleanup.
        void stop(std::shared_ptr<GCodeClient> client);

    private:
        /// Log tag to use for this class.
        static constexpr const char * const TAG = "gcode_client_mgr";

        /// Collection of connected clients.
        std::set<std::shared_ptr<GCodeClient>> clients_;

        /// Timer used for periodic reporting of client status.
        asio::system_timer timer_;

        /// Reports connected client count and status.
        ///
        /// @param ec @ref asio::error_code status.
        void report_client_count(const asio::error_code &ec);

        GCodeClientManager(const GCodeClientManager &) = delete;
        GCodeClientManager &operator=(const GCodeClientManager &) = delete;
    } clientManager_;

    /// Client implementation that processes incoming GCode commands.
    class GCodeClient
        : public std::enable_shared_from_this<GCodeClient>
    {
        using tcp = asio::ip::tcp;

    public:
        GCodeClient(const GCodeClient &) = delete;
        GCodeClient &operator=(const GCodeClient &) = delete;

        /// Constructor.
        ///
        /// @param socket TCP/IP socket for the remote client.
        /// @param manager @ref GCodeClientManager that manages this client.
        /// @param dispatcher Collection of commands that can be dispatched.
        GCodeClient(tcp::socket &&socket,
                    GCodeClientManager &manager,
                    dispatcher_type dispatcher);

        /// Starts this client.
        void start();

        /// Stops this client.
        void stop();

    private:
        /// Log tag to use for this class.
        static constexpr const char *CLIENT_TAG = "gcode_client";

        /// End of line character to look for when receiving data.
        static constexpr const char EOL = '\n';

        /// Socket connected to the remote peer.
        tcp::socket socket_;

        /// Client Manager.
        GCodeClientManager &manager_;

        /// Collection of supported commands.
        dispatcher_type dispatcher_;

        /// String format of the remote peer's address.
        std::string peer_;

        /// Buffer to use for receiving data from the peer.
        asio::streambuf streambuf_;

        /// Queue used for outbound responses to the peer.
        std::queue<std::string> outgoing_;

        /// Utility function that starts (or restarts) a read operation on the
        /// connected remote client.
        void read();

        /// Utility function that starts writing a response to the connected
        /// remote client.
        void write();

        /// Callback for read completion.
        ///
        /// @param error When non-zero an error occurred, otherwise the read
        /// can be considered successful.
        /// @param size Number of bytes received from the remote client.
        void on_read(asio::error_code error, std::size_t size);

        /// Callback for write completion.
        ///
        /// @param error When non-zero an error occurred, otherwise the write
        /// can be considered successful.
        /// @param size Number of bytes sent to the remote client.
        void on_write(asio::error_code error, std::size_t size);

        /// Processes a single command line received from the remote client.
        ///
        /// @param line Command line received.
        void process_line(std::string &line);
    };
};