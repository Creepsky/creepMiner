#include "MinerCL.hpp"
#include "logging/MinerLogger.hpp"

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

		log_system(MinerLogger::miner, "Available platforms:");

		// print platform infos
		for (auto platform : platforms)
		{
			std::string name;
			std::string version;

			if (!infoHelperFunc(platform, CL_PLATFORM_NAME, name) ||
				!infoHelperFunc(platform, CL_PLATFORM_VERSION, version))
			{
				log_error(MinerLogger::miner, "Could not get all infos of a OpenCL platform... skipping it!");
				continue;
			}

			log_system(MinerLogger::miner, "Platform: %s, Version: %s", name, version);
		}
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

		auto platform = platforms[platformIdx];
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
