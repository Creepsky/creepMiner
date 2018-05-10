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

#include "Plot.hpp"
#include <Poco/SHA1Engine.h>
#include <Poco/DigestStream.h>
#include "mining/Miner.hpp"
#include <Poco/File.h>
#include <Poco/DirectoryIterator.h>
#include "logging/MinerLogger.hpp"
#include "MinerUtil.hpp"

Burst::PlotFile::PlotFile(std::string&& path, Poco::UInt64 size)
	: path_(move(path)), size_(size)
{
	accountId_ = stoull(getAccountIdFromPlotFile(path_));
	nonceStart_ = stoull(getStartNonceFromPlotFile(path_));
	nonces_ = stoull(getNonceCountFromPlotFile(path_));
	staggerSize_ = stoull(getStaggerSizeFromPlotFile(path_));
}

const std::string& Burst::PlotFile::getPath() const
{
	return path_;
}

Poco::UInt64 Burst::PlotFile::getSize() const
{
	return size_;
}

Poco::UInt64 Burst::PlotFile::getAccountId() const
{
	return accountId_;
}

Poco::UInt64 Burst::PlotFile::getNonceStart() const
{
	return nonceStart_;
}

Poco::UInt64 Burst::PlotFile::getNonces() const
{
	return nonces_;
}

Poco::UInt64 Burst::PlotFile::getStaggerSize() const
{
	return staggerSize_;
}

Poco::UInt64 Burst::PlotFile::getStaggerCount() const
{
	return getNonces() / getStaggerSize();
}

Poco::UInt64 Burst::PlotFile::getStaggerBytes() const
{
	return getStaggerSize() * Settings::plotSize;
}

Poco::UInt64 Burst::PlotFile::getStaggerScoopBytes() const
{
	return getStaggerSize() * Settings::scoopSize;
}

Burst::PlotDir::PlotDir(std::string plotPath, Type type)
	: path_{std::move(plotPath)},
	  type_{type},
	  size_{0}
{
	addPlotLocation(path_);
	recalculateHash();
}

Burst::PlotDir::PlotDir(std::string path, const std::vector<std::string>& relatedPaths, Type type)
	: path_{std::move(path)},
	  type_{type},
	  size_{0}
{
	addPlotLocation(path_);

	for (const auto& relatedPath : relatedPaths)
		relatedDirs_.emplace_back(new PlotDir{relatedPath, type_});

	recalculateHash();
}

Burst::PlotDir::PlotList Burst::PlotDir::getPlotfiles(bool recursive) const
{
	// copy all plot files inside this plot directory
	PlotList plotFiles;

	plotFiles.insert(plotFiles.end(), plotfiles_.begin(), plotfiles_.end());

	if (recursive)
	{
		// copy also all plot files inside all related plot directories
		for (const auto& relatedPlotDir : getRelatedDirs())
		{
			auto relatedPlotFiles = relatedPlotDir->getPlotfiles(true);
			plotFiles.insert(std::end(plotFiles), std::begin(relatedPlotFiles), std::end(relatedPlotFiles));
		}
	}

	return plotFiles;
}

const std::string& Burst::PlotDir::getPath() const
{
	return path_;
}

Poco::UInt64 Burst::PlotDir::getSize() const
{
	return size_;
}

Burst::PlotDir::Type Burst::PlotDir::getType() const
{
	return type_;
}

std::vector<std::shared_ptr<Burst::PlotDir>> Burst::PlotDir::getRelatedDirs() const
{
	return relatedDirs_;
}

const std::string& Burst::PlotDir::getHash() const
{
	return hash_;
}

void Burst::PlotDir::rescan()
{
	plotfiles_.clear();
	size_ = 0;

	addPlotLocation(path_);

	for (auto& relatedDir : relatedDirs_)
		relatedDir->rescan();

	recalculateHash();
}

bool Burst::PlotDir::addPlotLocation(const std::string& fileOrPath)
{
	Poco::Path path;

	if (!path.tryParse(fileOrPath))
		throw Poco::Exception{Poco::format("%s is an invalid file/dir (syntax), skipping it!", fileOrPath)};

	Poco::File fileOrDir{path};

	if (!fileOrDir.exists())
		throw Poco::Exception{Poco::format("Plot file/dir does not exist: '%s'", path.toString())};

	// its a single plot file, add it if its really a plot file
	if (fileOrDir.isFile())
		return addPlotFile(fileOrPath) != nullptr;

	// its a dir, so we need to parse all plot files in it and add them
	if (fileOrDir.isDirectory())
	{
		Poco::DirectoryIterator iter{fileOrDir};
		const Poco::DirectoryIterator end;

		while (iter != end)
		{
			try
			{
				if (iter->isFile())
					addPlotFile(*iter);
			}
			catch (const Poco::Exception& e)
			{
				log_warning(MinerLogger::config, "Found an invalid plotfile, skipping it!\n\tPath: %s\n\tReason: %s",
					iter->path(), e.displayText());
			}

			++iter;
		}

		return true;
	}

	return false;
}

std::shared_ptr<Burst::PlotFile> Burst::PlotDir::addPlotFile(const Poco::File& file)
{
	const auto result = isValidPlotFile(file.path());

	if (result == PlotCheckResult::Ok)
	{
		// plot file is already in our list
		for (auto& plotfile : plotfiles_)
			if (plotfile->getPath() == file.path())
				return plotfile;

		// make a new plotfile and add it to the list
		auto plotFile = std::make_shared<PlotFile>(std::string(file.path()), file.getSize());
		plotfiles_.emplace_back(plotFile);
		size_ += file.getSize();

		return plotFile;
	}

	if (result == PlotCheckResult::EmptyParameter)
		return nullptr;

	std::string errorString;

	if (result == PlotCheckResult::Incomplete)
		throw Poco::Exception{"The plotfile is incomplete!"};

	if (result == PlotCheckResult::InvalidParameter)
		throw Poco::Exception{"The plotfile has invalid parameters!"};

	if (result == PlotCheckResult::WrongStaggersize)
		throw Poco::Exception{"The plotfile has an invalid staggersize!"};

	return nullptr;
}

void Burst::PlotDir::recalculateHash()
{
	Poco::SHA1Engine sha;
	Poco::DigestOutputStream shaStream{sha};

	hash_.clear();

	for (const auto& plotFile : getPlotfiles(true))
		shaStream << plotFile->getPath();

	shaStream << std::flush;
	hash_ = Poco::SHA1Engine::digestToHex(sha.digest());
}
