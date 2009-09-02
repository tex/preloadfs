#include "DeviceHttp.hpp"
#include <strstream>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <strings.h>

extern bool g_DebugMode;

DeviceHttp::DeviceHttp():
	m_resolver(m_ioservice),
	m_socket(m_ioservice),
	m_error(false),
	m_closed(false)
{
}

void DeviceHttp::cancel()
{
	if (g_DebugMode)
		std::cout << __PRETTY_FUNCTION__ << "\n";

	m_ioservice.stop();
}

bool DeviceHttp::open(const char *url)
{
	if (g_DebugMode)
		std::cout << __PRETTY_FUNCTION__ << "\n";

	parseUrl(url);
	resolve();

	if (g_DebugMode)
		std::cout << " reset/run\n";

	m_ioservice.run();

	return !m_error;
}

off_t DeviceHttp::size()
{
	return m_fileSize;
}

ssize_t DeviceHttp::pread(char *destination, size_t size, off_t start)
{
	if (g_DebugMode)
		std::cout << __PRETTY_FUNCTION__ << std::hex << " size: " << size << ", start: " << start << std::dec << "\n";

	off_t end = start + size - 1;

	if (start >= m_fileSize)
		return 0;

	m_data = destination;
	m_size = size;

	int attempt;
	for (attempt = 1; attempt <= 4; attempt++)
	{
		m_error = false;

		if (g_DebugMode)
			std::cout << " attempt #" << attempt << "\n";

		std::ostream request_stream(&m_request);
		request_stream << "GET " << m_path << " HTTP/1.1\r\n";
		request_stream << "Host: " << m_server << "\r\n";
		request_stream << "Range: bytes=" << start << "-" << end << "\r\n\r\n";

		if (g_DebugMode)
		{
			std::cout << "\n";
			std::cout << "GET " << m_path << " HTTP/1.1\n";
			std::cout << "Host: " << m_server << "\n";
			std::cout << "Range: bytes=" << start << "-" << end << "\n\n";
		}

		if (m_closed)
		{
			if (g_DebugMode)
				std::cout << "Performing reconect...\n";

			// Start an asynchronous resolve to translate the server and
			// service names into a list of endpoints.
			boost::asio::ip::tcp::resolver::query query(m_server, "http");
			m_resolver.async_resolve(query,
						 boost::bind(&DeviceHttp::handleResolve,
							     this,
							     boost::asio::placeholders::error,
							     boost::asio::placeholders::iterator));
		}
		else
		{
			// Reuse old connected socket.
			boost::system::error_code err;
			handleConnect(err, boost::asio::ip::tcp::resolver::iterator());
		}
		m_ioservice.reset();
		m_ioservice.run();

		if (!m_error)
			break;
	}

	// If error has been detected and we have read no data, return error code.
	if ((attempt > 1) && (m_size == size))
		return -1;

	return size - m_size;
}

void DeviceHttp::parseUrl(const std::string url)
{
	size_t t1 = url.find_first_of("//") + 2;
	size_t t2 = url.find_first_of("/", t1);
	m_server = url.substr(t1, t2 - t1);
	m_path = url.substr(t2);
}

void DeviceHttp::resolve()
{
	std::ostream request_stream(&m_request);
	request_stream << "HEAD " << m_path << " HTTP/1.1\r\n";
	request_stream << "Host: " << m_server << "\r\n\r\n";

	if (g_DebugMode)
	{
		std::cout << "\nHEAD " << m_path << " HTTP/1.1\n";
		std::cout << "Host: " << m_server << "\n\n";
	}

	// Start an asynchronous resolve to translate the server and
	// service names into a list of endpoints.
	boost::asio::ip::tcp::resolver::query query(m_server, "http");
	m_resolver.async_resolve(query,
	                         boost::bind(&DeviceHttp::handleResolve,
	                                     this,
	                                     boost::asio::placeholders::error,
	                                     boost::asio::placeholders::iterator));
}

void DeviceHttp::handleResolve(const boost::system::error_code& err,
                                      boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
{
	if (!err)
	{
		// Attempt a connection to the first endpoint in the list. Each endpoint
		// will be tried until we successfully establish a connection.
		boost::asio::ip::tcp::endpoint endpoint = *endpoint_iterator;
		m_socket.close();
		m_socket.async_connect(endpoint,
		                       boost::bind(&DeviceHttp::handleConnect,
		                                   this,
		                                   boost::asio::placeholders::error,
		                                   ++endpoint_iterator));
	}
	else
	{
		if (g_DebugMode)
			std::cout << "Error: " << err.message() << " / " << __PRETTY_FUNCTION__ << "\n";

		m_error = true;
		errno = ENOENT;
	}
}

void DeviceHttp::handleConnect(const boost::system::error_code& err,
                               boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
{
	if (!err)
	{
		// The connection was successful. Send the request.
		boost::asio::async_write(m_socket,
		                         m_request,
		                         boost::bind(&DeviceHttp::handleWriteRequest,
		                                     this,
		                                     boost::asio::placeholders::error));
	}
	else if (endpoint_iterator != boost::asio::ip::tcp::resolver::iterator())
	{
		// The connection failed. Try the next endpoint in the list.
		m_socket.close();
		boost::asio::ip::tcp::endpoint endpoint = *endpoint_iterator;
		m_socket.async_connect(endpoint,
		                       boost::bind(&DeviceHttp::handleConnect,
		                                   this,
		                                   boost::asio::placeholders::error,
		                                   ++endpoint_iterator));
	}
	else
	{
		if (g_DebugMode)
			std::cout << "Error: " << err.message() << " / " << __PRETTY_FUNCTION__ << "\n";

		m_error = true;
		errno = ENOENT;
	}
}

void DeviceHttp::handleWriteRequest(const boost::system::error_code& err)
{
	if (!err)
	{
		// Read the response headers.
		boost::asio::async_read_until(m_socket,
		                              m_response,
		                              "\r\n\r\n",
		                              boost::bind(&DeviceHttp::handleReadHeaders,
		                                          this,
		                                          boost::asio::placeholders::error));
	}
	else
	{
		if (g_DebugMode)
			std::cout << "Error: " << err.message() << " / " << __PRETTY_FUNCTION__ << "\n";

		m_error = true;
		errno = ENOENT;
	}
}

void DeviceHttp::handleReadHeaders(const boost::system::error_code& err)
{
	if (!err)
	{
		// Check that response is OK.
		std::istream response_stream(&m_response);
		std::string http_version;
		response_stream >> http_version;
		unsigned int status_code;
		response_stream >> status_code;
		std::string status_message;
		std::getline(response_stream, status_message);
		if (!response_stream || http_version.substr(0, 5) != "HTTP/")
			return;

		// Process the response headers.
		std::string header;
		std::string location;
		std::string contentLength;

		// We require HTTP/1.1 so default is keep-alive...
		m_closed = false;

		if (g_DebugMode)
			std::cout << "\n";

		while (std::getline(response_stream, header) && header != "\r")
		{
			if (g_DebugMode)
				std::cout << header << "\n";

			// Don't forget to remove last character (\r)...
			if (strncasecmp(header.c_str(), "Location: ", 10) == 0)
				location = header.substr(10, header.size() - 10 - 1);
			if (strncasecmp(header.c_str(), "Content-Length: ", 16) == 0)
				contentLength = header.substr(16, header.size() - 16 - 1);
			if (strncasecmp(header.c_str(), "Connection: close", 17) == 0)
				m_closed = true;
		}

		if (g_DebugMode)
			std::cout << "\n";

		if (200 == status_code)
		{
			// Success HEADER response, read total file size from content length.
			if (0 == m_fileSize)
				m_fileSize = boost::lexical_cast<off_t>(contentLength);
		}
		else if (206 == status_code)
		{
			// Success partial GET, read content length.
			m_contentLength = boost::lexical_cast<off_t>(contentLength);

			// Start reading remaining data.
			boost::asio::async_read(m_socket,
			                        m_response,
			                        boost::asio::transfer_at_least(1),
			                        boost::bind(&DeviceHttp::handleReadContent,
			                                    this,
			                                    boost::asio::placeholders::error));
		}
		else if ((status_code >= 300) && (status_code < 400) && !location.empty())
		{
			if (g_DebugMode)
				std::cout << "Redirection needed\n";

			// Redirection required.
			// Create an absolute path if relative is given.
			if (strncasecmp(location.c_str(), "http://", 7) != 0)
				location = "http://" + m_server + location;
			parseUrl(location);
			resolve();
		}
		else
		{
			if (g_DebugMode)
				std::cout << "Response returned with status code " << status_code << "\n";

			m_error = true;
			errno = ENOENT;
		}
	}
	else
	{
		if (g_DebugMode)
			std::cout << "Error: " << err.message() << err << " / " << __PRETTY_FUNCTION__ << "\n";

		m_error = true;
		errno = ENOENT;
		if (err == boost::asio::error::eof)
			m_closed = true;
	}
}

void DeviceHttp::handleReadContent(const boost::system::error_code& err)
{
	if (!err)
	{
		// Write all of the data that has been read so far.
		std::ostrstream out(m_data, m_size);
		m_data += m_response.size();
		m_size -= m_response.size();
		m_contentLength -= m_response.size();
		out << &m_response;

		// Continue reading remaining data if we expect it.
		if (m_contentLength > 0)
		{
			boost::asio::async_read(m_socket,
			                        m_response,
			                        boost::asio::transfer_at_least(1),
			                        boost::bind(&DeviceHttp::handleReadContent,
			                                    this,
			                                    boost::asio::placeholders::error));
		}
	}
	else
	{
		if (g_DebugMode)
			std::cout << "Error: " << err.message() << " / " << __PRETTY_FUNCTION__ << "\n";
	}
}

