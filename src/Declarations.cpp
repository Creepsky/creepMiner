// ==========================================================================
// 
// creepMiner - Burstcoin cryptocurrency CPU and GPU miner
// Copyright (C)  2016-2017 Creepsky (creepsky@gmail.com)
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

#include "Declarations.hpp"
#include <string>
#include <Poco/String.h>
#include <Poco/StringTokenizer.h>
#include <Poco/NumberParser.h>
#include "logging/MinerLogger.hpp"

const Burst::Version Burst::Settings::ProjectVersion = { 1, 7, 0, 1 };
std::string Burst::Settings::Cpu_Instruction_Set = "";
Burst::ProjectData Burst::Settings::Project = { "creepMiner", ProjectVersion };

#ifdef USE_SSE4
const bool Burst::Settings::Sse4 = true;
#else
const bool Burst::Settings::Sse4 = false;
#endif

#ifdef USE_AVX
const bool Burst::Settings::Avx = true;
#else
const bool Burst::Settings::Avx = false;
#endif

#ifdef USE_AVX2
const bool Burst::Settings::Avx2 = true;
#else
const bool Burst::Settings::Avx2 = false;
#endif

#ifdef USE_CUDA
const bool Burst::Settings::Cuda = true;
#else
const bool Burst::Settings::Cuda = false;
#endif

#ifdef USE_OPENCL
const bool Burst::Settings::OpenCl = true;
#else
const bool Burst::Settings::OpenCl = false;
#endif

void Burst::Version::updateLiterals()
{
	literal = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(build);
	literalVerbose = literal + "." + std::to_string(revision);
}

Burst::Version::Version(uint32_t major, uint32_t minor, uint32_t build, uint32_t revision)
	: major{major}, minor{minor}, build{build}, revision{revision}
{
	updateLiterals();
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

			if (tokenizer.count() == 4)
			{
				const auto& revisionStr = tokenizer[3];
				revision = Poco::NumberParser::parseUnsigned(revisionStr);
			}
			else
				revision = 0;

			updateLiterals();
		}
		catch (Poco::Exception& exc)
		{
			major = 0;
			minor = 0;
			build = 0;
			revision = 0;
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
	if (build < rhs.build)
		return false;

	// 1.5.6.>6< vs 1.5.6.>1<
	if (revision > rhs.revision)
		return true;

	// 1.5.6.>1< vs 1.5.6.>6<
	return false;
}

void Burst::ProjectData::refreshNameAndVersion()
{
	nameAndVersion = this->name + " " + this->version.literal;
	nameAndVersionVerbose = this->name + " " +
		(this->version.revision > 0 ? this->version.literalVerbose : this->version.literal) + " " +
		Settings::OsFamily + " " + Settings::Arch;

	if (Settings::Cpu_Instruction_Set != "")
		nameAndVersionVerbose += std::string(" ") + Settings::Cpu_Instruction_Set;
}

Burst::ProjectData::ProjectData(std::string&& name, Version version)
	: name{std::move(name)}, version{std::move(version)}
{
	refreshNameAndVersion();
}

void Burst::Settings::setCpuInstructionSet(std::string cpuInstructionSet)
{
	Cpu_Instruction_Set = std::move(cpuInstructionSet);
	Project.refreshNameAndVersion();
}
