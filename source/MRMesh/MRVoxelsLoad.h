#pragma once
#include "MRMeshFwd.h"
#include "MRProgressCallback.h"
#include "MRObject.h"
#include "MRVoxelsVolume.h"
#include "MRIOFilters.h"
#include <filesystem>
#include "MRExpected.h"

namespace MR
{

namespace VoxelsLoad
{

/// \defgroup VoxelsLoadGroup Voxels Load
/// \ingroup IOGroup
/// \{

MRMESH_API extern const IOFilters Filters;

#ifndef MRMESH_NO_OPENVDB
/// Sort files in given vector by names (respect numbers in it)
/// usually needed for scans
MRMESH_API void sortFilesByName( std::vector<std::filesystem::path>& scans );

#if !defined(MRMESH_NO_DICOM)
struct DicomVolume
{
    SimpleVolume vol;
    std::string name;
    AffineXf3f xf;
};

struct LoadDCMResult
{
    VdbVolume vdbVolume;
    std::string name;
    AffineXf3f xf;
};

/// Loads 3D all volumetric data from DICOM files in a folder
MRMESH_API std::vector<Expected<LoadDCMResult>> loadDCMsFolder( const std::filesystem::path& path,
                                                        unsigned maxNumThreads = 4, const ProgressCallback& cb = {} );
/// Loads 3D first volumetric data from DICOM files in a folder
MRMESH_API Expected<LoadDCMResult> loadDCMFolder( const std::filesystem::path& path,
                                                        unsigned maxNumThreads = 4, const ProgressCallback& cb = {} );

/// Loads 3D all volumetric data from DICOM files in a folder
MRMESH_API std::vector<Expected<DicomVolume>> loadDicomsFolder( const std::filesystem::path& path,
                                                        unsigned maxNumThreads = 4, const ProgressCallback& cb = {} );
/// Loads 3D first volumetric data from DICOM files in a folder
MRMESH_API Expected<DicomVolume> loadDicomFolder( const std::filesystem::path& path,
                                                        unsigned maxNumThreads = 4, const ProgressCallback& cb = {} );

/// Loads every subfolder with DICOM volume as new object
MRMESH_API std::vector<Expected<LoadDCMResult>> loadDCMFolderTree( const std::filesystem::path& path,
                                                        unsigned maxNumThreads = 4, const ProgressCallback& cb = {} );

/// Loads 3D volumetric data from a single DICOM file
MRMESH_API Expected<DicomVolume> loadDicomFile( const std::filesystem::path& path, const ProgressCallback& cb = {} );
#endif // MRMESH_NO_DICOM

struct RawParameters
{
    Vector3i dimensions;
    Vector3f voxelSize;
    bool gridLevelSet{ false }; ///< OpenVDB GridClass set as GRID_LEVEL_SET (need to set right surface normals direction)
    enum class ScalarType
    {
        UInt8,
        Int8,
        UInt16,
        Int16,
        UInt32,
        Int32,
        UInt64,
        Int64,
        Float32,
        Float64,
        Float32_4, ///< the last value from float[4]
        Unknown,
        Count
    } scalarType{ ScalarType::Float32 };
};
/// Load raw voxels from file with provided parameters
MRMESH_API Expected<VdbVolume> fromRaw( const std::filesystem::path& file, const RawParameters& params,
                                                         const ProgressCallback& cb = {} );
/// Load raw voxels from stream with provided parameters;
/// important on Windows: in stream must be open in binary mode
MRMESH_API Expected<VdbVolume> fromRaw( std::istream& in, const RawParameters& params,
                                                         const ProgressCallback& cb = {} );

/// Load raw voxels file, parsing parameters from name 
MRMESH_API Expected<VdbVolume> fromRaw( const std::filesystem::path& file,
                                                         const ProgressCallback& cb = {} );

#ifdef MRMESH_OPENVDB_USE_IO
/// Load raw voxels OpenVDB file
MRMESH_API Expected<std::vector<VdbVolume>> fromVdb( const std::filesystem::path& file,
                                                         const ProgressCallback& cb = {} );
#endif

/// Load voxel from Gav-file with micro CT reconstruction
MRMESH_API Expected<VdbVolume> fromGav( const std::filesystem::path& file, const ProgressCallback& cb = {} );
/// Load voxel from Gav-stream with micro CT reconstruction
MRMESH_API Expected<VdbVolume> fromGav( std::istream& in, const ProgressCallback& cb = {} );


/// Detects the format from file extension and loads voxels from it
MRMESH_API Expected<std::vector<VdbVolume>> fromAnySupportedFormat( const std::filesystem::path& file,
                                                                        const ProgressCallback& cb = {} );

/// \}

// Determines iso-surface orientation
enum class GridType
{
    // consider values less than iso as outer area
    DenseGrid = 0,
    // consider values less than iso as inner area
    LevelSet = 1
};

#ifndef MRMESH_NO_TIFF
struct LoadingTiffSettings
{
    std::filesystem::path dir;
    Vector3f voxelSize = { 1.0f, 1.0f, 1.0f };
    GridType gridType = GridType::DenseGrid;
    ProgressCallback cb = {};
};
/// Load voxels from a set of TIFF files
MRMESH_API Expected<VdbVolume> loadTiffDir( const LoadingTiffSettings& settings );
#endif // MRMESH_NO_TIFF

#endif // MRMESH_NO_OPENVDB
}

}
