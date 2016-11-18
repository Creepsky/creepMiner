#pragma once

#include <memory>
#include <string>

namespace Burst
{
	class Socket;

	class Response
	{
	public:
		Response(std::unique_ptr<Socket> socket);

		bool canReceive() const;
		bool receive(std::string& data) const;

		std::unique_ptr<Socket> close();

	private:
		std::unique_ptr<Socket> socket_;
	};

	class HttpResponse
	{
	public:
		HttpResponse(const std::string& response);

		void setResponse(const std::string& response);

		const std::string& getResponse() const;
		std::string getStatus() const;
		std::string getContentLength() const;
		std::string getContentType() const;
		std::string getDate() const;
		std::string getMessage() const;

	private:
		std::string getPart(size_t index) const;

		std::string response_;
	};
}
