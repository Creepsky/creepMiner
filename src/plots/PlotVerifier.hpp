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

#include <Poco/Task.h>
#include <vector>
#include "Declarations.hpp"
#include <Poco/AutoPtr.h>
#include <Poco/Notification.h>
#include <Poco/NotificationQueue.h>
#include "shabal/MinerShabal.hpp"
#include "mining/Miner.hpp"
#include "logging/Message.hpp"
#include "logging/MinerLogger.hpp"
#include "PlotReader.hpp"
#include "gpu/gpu_shell.hpp"
#include "gpu/algorithm/gpu_algorithm_atomic.hpp"

namespace Burst
{
	class PlotReadProgress;

	struct VerifyNotification : Poco::Notification
	{
		typedef Poco::AutoPtr<VerifyNotification> Ptr;

		ScoopData* buffer = nullptr;
		Poco::UInt64 accountId = 0;
		Poco::UInt64 nonceRead = 0;
		Poco::UInt64 nonceStart = 0;
		std::string inputPath = "";
		Poco::UInt64 block = 0;
		GensigData gensig;
		Poco::UInt64 baseTarget = 0;
		Poco::UInt64 nonces = 0;
	};
	
	using DeadlineTuple = std::pair<Poco::UInt64, Poco::UInt64>;
	using SubmitFunction = std::function<void(Poco::UInt64, Poco::UInt64, Poco::UInt64, Poco::UInt64, std::string, bool)>;

	template <typename TVerificationAlgorithm>
	class PlotVerifier : public Poco::Task
	{
	public:
		PlotVerifier(MinerData& data, Poco::NotificationQueue& queue, std::shared_ptr<PlotReadProgress> progress,
		             SubmitFunction submitFunction);
		~PlotVerifier() override;
		void runTask() override;
		
	private:
		MinerData* data_;
		Poco::NotificationQueue* queue_;
		std::shared_ptr<PlotReadProgress> progress_;
		SubmitFunction submitFunction_;
	};

	template <typename TVerificationAlgorithm>
	PlotVerifier<TVerificationAlgorithm>::PlotVerifier(MinerData& data, Poco::NotificationQueue& queue,
		std::shared_ptr<PlotReadProgress> progress, SubmitFunction submitFunction)
		: Task("PlotVerifier"), data_{&data}, queue_{&queue}, progress_{progress}, submitFunction_{submitFunction}
	{
	}

	template <typename TVerificationAlgorithm>
	PlotVerifier<TVerificationAlgorithm>::~PlotVerifier()
	{
	}

	template <typename TVerificationAlgorithm>
	void PlotVerifier<TVerificationAlgorithm>::runTask()
	{
		void* stream = nullptr;
		
		if (!TVerificationAlgorithm::initStream(&stream))
		{
			log_critical(MinerLogger::plotVerifier, "Could not create a verification stream!\n"
				"Shutting down the verifier...");
			return;
		}

		while (!isCancelled())
		{
			try
			{
				Poco::Notification::Ptr notification(queue_->waitDequeueNotification());
				VerifyNotification::Ptr verifyNotification;

				if (notification)
					verifyNotification = notification.cast<VerifyNotification>();
				else
					break;

				const auto stopFunction = [this, &verifyNotification]()
				{
					return isCancelled() || verifyNotification->block != data_->getCurrentBlockheight();
				};

				auto bestResult = TVerificationAlgorithm::run(verifyNotification->buffer, verifyNotification->nonces,
				                                              verifyNotification->nonceRead, verifyNotification->nonceStart,
				                                              verifyNotification->baseTarget, verifyNotification->gensig,
				                                              stopFunction, stream);

				if (bestResult.first != 0 && bestResult.second != 0)
				{
					submitFunction_(bestResult.first,
					                verifyNotification->accountId,
					                bestResult.second,
					                verifyNotification->block,
					                verifyNotification->inputPath,
					                true);
				}

				PlotReader::globalBufferSize.free(verifyNotification->buffer);

				if (progress_ != nullptr)
					progress_->add(verifyNotification->nonces * Settings::plotSize, verifyNotification->block);
			}
			catch (Poco::Exception& exc)
			{
				log_error(MinerLogger::plotVerifier, "One of the plot verifiers just crashed! It will recover now.\n"
					"\tReason: %s", exc.displayText());
				log_exception(MinerLogger::plotVerifier, exc);
			}
			catch (std::exception& exc)
			{
				log_error(MinerLogger::plotVerifier, "One of the plot verifiers just crashed! It will recover now.\n"
					"\tReason:\t%s", std::string(exc.what()));
			}
			catch (...)
			{
				log_error(MinerLogger::plotVerifier, "One of the plot verifiers just crashed by an unknown reason! It will recover now.");
			}
		}

		log_debug(MinerLogger::plotVerifier, "Verifier stopped");
	}

	template <typename TShabal>
	struct PlotVerifierOperations1
	{
		static void update(TShabal& shabal, const ScoopData* buffer, const size_t offset, const size_t size)
		{
			shabal.update(0 + offset >= size ? nullptr : reinterpret_cast<const unsigned char*>(buffer + offset + 0),
			              Settings::scoopSize);
		}

		template <typename TContainer>
		static void close(TShabal& shabal, TContainer& targets, const size_t offset, const size_t size)
		{
			shabal.close(0 + offset >= size ? nullptr : reinterpret_cast<unsigned char*>(targets[0].data()));
		}
	};

	template <typename TShabal>
	struct PlotVerifierOperations4
	{
		static void update(TShabal& shabal, const ScoopData* buffer, const size_t offset, const size_t size)
		{
			shabal.update(0 + offset >= size ? nullptr : reinterpret_cast<const unsigned char*>(buffer + offset + 0),
			              1 + offset >= size ? nullptr : reinterpret_cast<const unsigned char*>(buffer + offset + 1),
			              2 + offset >= size ? nullptr : reinterpret_cast<const unsigned char*>(buffer + offset + 2),
			              3 + offset >= size ? nullptr : reinterpret_cast<const unsigned char*>(buffer + offset + 3),
			              Settings::scoopSize);
		}

		template <typename TContainer>
		static void close(TShabal& shabal, TContainer& targets, const size_t offset, const size_t size)
		{
			shabal.close(0 + offset >= size ? nullptr : reinterpret_cast<unsigned char*>(targets[0].data()),
			             1 + offset >= size ? nullptr : reinterpret_cast<unsigned char*>(targets[1].data()),
			             2 + offset >= size ? nullptr : reinterpret_cast<unsigned char*>(targets[2].data()),
			             3 + offset >= size ? nullptr : reinterpret_cast<unsigned char*>(targets[3].data()));
		}
	};


	template <typename TShabal>
	struct PlotVerifierOperations8
	{
		static void update(TShabal& shabal, const ScoopData* buffer, const size_t offset, const size_t size)
		{
			shabal.update(0 + offset >= size ? nullptr : reinterpret_cast<const unsigned char*>(buffer + offset + 0),
			              1 + offset >= size ? nullptr : reinterpret_cast<const unsigned char*>(buffer + offset + 1),
			              2 + offset >= size ? nullptr : reinterpret_cast<const unsigned char*>(buffer + offset + 2),
			              3 + offset >= size ? nullptr : reinterpret_cast<const unsigned char*>(buffer + offset + 3),
			              4 + offset >= size ? nullptr : reinterpret_cast<const unsigned char*>(buffer + offset + 4),
			              5 + offset >= size ? nullptr : reinterpret_cast<const unsigned char*>(buffer + offset + 5),
			              6 + offset >= size ? nullptr : reinterpret_cast<const unsigned char*>(buffer + offset + 6),
			              7 + offset >= size ? nullptr : reinterpret_cast<const unsigned char*>(buffer + offset + 7),
			              Settings::scoopSize);
		}

		template <typename TContainer>
		static void close(TShabal& shabal, TContainer& targets, const size_t offset, const size_t size)
		{
			shabal.close(0 + offset >= size ? nullptr : reinterpret_cast<unsigned char*>(targets[0].data()),
			             1 + offset >= size ? nullptr : reinterpret_cast<unsigned char*>(targets[1].data()),
			             2 + offset >= size ? nullptr : reinterpret_cast<unsigned char*>(targets[2].data()),
			             3 + offset >= size ? nullptr : reinterpret_cast<unsigned char*>(targets[3].data()),
			             4 + offset >= size ? nullptr : reinterpret_cast<unsigned char*>(targets[4].data()),
			             5 + offset >= size ? nullptr : reinterpret_cast<unsigned char*>(targets[5].data()),
			             6 + offset >= size ? nullptr : reinterpret_cast<unsigned char*>(targets[6].data()),
			             7 + offset >= size ? nullptr : reinterpret_cast<unsigned char*>(targets[7].data()));
		}
	};

	template <typename TShabal, typename TShabalOperations>
	struct PlotVerifierAlgorithmCpu
	{
		static bool initStream(void** stream)
		{
			return true;
		}

		static DeadlineTuple run(ScoopData* buffer, const size_t size, const Poco::UInt64 nonceRead,
		                         const Poco::UInt64 nonceStart, const Poco::UInt64 baseTarget, const GensigData& gensig,
		                         const std::function<bool()>& stop, void* stream)
		{
			DeadlineTuple bestResult = {0, 0};
			TShabal shabalOriginal;

			// hash the gensig according to the cpu instruction level
			shabalOriginal.update(gensig.data(), Settings::hashSize);

			std::array<HashData, TShabal::HashSize> targets{};

			for (size_t i = 0; i < size && !stop(); i += TShabal::HashSize) 
			{
				auto shabal = shabalOriginal;

				// hash the scoop according to the cpu instruction level
				TShabalOperations::update(shabal, buffer, i, size);

				// digest the hash
				TShabalOperations::close(shabal, targets, i, size);

				for (auto j = 0u; j < TShabal::HashSize; ++j)
				{
					const auto nonce = nonceStart + nonceRead + i + j;
					const auto deadline = *reinterpret_cast<Poco::UInt64*>(targets[j].data()) / baseTarget;

					// make sure the nonce->deadline pair is valid and better than the others
					if (nonce > 0 && deadline > 0 && (bestResult.second == 0 || deadline < bestResult.second))
						bestResult = std::make_pair(nonce, deadline);
				}
			}

			return bestResult;
		}
	};


	template <typename TGpu, typename TAlgorithm>
	struct PlotVerifierAlgorithm_gpu
	{
		static bool initStream(void** stream)
		{
			return TGpu::initStream(stream);
		}

		static DeadlineTuple run(ScoopData* buffer, size_t size, Poco::UInt64 nonceRead, Poco::UInt64 nonceStart,
		                         Poco::UInt64 baseTarget, const GensigData& gensig, std::function<bool()> stop, void* stream)
		{
			DeadlineTuple bestDeadline{0, 0};
			TGpu::template run<TAlgorithm>(buffer, size, gensig, nonceStart + nonceRead, baseTarget, stream, bestDeadline);
			return bestDeadline;
		}
	};

	using PlotVerifierOperationSse2 = PlotVerifierOperations1<Shabal256Sse2>;
	using PlotVerifierOperationSse4 = PlotVerifierOperations4<Shabal256Sse4>;
	using PlotVerifierOperationAvx = PlotVerifierOperations4<Shabal256Avx>;
	using PlotVerifierOperationAvx2 = PlotVerifierOperations8<Shabal256Avx2>;

	using PlotVerifierAlgorithmSse2 = PlotVerifierAlgorithmCpu<Shabal256Sse2, PlotVerifierOperationSse2>;
	using PlotVerifierAlgorithmSse4 = PlotVerifierAlgorithmCpu<Shabal256Sse4, PlotVerifierOperationSse4>;
	using PlotVerifierAlgorithmAvx = PlotVerifierAlgorithmCpu<Shabal256Avx, PlotVerifierOperationAvx>;
	using PlotVerifierAlgorithmAvx2 = PlotVerifierAlgorithmCpu<Shabal256Avx2, PlotVerifierOperationAvx2>;

	using PlotVerifierSse2 = PlotVerifier<PlotVerifierAlgorithmSse2>;
	using PlotVerifierSse4 = PlotVerifier<PlotVerifierAlgorithmSse4>;
	using PlotVerifierAvx = PlotVerifier<PlotVerifierAlgorithmAvx>;
	using PlotVerifierAvx2 = PlotVerifier<PlotVerifierAlgorithmAvx2>;

	using PlotVerifierAlgorithmCuda = PlotVerifierAlgorithm_gpu<GpuCuda, GpuAlgorithmAtomic>;
	using PlotVerifierAlgorithmOpencl = PlotVerifierAlgorithm_gpu<GpuOpenCl, GpuAlgorithmAtomic>;

	using PlotVerifierCuda = PlotVerifier<PlotVerifierAlgorithmCuda>;
	using PlotVerifierOpencl = PlotVerifier<PlotVerifierAlgorithmOpencl>;
}

