// ==========================================================================
// 
// creepMiner - Burstcoin cryptocurrency CPU and GPU miner
// Copyright (C)  2016-2018 Creepsky (creepsky@gmail.com)
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110 - 1301  USA
// 
// ==========================================================================

#pragma once

#include <memory>
#include <vector>
#include <Poco/Net/HTTPClientSession.h>
#include "network/Url.hpp"
#include <Poco/Path.h>
#include <Poco/JSON/Object.h>
#include <functional>
#include "Declarations.hpp"
#include <chrono>

namespace Poco
{
	class File;
}

namespace Burst
{
	class PlotDir;
	class PlotFile;
	class MinerData;

	enum class HostType
	{
		Pool,
		Wallet,
		MiningInfo,
		Server
	};

	enum class TargetDeadlineType
	{
		Pool,
		Local,
		Combined
	};

	/**
	 * \brief Represents a passphrase, used for solo-mining.
	 * Includes informations for en-/decrypting a passphrase.
	 */
	struct Passphrase
	{
		std::string algorithm = "aes-256-cbc";
		std::string decrypted;
		bool deleteKey = true;
		std::string encrypted;
		Poco::UInt32 iterations = 1000;
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
		void recalculatePlotsHash();
		bool readConfigFile(const std::string& configPath);
		void rescan();

		/**
		 * \brief Rescans all plot dirs that are currently in use.
		 * Does NOT read new plot dirs from the configuration file!
		 */
		bool rescanPlotfiles();
		void checkPlotOverlaps();
		void printConsole() const;
		void printConsolePlots() const;
		void printUrl(HostType type) const;
		void printTargetDeadline() const;
		void printSubmitProbability() const;
		static void printUrl(const Url& url, const std::string& url_name);
		void printBufferSize() const;
		void printBufferChunks() const;

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
		 * \return true, if saved, false otherwise
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
		Poco::UInt64 getMaxBufferSizeRaw() const;
		Poco::UInt64 getMaxHistoricalBlocks() const;
		float getReceiveTimeout() const;
		float getSendTimeout() const;
		float getTimeout() const;
		Url getPoolUrl() const;
		Url getMiningInfoUrl() const;
		Url getWalletUrl() const;

		unsigned getReceiveMaxRetry() const;
		unsigned getSendMaxRetry() const;
		unsigned getSubmissionMaxRetry() const;
		unsigned getHttp() const;
		const std::string& getConfirmedDeadlinesPath() const;
		bool getStartServer() const;
		const std::string& getServerCertificatePath() const;
		const std::string& getServerCertificatePass() const;

		Url getServerUrl() const;
		double getSubmitProbability() const;
		double getTargetDLFactor() const;
		double getDeadlinePerformanceFac() const;
		Poco::UInt64 getTargetDeadline(TargetDeadlineType type = TargetDeadlineType::Combined) const;
		unsigned getMiningIntensity(bool real = true) const;
		bool forPlotDirs(std::function<bool(PlotDir&)> traverseFunction) const;
		//const std::string& getPlotsHash() const;
		const std::string& getPassphrase() const;
		bool useInsecurePlotfiles() const;
		bool isLogfileUsed() const;
		unsigned getMiningInfoInterval() const;
		bool isRescanningEveryBlock() const;
		LogOutputType getLogOutputType() const;
		bool isUsingLogColors() const;
		bool isSteadyProgressBar() const;
		bool isFancyProgressBar() const;
		unsigned getBufferChunkCount() const;
		bool isCalculatingEveryDeadline() const;

		/**
		 * \brief Returns the maximal amount of simultane plot reader.
		 * \param real If true and the value == 0, the amount of plot drives will be returned.
		 * If false the actual value will be returned.
		 * \return The maximal amount of simultane plot reader.
		 */
		unsigned getMaxPlotReaders(bool real = true) const;
		Poco::Path getPathLogfile() const;
		std::string getLogDir() const;
		std::string getServerUser() const;
		std::string getServerPass() const;
		unsigned getWalletRequestTries() const;
		unsigned getWalletRequestRetryWaitTime() const;
		unsigned getWakeUpTime() const;
		const std::string& getCpuInstructionSet() const;
		const std::string& getProcessorType() const;
		bool isBenchmark() const;
		long getBenchmarkInterval() const;
		unsigned getGpuPlatform() const;
		unsigned getGpuDevice() const;
		unsigned getMaxConnectionsQueued() const;
		unsigned getMaxConnectionsActive() const;
		bool isForwardingEverything() const;
		const std::vector<std::string>& getForwardingWhitelist() const;
		bool isCumulatingPlotsizes() const;
		bool isForwardingMinerName() const;

		void setUrl(std::string url, HostType hostType);
		void setBufferSize(Poco::UInt64 bufferSize);
		void setMaxHistoricalBlocks(Poco::UInt64 maxHistData);
		void setMaxSubmissionRetry(unsigned value);
		void setTimeout(float value);
		void setSubmitProbability(double subP);
		void setTargetDeadline(const std::string& target_deadline, TargetDeadlineType type);
		void setTargetDeadline(Poco::UInt64 target_deadline, TargetDeadlineType type);
		void setMininigIntensity(unsigned intensity);
		void setMaxPlotReaders(unsigned max_reader);
		void setLogDir(const std::string& log_dir);
		void setGetMiningInfoInterval(unsigned interval);
		void setBufferChunkCount(unsigned bufferChunkCount);
		void setPoolTargetDeadline(Poco::UInt64 targetDeadline);
		void setProcessorType(const std::string& processorType);
		void setCpuInstructionSet(const std::string& instructionSet);
		void setGpuPlatform(unsigned platformIndex);
		void setGpuDevice(unsigned deviceIndex);
		void setPlotDirs(const std::vector<std::string>& plotDirs);
		void setWebserverUri(const std::string& uri);
		void setProgressbar(bool fancy, bool steady);
		void setPassphrase(const std::string& passphrase);
		void setWebserverCredentials(const std::string& user, const std::string& pass);
		void setStartWebserver(bool start);

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
		 * \brief The passphrase, used by the webserver to hash (hmac) the username.
		 */
		static const std::string webserverUserPassphrase;

		/**
		 * \brief The passphrase, used by the webserver to hash (hmac) the password.
		 */
		static const std::string webserverPassPassphrase;

		/**
		 * \brief Returns the singleton-instance of the configuration.
		 * \return the current configuration.
		 */
		static MinerConfig& getConfig();
		
	private:
		static Poco::JSON::Object::Ptr readOutput(Poco::JSON::Object::Ptr json);
		static const std::string hashDelimiter;

		std::string configPath_;
		std::vector<std::shared_ptr<PlotDir>> plotDirs_;
		float timeout_ = 45.f;
		unsigned sendMaxRetry_ = 3;
		unsigned receiveMaxRetry_ = 3;
		unsigned submissionMaxRetry_ = 10;
		unsigned http_ = 1;
		std::string confirmedDeadlinesPath_ = "";
		Url urlPool_;
		Url urlMiningInfo_;
		Url urlWallet_;
		bool startServer_ = true;
		Url serverUrl_{"http://0.0.0.0:8124"};
		double targetDLFactor_ = 1.0;
		double deadlinePerformanceFac_ = 1.0;
		double submitProbability_ = 0.999;
		Poco::UInt64 targetDeadline_ = 0, targetDeadlinePool_ = 0;
		unsigned miningIntensity_ = 0;
		std::string plotsHash_;
		std::string serverUser_, serverPass_;
		unsigned maxPlotReaders_ = 0;
		Poco::Path pathLogfile_ = "";
		Poco::UInt64 maxBufferSizeMB_ = 0;
		Poco::UInt64 maxHistoricalBlocks_ = 0;
		unsigned bufferChunkCount_ = 16;
		unsigned walletRequestTries_ = 3;
		unsigned walletRequestRetryWaitTime_ = 3;
		Passphrase passphrase_ = {};
		bool useInsecurePlotfiles_ = false;
		bool logfile_ = false;
		unsigned getMiningInfoInterval_ = 3;
		bool rescanEveryBlock_ = false;
		LogOutputType logOutputType_ = LogOutputType::Terminal;
		bool logUseColors_ = true;
		bool steadyProgressBar_ = true;
		bool fancyProgressBar_ = true;
		unsigned wakeUpTime_ = 0;
		std::string cpuInstructionSet_ = "AUTO";
		std::string processorType_ = "CPU";
		bool benchmark_ = false;
		long benchmarkInterval_ = 60;
		unsigned gpuPlatform_ = 0, gpuDevice_ = 0;
		unsigned maxConnectionsQueued_ = 64, maxConnectionsActive_ = 32;
		std::vector<std::string> forwardingWhitelist_;
		bool cumulatePlotsizes_ = true;
		bool minerNameForwarding_ = true;
		bool calculateEveryDeadline_ = false;
		std::string serverCertificatePath_;
		std::string serverCertificatePass_;
		mutable Poco::Mutex mutex_;
	};
}
