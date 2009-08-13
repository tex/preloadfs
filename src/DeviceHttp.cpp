#include "DeviceHttp.hpp"
#include <strstream>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <strings.h>

DeviceHttp::DeviceHttp():
	m_resolver(m_ioservice),
	m_socket(m_ioservice)
{
}

void DeviceHttp::cancel()
{
	m_ioservice.stop();
}

bool DeviceHttp::open(const char *url)
{
	parseUrl(url);
	resolve();

	m_ioservice.reset();
	m_ioservice.run();

	return true;
}

off_t DeviceHttp::size()
{
	return m_fileSize;
}

ssize_t DeviceHttp::pread(char *destination, size_t size, off_t start)
{
	off_t end = start + size - 1;

	if (start >= m_fileSize)
		return 0;

	m_data = destination;
	m_size = size;

	do {
		m_error = false;

		std::ostream request_stream(&m_request);
		request_stream << "GET " << m_path << " HTTP/1.1\r\n";
		request_stream << "Host: " << m_server << "\r\n";
		request_stream << "Range: bytes=" << start << "-" << end << "\r\n\r\n";

		// Reuse old connected socket.
		boost::system::error_code err;
		handleGetSizeConnect(err, boost::asio::ip::tcp::resolver::iterator());

		m_ioservice.reset();
		m_ioservice.run();
	}
	while (m_error == true);

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

	// Start an asynchronous resolve to translate the server and
	// service names into a list of endpoints.
	boost::asio::ip::tcp::resolver::query query(m_server, "http");
	m_resolver.async_resolve(query,
	                         boost::bind(&DeviceHttp::handleGetSizeResolve,
	                                     this,
	                                     boost::asio::placeholders::error,
	                                     boost::asio::placeholders::iterator));
}

void DeviceHttp::handleGetSizeResolve(const boost::system::error_code& err,
                                      boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
{
	if (!err)
	{
		// Attempt a connection to the first endpoint in the list. Each endpoint
		// will be tried until we successfully establish a connection.
		boost::asio::ip::tcp::endpoint endpoint = *endpoint_iterator;
		m_socket.close();
		m_socket.async_connect(endpoint,
		                       boost::bind(&DeviceHttp::handleGetSizeConnect,
		                                   this,
		                                   boost::asio::placeholders::error,
		                                   ++endpoint_iterator));
	}
	else
	{
		std::cout << "Error: " << err.message() << " / " << __PRETTY_FUNCTION__ << "\n";
	}
}

void DeviceHttp::handleGetSizeConnect(const boost::system::error_code& err,
                                      boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
{
	if (!err)
	{
		// The connection was successful. Send the request.
		boost::asio::async_write(m_socket,
		                         m_request,
		                         boost::bind(&DeviceHttp::handleGetSizeWriteRequest,
		                                     this,
		                                     boost::asio::placeholders::error));
	}
	else if (endpoint_iterator != boost::asio::ip::tcp::resolver::iterator())
	{
		// The connection failed. Try the next endpoint in the list.
		m_socket.close();
		boost::asio::ip::tcp::endpoint endpoint = *endpoint_iterator;
		m_socket.async_connect(endpoint,
		                       boost::bind(&DeviceHttp::handleGetSizeConnect,
		                                   this,
		                                   boost::asio::placeholders::error,
		                                   ++endpoint_iterator));
	}
	else
	{
		std::cout << "Error: " << err.message() << " / " << __PRETTY_FUNCTION__ << "\n";
	}
}

void DeviceHttp::handleGetSizeWriteRequest(const boost::system::error_code& err)
{
	if (!err)
	{
		// Read the response headers.
		boost::asio::async_read_until(m_socket,
		                              m_response,
		                              "\r\n\r\n",
		                              boost::bind(&DeviceHttp::handleGetSizeReadHeaders,
		                                          this,
		                                          boost::asio::placeholders::error));
	}
	else
	{
		std::cout << "Error: " << err.message() << " / " << __PRETTY_FUNCTION__ << "\n";
	}
}

void DeviceHttp::handleGetSizeReadHeaders(const boost::system::error_code& err)
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
		while (std::getline(response_stream, header) && header != "\r")
		{
			// Don't forget to remove last character (\r)...
			if (strncasecmp(header.c_str(), "Location: ", 10) == 0)
				location = header.substr(10, header.size() - 10 - 1);
			if (strncasecmp(header.c_str(), "Content-Length: ", 16) == 0)
				contentLength = header.substr(16, header.size() - 16 - 1);
		}

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
			// Redirection required.
			// Create an absolute path if relative is given.
			if (strncasecmp(location.c_str(), "http://", 7) != 0)
				location = "http://" + m_server + location;
			parseUrl(location);
			resolve();
		}
		else
		{
			std::cout << "Response returned with status code " << status_code << "\n";
			errno = ENOENT;
		}
	}
	else
	{
		std::cout << "Error: " << err.message() << " / " << __PRETTY_FUNCTION__ << " / Performing reconect...\n";

		// Set flag to let pread() know that it must restart a request.
		m_error = true;
		resolve();
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
		std::cout << "Error: " << err.message() << " / " << __PRETTY_FUNCTION__ << "\n";
	}
}

