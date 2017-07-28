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

#pragma once

#include <Poco/Types.h>
#include <memory>
#include <vector>

namespace Poco {
	class File;
}

namespace Burst
{
	/**
	 * \brief Represents a plotfile.
	 * This class is not an actual representation of the physical file,
	 * but holds meta-informations about the plotfile.
	 */
	class PlotFile
	{
	public:
		/**
		 * \brief Constructor.
		 * \param path The path to the plotfile.
		 * \param size The size of the plotfile in Bytes.
		 */
		PlotFile(std::string&& path, Poco::UInt64 size);

		/**
		 * \brief Returns the path to the plotfile.
		 * \return A string, that holds the path to he plotfile.
		 */
		const std::string& getPath() const;

		/**
		 * \brief Returns the size of the plotfile.
		 * \return The size of the plotfile in Bytes.
		 */
		Poco::UInt64 getSize() const;

		/**
		 * \brief Returns the account id that the plotfile is bound to.
		 * \return The account id.
		 */
		Poco::UInt64 getAccountId() const;

		/**
		 * \brief Returns the nonce, that is the first nonce of the plot file.
		 * \return The first nonce of the plotfile.
		 */
		Poco::UInt64 getNonceStart() const;

		/**
		 * \brief Returns the number of nonces inside the plotfile.
		 * \return The size of the nonces.
		 */
		Poco::UInt64 getNonces() const;

		/**
		 * \brief Returns the size of the stagger.
		 * \return The staggersize.
		 */
		Poco::UInt64 getStaggerSize() const;

		/**
		 * \brief Returns the number of staggers.
		 * \return The number of the stagger, of which the plotfile consists.
		 */
		Poco::UInt64 getStaggerCount() const;

		/**
		 * \brief Returns the size of one stagger.
		 * \return The size of a stagger in bytes.
		 */
		Poco::UInt64 getStaggerBytes() const;

		/**
		 * \brief Returns the size of the scoops inside a stagger.
		 * \return The size of a scoop of all nonces inside a stagger in bytes.
		 */
		Poco::UInt64 getStaggerScoopBytes() const;

	private:
		std::string path_;
		Poco::UInt64 size_;
		Poco::UInt64 accountId_, nonceStart_, nonces_, staggerSize_;
	};

	/**
	 * \brief Represents a plot directory.
	 */
	class PlotDir
	{
	public:
		/**
		 * \brief The type of the plot directory.
		 */
		enum class Type
		{
			/**
			 * \brief Sequential; all files inside the plot directory are read by max. 1 plot reader.
			 */
			Sequential,
			/**
			 * \brief Parallel; all files inside the plot directory are read by min. 1 plot reader.
			 */
			Parallel
		};

		/**
		 * \brief Alias for std::vector<std::shared_ptr<PlotFile>>.
		 */
		using PlotList = std::vector<std::shared_ptr<PlotFile>>;

		/**
		 * \brief Constructor.
		 * Searches for all valid plot files inside the plot directory and adds them
		 * to a local list.
		 * Also a unique hash value for the specific directory is created.
		 * \param path The path of the plot directory.
		 * \param type The type of the plot directory.
		 */
		PlotDir(std::string path, Type type);

		/**
		 * \brief Constructor.
		 * Searches for all valid plot files inside the plot directory and adds them
		 * to a local list.
		 * Also a unique hash value for the specific directory is created.
		 * \param path The path of the plot directory.
		 * \param relatedPaths A list of all related plot directories.
		 * \param type The type of the plot directory.
		 * If Parallel, files of all related directories are also considered as parallel.
		 */
		PlotDir(std::string path, const std::vector<std::string>& relatedPaths, Type type);

		/**
		* \brief Returns all plot files inside the directory.
		* \param recursive If true, also all plot files in all related plot directories are gathered.
		* \return A list of all plot files.
		*/
		PlotList getPlotfiles(bool recursive = false) const;

		/**
		 * \brief Returns the path of the directory.
		 * \return The absolute path of the directory.
		 */
		const std::string& getPath() const;

		/**
		 * \brief The size of all valid plot files inside the directory.
		 * \return The size in Bytes.
		 */
		Poco::UInt64 getSize() const;

		/**
		 * \brief Returns the type of the directory.
		 * \return A value of \enum Type.
		 */
		Type getType() const;

		/**
		 * \brief Returns a list of all related plot directories.
		 * \return A list of shared_ptr of all related plot directories.
		 * Empty, if no related plot directories.
		 */
		std::vector<std::shared_ptr<PlotDir>> getRelatedDirs() const;

		/**
		 * \brief Returns the unique hash-value of the plot directory.
		 * It can be used to compare different miner instances.
		 * \return A SHA1-hash represented as a string.
		 */
		const std::string& getHash() const;

		/**
		 * \brief Resets the list of all plot files and searches the directory again for them.
		 * The unique hash value and the total size is also recalculated.
		 */
		void rescan();

	private:
		/**
		 * \brief Adds a plotfile to the internal list of plotfiles, if it is a valid plotfile.
		 * If the path is a directory, the function gets called recursively for every plotfile
		 * inside the plot directory.
		 * \param fileOrPath The path to the plotfile or plot directory.
		 * \return true, if all plot files could be added.
		 */
		bool addPlotLocation(const std::string& fileOrPath);

		/**
		 * \brief Adds a single plot file to the internal list of plotfiles, if it is a valid plotfile.
		 * The size of the plotfile is added to the total size of the plot directory. Also 
		 * \param file The file, that needs to be added.
		 * \return A shared_ptr that points to the internal created \class PlotFile
		 * If there is an error while creation, a nullptr will be returned instead.
		 */
		std::shared_ptr<PlotFile> addPlotFile(const Poco::File& file);

		/**
		 * \brief Calculates the unique hash value of all plot files inside the internal plotfiles list.
		 */
		void recalculateHash();

		std::string path_;
		Type type_;
		Poco::UInt64 size_;
		PlotList plotfiles_;
		std::vector<std::shared_ptr<PlotDir>> relatedDirs_;
		std::string hash_;
	};
}
