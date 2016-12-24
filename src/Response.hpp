#pragma once

#include <memory>
#include <string>
#include <vector>
#include <Poco/Net/HTTPClientSession.h>

namespace Burst
{
	class Response
	{
	public:
		Response(std::unique_ptr<Poco::Net::HTTPClientSession> session);
		Response(const Response& rhs) = delete;
		Response(Response&& rhs) = default;
		~Response();

		Response& operator=(const Response& rhs) = delete;
		Response& operator=(Response&& rhs) = default;

		bool canReceive() const;
		bool receive(std::string& data);

		std::unique_ptr<Poco::Net::HTTPClientSession> transferSession();
		const Poco::Exception* getLastError() const;
		bool isDataThere() const;

	private:
		std::unique_ptr<Poco::Net::HTTPClientSession> session_;
	};

	enum class SubmitResponse
	{
		Submitted,
		TooHigh,
		Error,
		None
	};

	struct NonceConfirmation
	{
		uint64_t deadline;
		SubmitResponse errorCode;
	};

	class NonceResponse
	{
	public:
		NonceResponse(std::unique_ptr<Poco::Net::HTTPClientSession> session);

		bool canReceive() const;
		NonceConfirmation getConfirmation();
		bool isDataThere();

		std::unique_ptr<Poco::Net::HTTPClientSession> transferSession();
		const Poco::Exception* getLastError() const;

	private:
		Response response_;
	};

	class HttpResponse
	{
	public:
		HttpResponse(const std::string& response);

		void setResponse(const std::string& response);

		const std::string& getResponse() const;
		const std::string& getStatus() const;
		const std::string& getContentLength() const;
		const std::string& getContentType() const;
		const std::string& getDate() const;
		const std::string& getMessage() const;

	private:
		const std::string& getPart(size_t index) const;

		std::string response_;
		std::vector<std::string> tokens_;
	};
}
