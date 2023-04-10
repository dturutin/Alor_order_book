#include <iostream>
#include <chrono>
#include <thread>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

using boost::asio::ip::tcp;

class SSLClient {
public:
    SSLClient(const std::string& host, const std::string& port)
            : m_host(host), m_port(port), m_ssl_context(boost::asio::ssl::context::tlsv12_client)
    {
        // Set up the SSL context
        m_ssl_context.set_default_verify_paths();
        m_ssl_context.set_verify_mode(boost::asio::ssl::verify_peer);
    }

    void connect() {
        try {
            // Resolve the host name and connect to the server 
            tcp::resolver resolver(m_io_context);
            auto endpoints = resolver.resolve(m_host, m_port);
            boost::asio::connect(m_ssl_socket.next_layer(), endpoints);

            // Perform the SSL handshake
            m_ssl_socket.handshake(boost::asio::ssl::stream_base::client);
        } catch(const boost::system::system_error& e) {
            std::cerr << "Error connecting to server: " << e.what() << std::endl;
            throw;
        }
    }

    std::string request(const std::string& message) {
        try {
            // Send the message
            boost::asio::write(m_ssl_socket, boost::asio::buffer(message));

            // Receive and return the response
            boost::asio::streambuf response_buf;
            boost::asio::read_until(m_ssl_socket, response_buf, "\r\n");
            std::istream response_stream(&response_buf);
            std::string http_version;
            response_stream >> http_version;
            unsigned int status_code;
            response_stream >> status_code;
            std::string status_message;
            std::getline(response_stream, status_message);
            std::string headers;
            while (std::getline(response_stream, headers) && headers != "\r") {
                // do nothing, headers are not used here
            }
            std::stringstream ss;
            ss << response_stream.rdbuf();
            return ss.str();
        } catch(const boost::system::system_error& e) {
            std::cerr << "Error sending message: " << e.what() << std::endl;
            throw;
        }
    }

    void read_stream() {
        try {
            while (true) {
                // Read the stream message
                std::vector<char> message_buffer;
                std::size_t message_size = 0;
                std::size_t read_size = 0;

                // Read the first two bytes of the frame header
                std::vector<char> header(2);
                boost::asio::read(m_ssl_socket, boost::asio::buffer(header), boost::asio::transfer_exactly(2));

                // Extract the opcode and payload length from the header
                bool fin = (header[0] & 0x80) != 0;
                int opcode = header[0] & 0x0F;
                bool mask = (header[1] & 0x80) != 0;
                std::size_t payload_length = header[1] & 0x7F;

                // Handle extended payload lengths
                if (payload_length == 126) {
                    std::vector<char> extended_payload_length(2);
                    boost::asio::read(m_ssl_socket, boost::asio::buffer(extended_payload_length), boost::asio::transfer_exactly(2));
                    payload_length = ntohs(*reinterpret_cast<uint16_t*>(&extended_payload_length[0]));
                } else if (payload_length == 127) {
                    std::vector<char> extended_payload_length(8);
                    boost::asio::read(m_ssl_socket, boost::asio::buffer(extended_payload_length), boost::asio::transfer_exactly(8));
                    payload_length = be64toh(*reinterpret_cast<uint64_t*>(&extended_payload_length[0]));
                }

                // Read the masking key if present
                std::vector<char> masking_key;
                if (mask) {
                    masking_key.resize(4);
                    boost::asio::read(m_ssl_socket, boost::asio::buffer(masking_key), boost::asio::transfer_exactly(4));
                }

                // Read the payload
                while (read_size < payload_length) {
                    std::vector<char> chunk(std::min<std::size_t>(payload_length - read_size, 1024));
                    std::size_t chunk_size = boost::asio::read(m_ssl_socket, boost::asio::buffer(chunk), boost::asio::transfer_at_least(1));
                    read_size += chunk_size;

                    // Unmask the payload if necessary
                    if (mask) {
                        for (std::size_t i = 0; i < chunk_size; ++i) {
                            chunk[i] = chunk[i] ^ masking_key[i % 4];
                        }
                    }

                    message_buffer.insert(message_buffer.end(), chunk.begin(), chunk.end());
                }

                // Convert the message buffer to a string
                std::string message(message_buffer.begin(), message_buffer.end());

                // Print the message
                std::cout << "Stream message: " << message << std::endl << std::endl << std::endl << std::endl;
            }
        } catch (const boost::system::system_error& e) {
            std::cerr << "Error reading stream: " << e.what() << std::endl;
            throw;
        }
    }



    std::string get_depth_snapshot(){
        try {
            // Create a TCP resolver and connect to the server
            boost::asio::io_context io_context;
            tcp::resolver resolver(io_context);
            tcp::socket snapshot_socket(io_context);
            boost::asio::connect(snapshot_socket, resolver.resolve("fapi.binance.com", "443"));
            std::string request = "GET /fapi/v1/depth?symbol=BTCUSDT&limit=1000 HTTP/1.1\r\n";
            request += "Host: fapi.binance.com\r\n";
            request += "User-Agent: CppHttpClient\r\n";
            request += "Connection: close\r\n\r\n";

            // Send the request and read the response
            boost::asio::write(snapshot_socket, boost::asio::buffer(request));
            boost::asio::streambuf response_buffer;
            boost::asio::read_until(snapshot_socket, response_buffer, "\r\n\r\n");
            std::string response(boost::asio::buffers_begin(response_buffer.data()), boost::asio::buffers_end(response_buffer.data()));

            // Parse the HTTP response headers
            std::size_t content_length = 0;
            std::stringstream ss(response);
            std::string line;
            while (std::getline(ss, line) && line != "\r") {
                if (line.substr(0, 16) == "Content-Length: ") {
                    content_length = std::stoi(line.substr(16));
                }
            }

            // Read the response body
            std::vector<char> response_body(content_length);
            boost::asio::read(snapshot_socket, boost::asio::buffer(response_body), boost::asio::transfer_exactly(content_length));

            // Convert the response body to a string and return it
            std::string snapshot(response_body.begin(), response_body.end());
            return snapshot;
        } catch (const boost::system::system_error& e) {
            std::cerr << "Error getting depth snapshot: " << e.what() << std::endl;
            throw;
        }
    }


private:
    std::string m_host;
    std::string m_port;
    boost::asio::io_context m_io_context;
    boost::asio::ssl::context m_ssl_context;
    tcp::socket m_socket{m_io_context};
    boost::asio::ssl::stream<tcp::socket&> m_ssl_socket{m_socket, m_ssl_context};
};

int main() {
    SSLClient client("fstream.binance.com", "443");
    try {
        client.connect();

        // Send the websocket request
        std::string request = "GET /stream?streams=btcusdt@depth HTTP/1.1\r\n";
        request += "Host: fstream.binance.com\r\n";
        request += "Upgrade: websocket\r\n";
        request += "Connection: Upgrade\r\n";
        request += "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n";
        request += "Sec-WebSocket-Version: 13\r\n\r\n";
        client.request(request);

        // Print the server response
//        std::cout << "Response: " << response << std::endl;

        // Read and print stream responses until app is terminated
        client.get_depth_snapshot();
        client.read_stream();
        
    } catch(...) {
        std::cerr << "An error occurred, exiting." << std::endl;
        return 1;
    }

    return 0;
}