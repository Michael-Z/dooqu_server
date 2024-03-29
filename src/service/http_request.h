//
// async_client.cpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef __HTTP_REQUEST_H__
#define __HTTP_REQUEST_H__

#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

using boost::asio::ip::tcp;

namespace dooqu_server
{
	namespace service
	{
		class http_request
		{
		public:
			tcp::resolver resolver_;
			tcp::socket socket_;
			boost::asio::streambuf request_;
			boost::asio::streambuf response_;
			boost::function<void(const boost::system::error_code&, const int, const string&)> callback_func_;


			http_request(boost::asio::io_service& io_service,
				const std::string& server, const std::string& path, boost::function<void(const boost::system::error_code&, const int status_code, const string&)> callback_func)
				: resolver_(io_service),
				socket_(io_service),
				callback_func_(callback_func)

			{
				// Form the request. We specify the "Connection: close" header so that the
				// server will close the socket after transmitting the response. This will
				// allow us to treat all data up until the EOF as the content.
				std::ostream request_stream(&request_);
				request_stream << "GET " << path << " HTTP/1.0\r\n";
				request_stream << "Host: " << server << "\r\n";
				request_stream << "Accept: */*\r\n";
				request_stream << "Connection: close\r\n\r\n";


				// Start an asynchronous resolve to translate the server and service names
				// into a list of endpoints.
				tcp::resolver::query query(server, "http");
				resolver_.async_resolve(query,
					boost::bind(&http_request::handle_resolve, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::iterator));
			}

			virtual ~http_request()
			{
				printf("~http_request\n");
			}

		private:
			void handle_resolve(const boost::system::error_code& err,
				tcp::resolver::iterator endpoint_iterator)
			{
				if (!err)
				{
					// Attempt a connection to each endpoint in the list until we
					// successfully establish a connection.
					boost::asio::async_connect(socket_, endpoint_iterator,
						boost::bind(&http_request::handle_connect, this,
						boost::asio::placeholders::error));
				}
				else
				{

					std::cout << "Error: " << err.message() << "\n";
					this->callback_func_(err, 0, std::string());
					//delete this;
				}
			}

			void handle_connect(const boost::system::error_code& err)
			{
				if (!err)
				{
					// The connection was successful. Send the request.
					boost::asio::async_write(socket_, request_,
						boost::bind(&http_request::handle_write_request, this,
						boost::asio::placeholders::error));
				}
				else
				{
					std::cout << "Error: " << err.message() << "\n";
					this->callback_func_(err, 0, std::string());
					//delete this;
					//boost::singleton_pool<http_request<game_service>, sizeof(http_request<game_service>)>::free(this);
				}
			}

			void handle_write_request(const boost::system::error_code& err)
			{
				if (!err)
				{
					// Read the response status line. The response_ streambuf will
					// automatically grow to accommodate the entire line. The growth may be
					// limited by passing a maximum size to the streambuf constructor.
					boost::asio::async_read_until(socket_, response_, "\r\n",
						boost::bind(&http_request::handle_read_status_line, this,
						boost::asio::placeholders::error));
				}
				else
				{
					std::cout << "Error: " << err.message() << "\n";
					this->callback_func_(err, 0, std::string());
					//delete this;
				}
			}

			void handle_read_status_line(const boost::system::error_code& err)
			{
				if (!err)
				{
					// Check that response is OK.
					std::istream response_stream(&response_);
					std::string http_version;
					response_stream >> http_version;
					unsigned int status_code;
					response_stream >> status_code;
					std::string status_message;
					std::getline(response_stream, status_message);


					if (!response_stream || http_version.substr(0, 5) != "HTTP/")
					{
						std::cout << "Invalid response\n";
						this->callback_func_(err, 400, std::string());
						//delete this;
						return;
					}


					if (status_code != 200)
					{
						std::cout << "Response returned with status code ";
						std::cout << status_code << "\n";
						this->callback_func_(err, status_code, std::string());
						return;
					}


					// Read the response headers, which are terminated by a blank line.
					boost::asio::async_read_until(socket_, response_, "\r\n\r\n",
						boost::bind(&http_request::handle_read_headers, this,
						boost::asio::placeholders::error));
				}
				else
				{
					std::cout << "Error: " << err << "\n";
					this->callback_func_(err, 0, std::string());
					//delete this;
				}
			}

			void handle_read_headers(const boost::system::error_code& err)
			{
				if (!err)
				{
					// Process the response headers.
					std::istream response_stream(&response_);
					std::string header;
					//while (std::getline(response_stream, header) && header != "\r")
					//	std::cout << header << "\n";
					//std::cout << "\n";

					// Write whatever content we already have to output.
					//if (response_.size() > 0)
					//	std::cout << &response_;

					// Start reading remaining data until EOF.
					boost::asio::async_read(socket_, response_,
						boost::asio::transfer_at_least(1),
						boost::bind(&http_request::handle_read_content, this,
						boost::asio::placeholders::error));
				}
				else
				{
					std::cout << "Error: " << err << "\n";
					this->callback_func_(err, 0, std::string());
					//delete this;
				}
			}

			void handle_read_content(const boost::system::error_code& err)
			{
				if (!err)
				{
					// Write all of the data that has been read so far.
					//std::cout << &response_;
					// Continue reading remaining data until EOF.
					boost::asio::async_read(socket_, response_,
						boost::asio::transfer_at_least(1),
						boost::bind(&http_request::handle_read_content, this,
						boost::asio::placeholders::error));
				}
				else if (err != boost::asio::error::eof)
				{
					std::cout << "Error: " << err << "\n";
					this->callback_func_(err, 0, std::string());
				}
				else
				{
					boost::asio::streambuf::const_buffers_type bufs = response_.data();

					int size = response_.size();
					std::string s(boost::asio::buffers_begin(bufs), boost::asio::buffers_begin(bufs) + size);

					//boost::system::error_code error;
					//error.assign(1, boost::system::get_system_category());
					this->callback_func_(err, 200, s);
				}
			}
		};
	}
}

#endif