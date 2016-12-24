//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#include "MinerUtil.hpp"
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include "Declarations.hpp"
#include "MinerLogger.hpp"
#include <iostream>

bool Burst::isNumberStr(const std::string& str)
{
	return std::all_of(str.begin(), str.end(), ::isdigit);
}

std::string Burst::getFileNameFromPath(const std::string& strPath)
{
	size_t iLastSeparator;
	return strPath.substr((iLastSeparator = strPath.find_last_of("/\\")) !=
						  std::string::npos ? iLastSeparator + 1 : 0, strPath.size() - strPath.find_last_of("."));
}

std::vector<std::string> Burst::splitStr(const std::string& s, char delim)
{
	std::vector<std::string> elems;
	splitStr(s, delim, elems);
	return elems;
}

std::vector<std::string> Burst::splitStr(const std::string& s, const std::string& delim)
{
	std::vector<std::string> tokens;
	std::string::size_type pos, lastPos = 0, length = s.length();

	using size_type  = typename std::vector<std::string>::size_type;

	while(lastPos < length + 1)
	{
		pos = s.find_first_of(delim, lastPos);

		if(pos == std::string::npos)
			pos = length;

		if(pos != lastPos)
			tokens.push_back(std::string(s.data() + lastPos,
			static_cast<size_type>(pos-lastPos)));

		lastPos = pos + 1;
	}

	return tokens;
}

std::vector<std::string>& Burst::splitStr(const std::string& s, char delim, std::vector<std::string>& elems)
{
	std::stringstream ss(s);
	std::string item;

	while (std::getline(ss, item, delim))
		elems.push_back(item);

	return elems;
}

bool Burst::isValidPlotFile(const std::string& filePath)
{
	auto result = false;
	auto fileName = getFileNameFromPath(filePath);

	if (getAccountIdFromPlotFile(fileName) != "" &&
		getStartNonceFromPlotFile(fileName) != "")
	{
		//struct stat info;
		//int statResult = stat( filePath.c_str(), &info );

		//if( statResult >= 0)
		//{
		//    if( (info.st_mode & S_IFDIR) == 0)
		//    {
		std::ifstream testRead(filePath);

		if (testRead.good())
			result = true;

		testRead.close();
		//    }
		//}
	}

	return result;
}

std::string Burst::getAccountIdFromPlotFile(const std::string& path)
{
	return getInformationFromPlotFile(path, 0);
}

std::string Burst::getNonceCountFromPlotFile(const std::string& path)
{
	return getInformationFromPlotFile(path, 2);
}

std::string Burst::getStaggerSizeFromPlotFile(const std::string& path)
{
	return getInformationFromPlotFile(path, 3);
}

std::string Burst::getStartNonceFromPlotFile(const std::string& path)
{
	size_t filenamePos = path.find_last_of("/\\");
	if (filenamePos == std::string::npos)
	{
		filenamePos = 0;
	}
	std::vector<std::string> fileNamePart = splitStr(path.substr(filenamePos + 1, path.length() - (filenamePos + 1)), '_');
	if (fileNamePart.size() > 3)
	{
		std::string nonceStartPart = fileNamePart[1];
		if (isNumberStr(nonceStartPart))
		{
			return nonceStartPart;
		}
	}
	return "";
}

std::string Burst::deadlineFormat(uint64_t seconds)
{
	auto secs = seconds;
	auto mins = secs / 60;
	auto hours = mins / 60;
	auto day = hours / 24;
	auto months = day / 30;
	auto years = months / 12;
	
	std::stringstream ss;
	
	ss.imbue(std::locale(""));
	ss << std::fixed;
	
	if (years > 0)
		ss << years << "y ";
	if (months > 0)
		ss << months % 12 << "m ";

	ss << day % 30 << "d ";
	ss << std::setw(2) << std::setfill('0');
	ss << hours % 24 << ':';
	ss << std::setw(2) << std::setfill('0');
	ss << mins % 60 << ':';
	ss << std::setw(2) << std::setfill('0');
	ss << secs % 60;

	return ss.str();
}

std::string Burst::gbToString(uint64_t size)
{
	std::stringstream ss;
	ss << std::fixed << std::setprecision(2);
	ss << size / 1024 / 1024 / 1024;
	return ss.str();
}

std::string Burst::versionToString()
{
	return std::to_string(Version::Major) + "." +
			std::to_string(Version::Minor) + "." +
			std::to_string(Version::Build);
}

std::string Burst::getInformationFromPlotFile(const std::string& path, uint8_t index)
{
	auto filenamePos = path.find_last_of("/\\");

	if (filenamePos == std::string::npos)
		filenamePos = 0;

	auto fileNamePart = splitStr(path.substr(filenamePos + 1, path.length() - (filenamePos + 1)), '_');

	if (index >= fileNamePart.size())
		return "";

	return fileNamePart[index];
}

Poco::Timespan Burst::secondsToTimespan(float seconds)
{
	auto secondsInt = static_cast<long>(seconds);
	auto microSeconds = static_cast<long>((seconds - secondsInt) * 100000);
	return Poco::Timespan{secondsInt, microSeconds};
}
