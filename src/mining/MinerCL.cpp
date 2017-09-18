#include "MinerCL.hpp"
#include "logging/MinerLogger.hpp"
#include <sstream>

namespace Burst
{
	struct MinerCLHelper
	{
		template <typename TSubject, typename TInfoFunc, typename TArg>
		static auto infoHelperFunc(const TSubject& subject, std::string& info, TInfoFunc infoFunc, TArg arg)
		{
			info = std::string(255, '\0');
			size_t size = 0;

			if (infoFunc(subject, arg, info.size(), &info[0], &size) != CL_SUCCESS)
				return false;

			// cut away the \0
			if (size > 0)
				--size;

			info.resize(size);
			return true;
		};
	};
}

bool Burst::MinerCL::create()
{
	std::vector<cl_platform_id> platforms;
	std::vector<cl_device_id> devices;

	// first, delete the old context 
	if (context_ != nullptr)
	{
		clReleaseContext(context_);
		context_ = nullptr;
	}

	// get the platforms and print infos 
	{
		cl_uint sizePlatforms;

		// get the number of valid opencl platforms 
		if (clGetPlatformIDs(0, nullptr, &sizePlatforms) != CL_SUCCESS)
		{
			log_fatal(MinerLogger::miner, "Could not detect the number of valid OpenCL platforms!");
			return false;
		}

		if (sizePlatforms == 0)
		{
			log_fatal(MinerLogger::miner, "No valid OpenCL platforms detected!");
			return false;
		}

		// get all valid opencl platforms 
		platforms.resize(sizePlatforms);

		if (clGetPlatformIDs(sizePlatforms, platforms.data(), nullptr) != CL_SUCCESS)
		{
			log_critical(MinerLogger::miner, "Could not get a list of valid OpenCL platforms!");
			return false;
		}

		const auto infoHelperFunc = [](decltype(platforms)::value_type platform, int type, std::string& info)
		{
			info = std::string(255, '\0');
			size_t size = 0;
			
			if (clGetPlatformInfo(platform, type, info.size(), &info[0], &size) != CL_SUCCESS)
				return false;

			// cut away the \0
			if (size > 0)
				--size;

			info.resize(size);

			return true;
		};

		std::stringstream sstream;
		sstream << "Available OpenCL platforms:";

		// print platform infos
		for (auto i = 0u; i < platforms.size(); ++i)
		{
			sstream << std::endl;
			std::string name;
			std::string version;

			const auto& platform = platforms[i];
			
			if (!MinerCLHelper::infoHelperFunc(platform, name, clGetPlatformInfo, CL_PLATFORM_NAME) ||
				!MinerCLHelper::infoHelperFunc(platform, version, clGetPlatformInfo, CL_PLATFORM_VERSION))
			{
				sstream << '\t' << "Could not get all infos of a OpenCL platform... skipping it!" << std::endl;
				continue;
			}

			sstream << '\t' << "Platform[" << i << "]: " << name << ", Version: " << version;
		}

		log_system(MinerLogger::miner, sstream.str());
	}

	// get the devices of the desired platform 
	{
		size_t platformIdx = 0;

		if (platformIdx < platforms.size())
		{
			log_system(MinerLogger::miner, "Using platform[%z]", platformIdx);
		}
		else
		{
			log_fatal(MinerLogger::miner, "Platform index is out of bounds (%z, max: %z)",
				platformIdx, platforms.size() - 1);
			return false;
		}

		const auto platform = platforms[platformIdx];
		cl_uint sizeDevices = 0;

		if (clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 0, nullptr, &sizeDevices) != CL_SUCCESS)
		{
			log_fatal(MinerLogger::miner, "Could not detect the number of valid OpenCL devices!");
			return false;
		}

		devices.resize(sizeDevices);

		if (clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, sizeDevices, devices.data(), nullptr) != CL_SUCCESS)
		{
			log_fatal(MinerLogger::miner, "Could not detect the number of valid OpenCL devices!");
			return false;
		}

		std::stringstream sstream;
		sstream << "Available OpenCL devices on platform[" << platformIdx << "]:";

		// print platform infos
		for (auto i = 0u; i < devices.size(); ++i)
		{
			sstream << std::endl;
			std::string name;

			const auto& device = devices[i];

			if (!MinerCLHelper::infoHelperFunc(device, name, clGetDeviceInfo, CL_DEVICE_NAME))
			{
				sstream << '\t' << "Could not get all infos of a OpenCL device... skipping it!" << std::endl;
				continue;
			}

			sstream << '\t' << "Device[" << i << "]: " << name;
		}

		log_system(MinerLogger::miner, sstream.str());
	}

	// create the context 
	{
		cl_int err = 0;

		context_ = clCreateContext(nullptr, devices.size(), devices.data(), nullptr, nullptr, &err);

		if (err != CL_SUCCESS)
		{
			log_fatal(MinerLogger::miner, "Could not create an OpenCL context!");
			return false;
		}

		log_system(MinerLogger::miner, "Successfully created OpenCL context");
	}

	return true;
}

cl_context& Burst::MinerCL::getContext()
{
	return context_;
}

const cl_context& Burst::MinerCL::getContext() const
{
	return context_;
}

Burst::MinerCL& Burst::MinerCL::getCL()
{
	static MinerCL minerCL;
	return minerCL;
}
