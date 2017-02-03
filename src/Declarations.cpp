#include "Declarations.hpp"
#include <string>

const Burst::Version Burst::Settings::ProjectVersion = { 1, 3, 0 };

#if defined MINING_CUDA
const Burst::ProjectData Burst::Settings::Project = { "creepMiner CUDA", ProjectVersion };
#else
const Burst::ProjectData Burst::Settings::Project = { "creepMiner", ProjectVersion };
#endif

Burst::Version::Version(uint32_t major, uint32_t minor, uint32_t build)
	: major{major}, minor{minor}, build{build}
{
	literal = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(build);
}

Burst::ProjectData::ProjectData(std::string&& name, Version version)
	: name{std::move(name)}, version{std::move(version)}
{
	nameAndVersion = this->name + " " + this->version.literal;
	nameAndVersionAndOs = this->name + " " + this->version.literal + " " + Settings::OsFamily;
}
