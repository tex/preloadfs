#ifndef DEVICEHTTP_HPP
#define DEVICEHTTP_HPP

#include "Device.hpp"
#include <string>
#include <boost/asio.hpp>

class DeviceHttp : public Device
{
public:
	DeviceHttp();
	bool open(const char *name);
	ssize_t pread(char *buf, size_t len, off_t offset);
	off_t size();
	void cancel();

private:
	void resolve();
	void handleGetSizeReadHeaders(const boost::system::error_code& err);
	void handleGetSizeWriteRequest(const boost::system::error_code& err);
	void handleGetSizeConnect(const boost::system::error_code& err, boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
	void handleGetSizeResolve(const boost::system::error_code& err, boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
	void handleReadContent(const boost::system::error_code& err);
	void parseUrl(const std::string url);

	boost::asio::io_service         m_ioservice;
	boost::asio::ip::tcp::resolver  m_resolver;
	boost::asio::ip::tcp::socket    m_socket;
	boost::asio::streambuf          m_request;
	boost::asio::streambuf          m_response;

	std::string m_server;
	std::string m_path;

	off_t  m_fileSize;
        off_t  m_contentLength;
        char  *m_data;
        size_t m_size;

	// If true, connection has been closed by server. The connection
	// must be restarted in that case.
	//
	bool   m_error;
};

#endif

