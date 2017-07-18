//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#pragma once

#include <memory>
#include <vector>
#include <Poco/Net/HTTPClientSession.h>
#include "Url.hpp"
#include <Poco/Path.h>
#include <Poco/JSON/Object.h>
#include <functional>

namespace Poco
{
	class File;
}

namespace Burst
{
	class MinerData;

	enum class HostType
	{
		Pool,
		Wallet,
		MiningInfo,
		Server
	};

	class PlotFile
	{
	public:
		PlotFile(std::string&& path, Poco::UInt64 size);

		const std::string& getPath() const;
		Poco::UInt64 getSize() const;
		
	private:
		std::string path_;
		Poco::UInt64 size_;
	};

	class PlotDir
	{
	public:
		enum class Type
		{
			Sequential,
			Parallel
		};

		using PlotList = std::vector<std::shared_ptr<PlotFile>>;

		PlotDir(std::string path, Type type);
		PlotDir(std::string path, const std::vector<std::string>& relatedPaths, Type type);

		/**
		 * \brief Returns all plot files inside the directory.
		 * \param recursive If true, also all plot files in all related plot directories are count.
		 * \return A list of all plot files.
		 */
		PlotList getPlotfiles(bool recursive = false) const;
		const std::string& getPath() const;
		Poco::UInt64 getSize() const;
		Type getType() const;
		std::vector<std::shared_ptr<PlotDir>> getRelatedDirs() const;
		const std::string& getHash() const;
		void rescan();

	private:
		bool addPlotLocation(const std::string& fileOrPath);
		std::shared_ptr<PlotFile> addPlotFile(const Poco::File& file);

		void recalculateHash();

		std::string path_;
		Type type_;
		Poco::UInt64 size_;
		PlotList plotfiles_;
		std::vector<std::shared_ptr<PlotDir>> relatedDirs_;
		std::string hash_;
	};

	/**
	 * \brief Represents a passphrase, used for solo-mining.
	 * Includes informations for en-/decrypting a passphrase.
	 */
	struct Passphrase
	{
		std::string algorithm;
		std::string decrypted;
		bool deleteKey;
		std::string encrypted;
		Poco::UInt32 iterations;
		std::string key;
		std::string salt;

		/**
		 * \brief Decrypts the encrypted passphrase.
		 * \return The plain-text passphrase.
		 */
		const std::string& decrypt();

		/**
		 * \brief Encrypts the plain-text passphrase.
		 * \return The encrypted passphrase.
		 */
		const std::string& encrypt();
	};

	class Socket;

	class MinerConfig
	{
	public:
		bool readConfigFile(const std::string& configPath);
		void rescan();

		/**
		 * \brief Rescans all plot dirs that are currently in use.
		 * Does NOT read new plot dirs from the configuration file!
		 */
		void rescanPlotfiles();
		void printConsole() const;
		void printConsolePlots() const;
		void printUrl(HostType type) const;
		void printTargetDeadline() const;
		static void printUrl(const Url& url, const std::string& url_name);
		void printBufferSize() const;

		/**
		 * \brief Saves the current settings by creating a JSON Object for it and saving it
		 * into the configuration file.
		 * \return true, if saved, false otherwise.
		 */
		bool save() const;
		
		/**
		 * \brief Saves the current settings by creating a JSON object for it and saving it
		 * into a file.
		 * \param path The path to the file, where the setting is written into.
		 * \return true, if saved, false otherwise.
		 */
		bool save(const std::string& path) const;

		/**
		 * \brief Saves a JSON object into a file.
		 * \param path The path to the file, where the JSON object is written into.
		 * \param json The JSON object, that needs to be saved.
		 * \return true, if saved, false otherwise.
		 */
		static bool save(const std::string& path, const Poco::JSON::Object& json);

		bool addPlotDir(std::shared_ptr<PlotDir> plotDir);

		const std::string& getPath() const;

		std::vector<std::shared_ptr<PlotFile>> getPlotFiles() const;
		uintmax_t getTotalPlotsize() const;

		Poco::UInt64 getMaxBufferSize() const;
		float getReceiveTimeout() const;
		float getSendTimeout() const;
		float getTimeout() const;
		Url getPoolUrl() const;
		Url getMiningInfoUrl() const;
		Url getWalletUrl() const;

		size_t getReceiveMaxRetry() const;
		size_t getSendMaxRetry() const;
		size_t getSubmissionMaxRetry() const;
		size_t getHttp() const;
		const std::string& getConfirmedDeadlinesPath() const;
		bool getStartServer() const;
		Url getServerUrl() const;
		Poco::UInt64 getTargetDeadline() const;
		size_t getMiningIntensity() const;
		bool forPlotDirs(std::function<bool(PlotDir&)> traverseFunction) const;
		const std::string& getPlotsHash() const;
		const std::string& getPassphrase() const;
		bool useInsecurePlotfiles() const;
		bool isLogfileUsed() const;

		/**
		 * \brief Returns the maximal amount of simultane plot reader.
		 * \param real If true and the value == 0, the amount of plot drives will be returned.
		 * If false the actual value will be returned.
		 * \return The maximal amount of simultane plot reader.
		 */
		size_t getMaxPlotReaders(bool real = true) const;
		Poco::Path getPathLogfile() const;
		std::string getLogDir() const;
		std::string getServerUser() const;
		std::string getServerPass() const;
		size_t getWalletRequestTries() const;
		size_t getWalletRequestRetryWaitTime() const;

		void setUrl(std::string url, HostType hostType);
		void setBufferSize(uint64_t bufferSize);
		void setMaxSubmissionRetry(uint64_t value);
		void setTimeout(float value);
		void setTargetDeadline(const std::string& target_deadline);
		void setTargetDeadline(uint64_t target_deadline);
		void setMininigIntensity(unsigned intensity);
		void setMaxPlotReaders(unsigned max_reader);
		void setLogDir(const std::string& log_dir);

		/**
		 * \brief Instructs the miner wether he should use a logfile.
		 * \note If true and so far no logfile was used, a new logfile will be created.
		 * If false and so far a logfile has been used, the logfile will be closed.
		 * \param use Indicates, if the miner uses a logfile.
		 */
		void useLogfile(bool use);

		bool addPlotDir(const std::string& dir);
		bool removePlotDir(const std::string& dir);

		/**
		 * \brief Creates a session for network communication over http.
		 * \param hostType The type of the far-end peer.
		 * \return The http session, if the connection was successful, nullptr otherwise.
		 */
		std::unique_ptr<Poco::Net::HTTPClientSession> createSession(HostType hostType) const;

		/**
		 * \brief The passphrase, used by the webserver to hash (hmac) username and password.
		 */
		static const std::string WebserverPassphrase;

		/**
		 * \brief Returns the singleton-instance of the configuration.
		 * \return the current configuration.
		 */
		static MinerConfig& getConfig();
		
	private:
		static Poco::JSON::Object::Ptr readOutput(Poco::JSON::Object::Ptr json);
		
		std::string configPath_;
		std::vector<std::shared_ptr<PlotDir>> plotDirs_;
		float timeout_ = 45.f;
		size_t send_max_retry_ = 3;
		size_t receive_max_retry_ = 3;
		size_t submission_max_retry_ = 3;
		size_t http_ = 0;
		std::string confirmedDeadlinesPath_ = "";
		Url urlPool_;
		Url urlMiningInfo_;
		Url urlWallet_;
		bool startServer_ = false;
		Url serverUrl_;
		Poco::UInt64 targetDeadline_ = 0;
		size_t miningIntensity_ = 1;
		std::string plotsHash_;
		std::string serverUser_, serverPass_;
		size_t maxPlotReaders_ = 0;
		Poco::Path pathLogfile_ = "";
		size_t maxBufferSizeMB_ = 256;
		size_t walletRequestTries_ = 5;
		size_t walletRequestRetryWaitTime_ = 3;
		Passphrase passphrase_;
		bool useInsecurePlotfiles_ = false;
		bool logfile_ = true;
		mutable Poco::Mutex mutex_;
	};
}
