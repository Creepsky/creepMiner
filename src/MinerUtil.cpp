//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#include "MinerUtil.h"
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include "Declarations.hpp"

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
	size_t secs = seconds;
	size_t min = 0;
	size_t hour = 0;
	size_t day = 0;

	if (secs > 59)
	{
		min = secs / 60;
		secs = secs - min * 60;
	}

	if (min > 59)
	{
		hour = min / 60;
		min = min - hour * 60;
	}

	if (hour > 23)
	{
		day = hour / 24;
		hour = hour - day * 24;
	}

	std::string hourStr = std::to_string(hour);
	if (hour < 10)
	{
		hourStr = "0" + hourStr;
	}

	std::string minStr = std::to_string(min);
	if (min < 10)
	{
		minStr = "0" + minStr;
	}

	std::string secStr = std::to_string(secs);
	if (secs < 10)
	{
		secStr = "0" + secStr;
	}

	return std::to_string(day) + " days " + hourStr + ":" + minStr + ":" + secStr;
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
