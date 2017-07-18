#include "Declarations.hpp"
#include <string>
#include <Poco/String.h>
#include <Poco/StringTokenizer.h>
#include <Poco/NumberParser.h>
#include "MinerLogger.hpp"

const Burst::Version Burst::Settings::ProjectVersion = { 1, 6, 0 };

#if USE_AVX
const std::string Burst::Settings::Cpu_Instruction_Set = "AVX";
#elif USE_AVX2
const std::string Burst::Settings::Cpu_Instruction_Set = "AVX2";
#else
const std::string Burst::Settings::Cpu_Instruction_Set = "";
#endif

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

Burst::Version::Version(std::string version)
{
	// trim left and right spaces
	Poco::trimInPlace(version);

	// remove all spaces inside
	Poco::replaceInPlace(version, " ", "");

	Poco::StringTokenizer tokenizer{ version, "." };

	// need major.minor.build (3 parts, concatenated by a dot)
	if (tokenizer.count() == 3)
	{
		try
		{
			const auto& majorStr = tokenizer[0];
			const auto& minorStr = tokenizer[1];
			const auto& buildStr = tokenizer[2];

			major = Poco::NumberParser::parseUnsigned(majorStr);
			minor = Poco::NumberParser::parseUnsigned(minorStr);
			build = Poco::NumberParser::parseUnsigned(buildStr);

			literal = version;
		}
		catch (Poco::Exception& exc)
		{
			major = 0;
			minor = 0;
			build = 0;
			literal = "";

			log_error(MinerLogger::general, "Could not parse version from string! (%s)", version);
			log_exception(MinerLogger::general, exc);
		}
	}
}

bool Burst::Version::operator>(const Version& rhs) const
{
	// <this> vs <other>
	//
	// >2<.0.0 vs >1<.5.0
	if (major > rhs.major)
		return true;

	// >1<.5.0 vs >2<.0.0
	if (major < rhs.major)
		return false;

	// 1.>5<.0 vs 1.>4<.0
	if (minor > rhs.minor)
		return true;

	// 1.>4<.0 vs 1.>5<.0
	if (minor < rhs.minor)
		return false;

	// 1.5.>6< vs 1.5.>4<
	if (build > rhs.build)
		return true;

	// 1.5.>4< vs 1.5.>6<
	return false;
}

Burst::ProjectData::ProjectData(std::string&& name, Version version)
	: name{std::move(name)}, version{std::move(version)}
{
	nameAndVersion = this->name + " " + this->version.literal;
	nameAndVersionVerbose = this->name + " " + this->version.literal + " " + Settings::OsFamily + " " + Settings::Arch;

	if (Settings::Cpu_Instruction_Set != "")
		nameAndVersionVerbose += std::string(" ") + Settings::Cpu_Instruction_Set;
}
