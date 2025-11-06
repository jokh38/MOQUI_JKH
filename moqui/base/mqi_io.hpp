#ifndef MQI_IO_HPP
#define MQI_IO_HPP

#include <algorithm>
#include <complex>
#include <cstdint>
#include <iomanip>   // std::setprecision
#include <iostream>
#include <numeric>   //accumulate
#include <valarray>
#include <zlib.h>
#include <sstream>
#include <ctime>

#include <sys/mman.h>   //for io

#include <moqui/base/mqi_common.hpp>
#include <moqui/base/mqi_hash_table.hpp>
#include <moqui/base/mqi_roi.hpp>
#include <moqui/base/mqi_sparse_io.hpp>
#include <moqui/base/mqi_scorer.hpp>

// GDCM headers for DICOM support
#include "gdcmAttribute.h"
#include "gdcmDataElement.h"
#include "gdcmDataSet.h"
#include "gdcmFile.h"
#include "gdcmImage.h"
#include "gdcmImageWriter.h"
#include "gdcmUIDGenerator.h"
#include "gdcmWriter.h"

namespace mqi
{
namespace io
{
///<  save scorer data to a file in binary format
///<  scr: scorer pointer
///<  scale: data will be multiplied by
///<  dir  : directory path. file name will be dir + scr->name + ".bin"
///<  reshape: roi is used in scorer, original size will be defined.
template<typename R>
void
save_to_bin(const mqi::scorer<R>* src,
            const R               scale,
            const std::string&    filepath,
            const std::string&    filename);

template<typename R>
void
save_to_bin(const R*           src,
            const R            scale,
            const std::string& filepath,
            const std::string& filename,
            const uint32_t     length);

template<typename R>
void
save_to_npz(const mqi::scorer<R>* src,
            const R               scale,
            const std::string&    filepath,
            const std::string&    filename,
            mqi::vec3<mqi::ijk_t> dim,
            uint32_t              num_spots);

template<typename R>
void
save_to_npz2(const mqi::scorer<R>* src,
             const R               scale,
             const std::string&    filepath,
             const std::string&    filename,
             mqi::vec3<mqi::ijk_t> dim,
             uint32_t              num_spots);

template<typename R>
void
save_to_npz(const mqi::scorer<R>* src,
            const R               scale,
            const std::string&    filepath,
            const std::string&    filename,
            mqi::vec3<mqi::ijk_t> dim,
            uint32_t              num_spots,
            R*                    time_scale,
            R                     threshold);

template<typename R>
void
save_to_bin(const mqi::key_value* src,
            const R               scale,
            uint32_t              max_capacity,
            const std::string&    filepath,
            const std::string&    filename);

template<typename R>
void
save_to_mhd(const mqi::node_t<R>* children,
            const double*         src,
            const R               scale,
            const std::string&    filepath,
            const std::string&    filename,
            const uint32_t        length);

template<typename R>
void
save_to_mha(const mqi::node_t<R>* children,
             const double*         src,
             const R               scale,
             const std::string&    filepath,
             const std::string&    filename,
             const uint32_t        length);

template<typename R>
void
save_to_dcm(const mqi::scorer<R>* src,
            const R               scale,
            const std::string&    filepath,
            const std::string&    filename,
            const uint32_t        length,
            const mqi::vec3<ijk_t>& dim,
            const bool            is_2cm_mode = false);
}   // namespace io
}   // namespace mqi

///< Function to write key values into file
///< src: array and this array is copied
///<
template<typename R>
void
mqi::io::save_to_bin(const mqi::scorer<R>* src,
                     const R               scale,
                     const std::string&    filepath,
                     const std::string&    filename) {
    /// create a copy using valarray and apply scale

    unsigned int            nnz = 0;
    std::vector<mqi::key_t> key1;
    std::vector<mqi::key_t> key2;
    std::vector<double>     value;
    key1.clear();
    key2.clear();
    value.clear();
    for (int ind = 0; ind < src->max_capacity_; ind++) {
        if (src->data_[ind].key1 != mqi::empty_pair && src->data_[ind].key2 != mqi::empty_pair &&
            src->data_[ind].value > 0) {
            key1.push_back(src->data_[ind].key1);
            key2.push_back(src->data_[ind].key2);
            value.push_back(src->data_[ind].value * scale);
        }
    }

    printf("length %lu %lu %lu\n", key1.size(), key2.size(), value.size());

    /// open out stream
    std::ofstream fid_key1(filepath + "/" + filename + "_key1.raw",
                           std::ios::out | std::ios::binary);
    if (!fid_key1)
        std::cout << "Cannot write :" << filepath + "/" + filename + "_key1.raw" << std::endl;

    /// write to a file
    fid_key1.write(reinterpret_cast<const char*>(&key1.data()[0]),
                   key1.size() * sizeof(mqi::key_t));
    fid_key1.close();

    std::ofstream fid_key2(filepath + "/" + filename + "_key2.raw",
                           std::ios::out | std::ios::binary);
    if (!fid_key2)
        std::cout << "Cannot write :" << filepath + "/" + filename + "_key2.raw" << std::endl;

    /// write to a file
    fid_key2.write(reinterpret_cast<const char*>(&key2.data()[0]),
                   key2.size() * sizeof(mqi::key_t));
    fid_key2.close();

    std::ofstream fid_bin(filepath + "/" + filename + "_value.raw",
                          std::ios::out | std::ios::binary);
    if (!fid_bin)
        std::cout << "Cannot write :" << filepath + "/" + filename + "_value.raw" << std::endl;

    /// write to a file
    fid_bin.write(reinterpret_cast<const char*>(&value.data()[0]), value.size() * sizeof(double));
    fid_bin.close();
}

///< Function to write array into file
///< src: array and this array is copied
///<
template<typename R>
void
mqi::io::save_to_bin(const R*           src,
                     const R            scale,
                     const std::string& filepath,
                     const std::string& filename,
                     const uint32_t     length) {
    /// create a copy using valarray and apply scale
    std::valarray<R> dest(src, length);
    munmap(&dest, length * sizeof(R));
    dest *= scale;
    /// open out stream
    std::ofstream fid_bin(filepath + "/" + filename + ".raw", std::ios::out | std::ios::binary);
    if (!fid_bin) std::cout << "Cannot write :" << filepath + "/" + filename + ".raw" << std::endl;

    /// write to a file
    fid_bin.write(reinterpret_cast<const char*>(&dest[0]), length * sizeof(R));
    fid_bin.close();
}

///< Function to write key values into file
///< src: array and this array is copied
///<
template<typename R>
void
mqi::io::save_to_bin(const mqi::key_value* src,
                     const R               scale,
                     uint32_t              max_capacity,
                     const std::string&    filepath,
                     const std::string&    filename) {
    /// create a copy using valarray and apply scale

    unsigned int            nnz = 0;
    std::vector<mqi::key_t> key1;
    std::vector<mqi::key_t> key2;
    std::vector<R>          value;
    key1.clear();
    key2.clear();
    value.clear();
    for (int ind = 0; ind < max_capacity; ind++) {
        if (src[ind].key1 != mqi::empty_pair && src[ind].key2 != mqi::empty_pair &&
            src[ind].value > 0) {
            key1.push_back(src[ind].key1);
            key2.push_back(src[ind].key2);
            value.push_back(src[ind].value * scale);
        }
    }

    printf("length %lu %lu %lu\n", key1.size(), key2.size(), value.size());
    /// open out stream
    std::ofstream fid_key1(filepath + "/" + filename + "_key1.raw",
                           std::ios::out | std::ios::binary);
    if (!fid_key1)
        std::cout << "Cannot write :" << filepath + "/" + filename + "_key1.raw" << std::endl;

    /// write to a file
    fid_key1.write(reinterpret_cast<const char*>(&key1.data()[0]),
                   key1.size() * sizeof(mqi::key_t));
    fid_key1.close();

    std::ofstream fid_key2(filepath + "/" + filename + "_key2.raw",
                           std::ios::out | std::ios::binary);
    if (!fid_key2)
        std::cout << "Cannot write :" << filepath + "/" + filename + "_key2.raw" << std::endl;

    /// write to a file
    fid_key2.write(reinterpret_cast<const char*>(&key2.data()[0]),
                   key2.size() * sizeof(mqi::key_t));
    fid_key2.close();

    std::ofstream fid_bin(filepath + "/" + filename + "_value.raw",
                          std::ios::out | std::ios::binary);
    if (!fid_bin)
        std::cout << "Cannot write :" << filepath + "/" + filename + "_value.raw" << std::endl;

    /// write to a file
    fid_bin.write(reinterpret_cast<const char*>(&value.data()[0]), value.size() * sizeof(R));
    fid_bin.close();
}

///< Function to write key values into file
///< src: array and this array is copied
///<

template<typename R>
void
mqi::io::save_to_npz(const mqi::scorer<R>* src,
                     const R               scale,
                     const std::string&    filepath,
                     const std::string&    filename,
                     mqi::vec3<mqi::ijk_t> dim,
                     uint32_t              num_spots) {
    uint32_t vol_size;
    vol_size = dim.x * dim.y * dim.z;

    /// create a copy using valarray and apply scale
    const std::string name_a = "indices.npy", name_b = "indptr.npy", name_c = "shape.npy",
                      name_d = "data.npy", name_e = "format.npy";
    std::vector<double>* value_vec = new std::vector<double>[num_spots];
    std::vector<mqi::key_t>*          vox_vec = new std::vector<mqi::key_t>[num_spots];
    std::vector<double>               data_vec;
    std::vector<uint32_t>             indices_vec;
    std::vector<uint32_t>             indptr_vec;
    mqi::key_t                        vox_ind, spot_ind;
    double                            value;
    int                               spot_start = 0, spot_end = 0;
    int                               vox_in_spot[num_spots];
    std::vector<double>::iterator     it_data;
    std::vector<uint32_t>::iterator   it_ind;
    std::vector<mqi::key_t>::iterator it_spot;
    int                               vox_count;
    printf("save_to_npz\n");

    printf("scan start %d\n", src->max_capacity_);
    for (int ind = 0; ind < src->max_capacity_; ind++) {
        if (src->data_[ind].key1 != mqi::empty_pair && src->data_[ind].key2 != mqi::empty_pair) {
            vox_count = 0;
            vox_ind   = src->data_[ind].key1;
            spot_ind  = src->data_[ind].key2;
            assert(vox_ind >= 0 && vox_ind < vol_size);
            value = src->data_[ind].value;
            value_vec[spot_ind].push_back(value * scale);
            vox_vec[spot_ind].push_back(vox_ind);
        }
    }

    vox_count = 0;
    indptr_vec.push_back(vox_count);
    for (int ii = 0; ii < num_spots; ii++) {
        data_vec.insert(data_vec.end(), value_vec[ii].begin(), value_vec[ii].end());
        indices_vec.insert(indices_vec.end(), vox_vec[ii].begin(), vox_vec[ii].end());
        vox_count += vox_vec[ii].size();
        indptr_vec.push_back(vox_count);
    }

    printf("scan done %lu %lu %lu\n", data_vec.size(), indices_vec.size(), indptr_vec.size());
    printf("%d %d\n", vol_size, num_spots);

    uint32_t    shape[2] = { num_spots, vol_size };
    std::string format   = "csr";
    size_t      size_a = indices_vec.size(), size_b = indptr_vec.size(), size_c = 2,
           size_d = data_vec.size(), size_e = 3;

    uint32_t* indices = new uint32_t[indices_vec.size()];
    uint32_t* indptr  = new uint32_t[indptr_vec.size()];
    double*   data    = new double[data_vec.size()];
    std::copy(indices_vec.begin(), indices_vec.end(), indices);
    std::copy(indptr_vec.begin(), indptr_vec.end(), indptr);
    std::copy(data_vec.begin(), data_vec.end(), data);
    printf("%lu\n", size_b);
    mqi::io::save_npz(filepath + "/" + filename + ".npz", name_a, indices, size_a, "w");
    mqi::io::save_npz(filepath + "/" + filename + ".npz", name_b, indptr, size_b, "a");
    mqi::io::save_npz(filepath + "/" + filename + ".npz", name_c, shape, size_c, "a");
    mqi::io::save_npz(filepath + "/" + filename + ".npz", name_d, data, size_d, "a");
    mqi::io::save_npz(filepath + "/" + filename + ".npz", name_e, format, size_e, "a");
}

template<typename R>
void
mqi::io::save_to_npz2(const mqi::scorer<R>* src,
                      const R               scale,
                      const std::string&    filepath,
                      const std::string&    filename,
                      mqi::vec3<mqi::ijk_t> dim,
                      uint32_t              num_spots) {
    uint32_t vol_size;
    vol_size = src->roi_->get_mask_size();
    /// create a copy using valarray and apply scale
    const std::string name_a = "indices.npy", name_b = "indptr.npy", name_c = "shape.npy",
                      name_d = "data.npy", name_e = "format.npy";

    std::vector<double>*              value_vec = new std::vector<double>[vol_size];
    std::vector<mqi::key_t>*          spot_vec  = new std::vector<mqi::key_t>[vol_size];
    std::vector<double>               data_vec;
    std::vector<uint32_t>             indices_vec;
    std::vector<uint32_t>             indptr_vec;
    mqi::key_t                        vox_ind, spot_ind;
    double                            value;
    int                               spot_start = 0, spot_end = 0;
    std::vector<double>::iterator     it_data;
    std::vector<uint32_t>::iterator   it_ind;
    std::vector<mqi::key_t>::iterator it_spot;
    int                               spot_count;
    printf("save_to_npz\n");

    printf("scan start %d\n", src->max_capacity_);
    for (int ind = 0; ind < src->max_capacity_; ind++) {
        if (src->data_[ind].key1 != mqi::empty_pair && src->data_[ind].key2 != mqi::empty_pair) {
            vox_ind = src->data_[ind].key1;
            vox_ind = src->roi_->get_mask_idx(vox_ind);
            if (vox_ind < 0) {
                printf("is this right?\n");
                continue;
            }
            spot_ind = src->data_[ind].key2;
            assert(vox_ind >= 0 && vox_ind < vol_size);
            value = src->data_[ind].value;
            assert(value > 0);
            value_vec[vox_ind].push_back(value * scale);
            spot_vec[vox_ind].push_back(spot_ind);
        }
    }
    printf("Sorting start\n");
    for (int ind = 0; ind < vol_size; ind++) {
        if (spot_vec[ind].size() > 1) {
            std::vector<int> sort_ind(spot_vec[ind].size());
            std::iota(sort_ind.begin(), sort_ind.end(), 0);
            sort(sort_ind.begin(), sort_ind.end(), [&](int i, int j) {
                return spot_vec[ind][i] < spot_vec[ind][j];
            });
            std::vector<double>     sorted_value(spot_vec[ind].size());
            std::vector<mqi::key_t> sorted_spot(spot_vec[ind].size());
            for (int sorted_ind = 0; sorted_ind < spot_vec[ind].size(); sorted_ind++) {
                sorted_value[sorted_ind] = value_vec[ind][sort_ind[sorted_ind]];
                sorted_spot[sorted_ind]  = spot_vec[ind][sort_ind[sorted_ind]];
            }
            spot_vec[ind]  = sorted_spot;
            value_vec[ind] = sorted_value;
        }
    }

    spot_count = 0;
    indptr_vec.push_back(spot_count);
    for (int ii = 0; ii < vol_size; ii++) {
        data_vec.insert(data_vec.end(), value_vec[ii].begin(), value_vec[ii].end());
        indices_vec.insert(indices_vec.end(), spot_vec[ii].begin(), spot_vec[ii].end());
        spot_count += spot_vec[ii].size();
        indptr_vec.push_back(spot_count);
    }

    uint32_t    shape[2] = { vol_size, num_spots };
    std::string format   = "csr";
    size_t      size_a = indices_vec.size(), size_b = indptr_vec.size(), size_c = 2,
           size_d = data_vec.size(), size_e = 3;

    uint32_t* indices = new uint32_t[indices_vec.size()];
    uint32_t* indptr  = new uint32_t[indptr_vec.size()];
    double*   data    = new double[data_vec.size()];
    std::copy(indices_vec.begin(), indices_vec.end(), indices);
    std::copy(indptr_vec.begin(), indptr_vec.end(), indptr);
    std::copy(data_vec.begin(), data_vec.end(), data);
    printf("%lu\n", size_b);
    mqi::io::save_npz(filepath + "/" + filename + ".npz", name_a, indices, size_a, "w");
    mqi::io::save_npz(filepath + "/" + filename + ".npz", name_b, indptr, size_b, "a");
    mqi::io::save_npz(filepath + "/" + filename + ".npz", name_c, shape, size_c, "a");
    mqi::io::save_npz(filepath + "/" + filename + ".npz", name_d, data, size_d, "a");
    mqi::io::save_npz(filepath + "/" + filename + ".npz", name_e, format, size_e, "a");
}

template<typename R>
void
mqi::io::save_to_npz(const mqi::scorer<R>* src,
                     const R               scale,
                     const std::string&    filepath,
                     const std::string&    filename,
                     mqi::vec3<mqi::ijk_t> dim,
                     uint32_t              num_spots,
                     R*                    time_scale,
                     R                     threshold) {
    uint32_t vol_size;
    vol_size = dim.x * dim.y * dim.z;
    /// create a copy using valarray and apply scale
    const std::string name_a = "indices.npy", name_b = "indptr.npy", name_c = "shape.npy",
                      name_d = "data.npy", name_e = "format.npy";
    std::vector<double>               value_vec[num_spots];
    std::vector<mqi::key_t>           vox_vec[num_spots];
    std::vector<double>               data_vec;
    std::vector<uint32_t>             indices_vec;
    std::vector<uint32_t>             indptr_vec;
    mqi::key_t                        vox_ind, spot_ind;
    double                            value;
    int                               spot_start = 0, spot_end = 0;
    int                               vox_in_spot[num_spots];
    std::vector<double>::iterator     it_data;
    std::vector<uint32_t>::iterator   it_ind;
    std::vector<mqi::key_t>::iterator it_spot;
    int                               vox_count;
    printf("save_to_npz\n");
    for (int ind = 0; ind < num_spots; ind++) {
        vox_in_spot[ind] = 0;
    }
    printf("scan start %d\n", src->max_capacity_);
    for (int ind = 0; ind < src->max_capacity_; ind++) {
        if (src->data_[ind].key1 != mqi::empty_pair && src->data_[ind].key2 != mqi::empty_pair) {
            vox_count = 0;
            vox_ind   = src->data_[ind].key1;
            spot_ind  = src->data_[ind].key2;
            assert(vox_ind >= 0 && vox_ind < vol_size);
            value = src->data_[ind].value;
            value *= scale;
            value -= 2 * threshold;
            if (value < 0) value = 0;
            value /= time_scale[spot_ind];
            value_vec[spot_ind].push_back(value);
            vox_vec[spot_ind].push_back(vox_ind);
        }
    }

    vox_count = 0;
    indptr_vec.push_back(vox_count);
    for (int ii = 0; ii < num_spots; ii++) {
        data_vec.insert(data_vec.end(), value_vec[ii].begin(), value_vec[ii].end());
        indices_vec.insert(indices_vec.end(), vox_vec[ii].begin(), vox_vec[ii].end());
        vox_count += vox_vec[ii].size();
        indptr_vec.push_back(vox_count);
    }
    printf("scan done %lu %lu %lu\n", data_vec.size(), indices_vec.size(), indptr_vec.size());
    printf("%d %d\n", vol_size, num_spots);

    uint32_t    shape[2] = { num_spots, vol_size };
    std::string format   = "csr";
    size_t      size_a = indices_vec.size(), size_b = indptr_vec.size(), size_c = 2,
           size_d = data_vec.size(), size_e = 3;

    uint32_t* indices = new uint32_t[indices_vec.size()];
    uint32_t* indptr  = new uint32_t[indptr_vec.size()];
    double*   data    = new double[data_vec.size()];
    std::copy(indices_vec.begin(), indices_vec.end(), indices);
    std::copy(indptr_vec.begin(), indptr_vec.end(), indptr);
    std::copy(data_vec.begin(), data_vec.end(), data);
    printf("%lu\n", size_b);
    mqi::io::save_npz(filepath + "/" + filename + ".npz", name_a, indices, size_a, "w");
    mqi::io::save_npz(filepath + "/" + filename + ".npz", name_b, indptr, size_b, "a");
    mqi::io::save_npz(filepath + "/" + filename + ".npz", name_c, shape, size_c, "a");
    mqi::io::save_npz(filepath + "/" + filename + ".npz", name_d, data, size_d, "a");
    mqi::io::save_npz(filepath + "/" + filename + ".npz", name_e, format, size_e, "a");
}

template<typename R>
void
mqi::io::save_to_mhd(const mqi::node_t<R>* children,
                     const double*         src,
                     const R               scale,
                     const std::string&    filepath,
                     const std::string&    filename,
                     const uint32_t        length) {
    ///< TODO: this works only for two depth world
    ///< TODO: dx, dy, and dz calculation works only for AABB
    float dx = children->geo[0].get_x_edges()[1];
    dx -= children->geo[0].get_x_edges()[0];
    float dy = children->geo[0].get_y_edges()[1];
    dy -= children->geo[0].get_y_edges()[0];
    float dz = children->geo[0].get_z_edges()[1];
    dz -= children->geo[0].get_z_edges()[0];
    float x0 = children->geo[0].get_x_edges()[0];
    x0 += children->geo[0].get_x_edges()[0];
    x0 /= 2.0;
    float y0 = children->geo[0].get_y_edges()[0];
    y0 += children->geo[0].get_y_edges()[0];
    y0 /= 2.0;
    float z0 = children->geo[0].get_z_edges()[0];
    z0 += children->geo[0].get_z_edges()[0];
    z0 /= 2.0;
    std::ofstream fid_header(filepath + "/" + filename + ".mhd", std::ios::out);
    if (!fid_header) { std::cout << "Cannot open file!" << std::endl; }
    fid_header << "ObjectType = Image\n";
    fid_header << "NDims = 3\n";
    fid_header << "BinaryData = True\n";
    fid_header
      << "BinaryDataByteOrderMSB = False\n";   // True for big endian, False for little endian
    fid_header << "CompressedData = False\n";
    fid_header << "TransformMatrix 1 0 0 0 1 0 0 0 1\n";
    fid_header << "Offset " << x0 << " " << y0 << " " << z0 << std::endl;
    fid_header << "CenterOfRotation 0 0 0\n";
    fid_header << "AnatomicOrientation = RAI\n";
    fid_header << "DimSize = " << children->geo[0].get_nxyz().x << " "
               << children->geo[0].get_nxyz().y << " " << children->geo[0].get_nxyz().z << "\n";
    ///< TODO: if R is double, MET_FLOAT should be MET_DOUBLE
    fid_header << "ElementType = MET_DOUBLE\n";

    fid_header << "ElementSpacing = " << dx << " " << dy << " " << dz << "\n";
    fid_header << "ElementDataFile = " << filename << ".raw"
               << "\n";
    fid_header.close();
    if (!fid_header.good()) { std::cout << "Error occurred at writing time!" << std::endl; }
    std::valarray<double> dest(src, length);
    munmap(&dest, length * sizeof(double));
    dest *= scale;
    std::ofstream fid_raw(filepath + "/" + filename + ".raw", std::ios::out | std::ios::binary);
    if (!fid_raw) { std::cout << "Cannot open file!" << std::endl; }
    fid_raw.write(reinterpret_cast<const char*>(&dest[0]), length * sizeof(double));

    fid_raw.close();
    if (!fid_raw.good()) { std::cout << "Error occurred at writing time!" << std::endl; }
}

template<typename R>
void
mqi::io::save_to_mha(const mqi::node_t<R>* children,
                     const double*         src,
                     const R               scale,
                     const std::string&    filepath,
                     const std::string&    filename,
                     const uint32_t        length) {
    ///< TODO: this works only for two depth world
    ///< TODO: dx, dy, and dz calculation works only for AABB
    float dx = children->geo[0].get_x_edges()[1];
    dx -= children->geo[0].get_x_edges()[0];
    float dy = children->geo[0].get_y_edges()[1];
    dy -= children->geo[0].get_y_edges()[0];
    float dz = children->geo[0].get_z_edges()[1];
    dz -= children->geo[0].get_z_edges()[0];
    float x0 = children->geo[0].get_x_edges()[0] + dx * 0.5;
    float y0 = children->geo[0].get_y_edges()[0] + dy * 0.5;
    float z0 = children->geo[0].get_z_edges()[0] + dz * 0.5;
    std::cout << "x0 " << std::setprecision(9) << x0 << " y0 " << y0 << " z0 " << z0 << std::endl;
    std::valarray<double> dest(src, length);
    munmap(&dest, length * sizeof(double));
    dest *= scale;
    std::ofstream fid_header(filepath + "/" + filename + ".mha", std::ios::out);
    if (!fid_header) { std::cout << "Cannot open file!" << std::endl; }
    fid_header << "ObjectType = Image\n";
    fid_header << "NDims = 3\n";
    fid_header << "BinaryData = True\n";
    fid_header
      << "BinaryDataByteOrderMSB = False\n";   // True for big endian, False for little endian
    fid_header << "CompressedData = False\n";
    fid_header << "TransformMatrix = 1 0 0 0 1 0 0 0 1\n";
    fid_header << "Origin = " << std::setprecision(9) << x0 << " " << y0 << " " << z0 << "\n";
    fid_header << "CenterOfRotation = 0 0 0\n";
    fid_header << "AnatomicOrientation = RAI\n";
    fid_header << "DimSize = " << children->geo[0].get_nxyz().x << " "
               << children->geo[0].get_nxyz().y << " " << children->geo[0].get_nxyz().z << "\n";
    ///< TODO: if R is double, MET_FLOAT should be MET_DOUBLE
    fid_header << "ElementType = MET_DOUBLE\n";
    fid_header << "HeaderSize = -1\n";
    fid_header << "ElementSpacing = " << std::setprecision(9) << dx << " " << dy << " " << dz
               << "\n";
    fid_header << "ElementDataFile = LOCAL\n";
    fid_header.write(reinterpret_cast<const char*>(&dest[0]), length * sizeof(double));
    fid_header.close();
    if (!fid_header.good()) { std::cout << "Error occurred at writing time!" << std::endl; }
}

template<typename R>
void
mqi::io::save_to_dcm(const mqi::scorer<R>* src,
                     const R               scale,
                     const std::string&    filepath,
                     const std::string&    filename,
                     const uint32_t        length,
                     const mqi::vec3<ijk_t>& dim,
                     const bool            is_2cm_mode) {
    // Create a copy of scorer data and apply scale
    std::vector<double> dose_data;
    dose_data.resize(dim.x * dim.y * dim.z, 0.0);
    
    // Extract data from scorer
    for (int ind = 0; ind < src->max_capacity_; ind++) {
        if (src->data_[ind].key1 != mqi::empty_pair &&
            src->data_[ind].key2 != mqi::empty_pair &&
            src->data_[ind].value > 0) {
            if (src->data_[ind].key1 < dose_data.size()) {
                dose_data[src->data_[ind].key1] += src->data_[ind].value * scale;
            }
        }
    }
    
    // Calculate maximum dose for scaling
    double max_dose = 0.0;
    for (size_t i = 0; i < dose_data.size(); i++) {
        if (dose_data[i] > max_dose) max_dose = dose_data[i];
    }
    
    // Scale to 16-bit unsigned integer (DICOM compliant)
    double scale_factor = (max_dose > 0) ? 65535.0 / max_dose : 1.0;
    std::vector<uint16_t> pixel_data;
    pixel_data.resize(dose_data.size());
    
    for (size_t i = 0; i < dose_data.size(); i++) {
        pixel_data[i] = static_cast<uint16_t>(dose_data[i] * scale_factor);
    }
    
    // Create DICOM file
    gdcm::File file;
    gdcm::DataSet& ds = file.GetDataSet();
    
    // File Meta Information
    gdcm::FileMetaInformation& fmi = file.GetHeader();
    fmi.SetDataSetTransferSyntax(gdcm::TransferSyntax::ImplicitVRLittleEndian);
    
    // SOP Class UID for RT Dose Storage
    gdcm::DataElement sop_class_uid(gdcm::Tag(0x0008, 0x0016));
    sop_class_uid.SetVR(gdcm::VR::UI);
    sop_class_uid.SetByteValue("1.2.840.10008.5.1.4.1.1.481.2", strlen("1.2.840.10008.5.1.4.1.1.481.2"));
    ds.Insert(sop_class_uid);
    
    // SOP Instance UID (generate unique)
    gdcm::UIDGenerator uid_generator;
    std::string sop_instance_uid = uid_generator.Generate();
    gdcm::DataElement sop_instance_uid_elem(gdcm::Tag(0x0008, 0x0018));
    sop_instance_uid_elem.SetVR(gdcm::VR::UI);
    sop_instance_uid_elem.SetByteValue(sop_instance_uid.c_str(), sop_instance_uid.length());
    ds.Insert(sop_instance_uid_elem);
    
    // Study Instance UID (generate unique)
    std::string study_instance_uid = uid_generator.Generate();
    gdcm::DataElement study_instance_uid_elem(gdcm::Tag(0x0020, 0x000D));
    study_instance_uid_elem.SetVR(gdcm::VR::UI);
    study_instance_uid_elem.SetByteValue(study_instance_uid.c_str(), study_instance_uid.length());
    ds.Insert(study_instance_uid_elem);
    
    // Series Instance UID (generate unique)
    std::string series_instance_uid = uid_generator.Generate();
    gdcm::DataElement series_instance_uid_elem(gdcm::Tag(0x0020, 0x000E));
    series_instance_uid_elem.SetVR(gdcm::VR::UI);
    series_instance_uid_elem.SetByteValue(series_instance_uid.c_str(), series_instance_uid.length());
    ds.Insert(series_instance_uid_elem);
    
    // Modality
    gdcm::DataElement modality(gdcm::Tag(0x0008, 0x0060));
    modality.SetVR(gdcm::VR::CS);
    modality.SetByteValue("RTDOSE", strlen("RTDOSE"));
    ds.Insert(modality);
    
    // Series Number
    gdcm::DataElement series_number(gdcm::Tag(0x0020, 0x0011));
    series_number.SetVR(gdcm::VR::IS);
    series_number.SetByteValue("1", strlen("1"));
    ds.Insert(series_number);
    
    // Image dimensions
    gdcm::DataElement rows(gdcm::Tag(0x0028, 0x0010));
    rows.SetVR(gdcm::VR::US);
    uint16_t rows_val = static_cast<uint16_t>(dim.y);
    rows.SetByteValue(reinterpret_cast<const char*>(&rows_val), sizeof(uint16_t));
    ds.Insert(rows);
    
    gdcm::DataElement columns(gdcm::Tag(0x0028, 0x0011));
    columns.SetVR(gdcm::VR::US);
    uint16_t cols_val = static_cast<uint16_t>(dim.x);
    columns.SetByteValue(reinterpret_cast<const char*>(&cols_val), sizeof(uint16_t));
    ds.Insert(columns);
    
    // Pixel Spacing (assuming 1mm spacing for now)
    std::ostringstream pixel_spacing_stream;
    pixel_spacing_stream << std::fixed << std::setprecision(6) << "1.0\\1.0";
    std::string pixel_spacing_str = pixel_spacing_stream.str();
    gdcm::DataElement pixel_spacing(gdcm::Tag(0x0028, 0x0030));
    pixel_spacing.SetVR(gdcm::VR::DS);
    pixel_spacing.SetByteValue(pixel_spacing_str.c_str(), pixel_spacing_str.length());
    ds.Insert(pixel_spacing);
    
    // Image Position Patient (assuming origin at 0,0,0)
    std::ostringstream image_pos_stream;
    image_pos_stream << std::fixed << std::setprecision(6) << "0.0\\0.0\\0.0";
    std::string image_pos_str = image_pos_stream.str();
    gdcm::DataElement image_position(gdcm::Tag(0x0020, 0x0032));
    image_position.SetVR(gdcm::VR::DS);
    image_position.SetByteValue(image_pos_str.c_str(), image_pos_str.length());
    ds.Insert(image_position);
    
    // Slice Thickness
    gdcm::DataElement slice_thickness(gdcm::Tag(0x0018, 0x0050));
    slice_thickness.SetVR(gdcm::VR::DS);
    slice_thickness.SetByteValue("1.0", strlen("1.0"));
    ds.Insert(slice_thickness);
    
    // RT Dose specific tags
    gdcm::DataElement dose_units(gdcm::Tag(0x300A, 0x0002));
    dose_units.SetVR(gdcm::VR::CS);
    dose_units.SetByteValue("GY", strlen("GY"));
    ds.Insert(dose_units);
    
    gdcm::DataElement dose_type(gdcm::Tag(0x300A, 0x0004));
    dose_type.SetVR(gdcm::VR::CS);
    dose_type.SetByteValue("PHYSICAL", strlen("PHYSICAL"));
    ds.Insert(dose_type);
    
    gdcm::DataElement dose_summation_type(gdcm::Tag(0x300A, 0x0006));
    dose_summation_type.SetVR(gdcm::VR::CS);
    dose_summation_type.SetByteValue("VOLUME", strlen("VOLUME"));
    ds.Insert(dose_summation_type);
    
    // Dose Grid Scaling
    double dose_grid_scaling = (max_dose > 0) ? 1.0 / scale_factor : 1.0;
    std::ostringstream dose_grid_stream;
    dose_grid_stream << std::fixed << std::setprecision(10) << dose_grid_scaling;
    std::string dose_grid_str = dose_grid_stream.str();
    gdcm::DataElement dose_grid_scaling_elem(gdcm::Tag(0x3004, 0x000A));
    dose_grid_scaling_elem.SetVR(gdcm::VR::DS);
    dose_grid_scaling_elem.SetByteValue(dose_grid_str.c_str(), dose_grid_str.length());
    ds.Insert(dose_grid_scaling_elem);
    
    // Image Pixel Module
    gdcm::DataElement bits_allocated(gdcm::Tag(0x0028, 0x0100));
    bits_allocated.SetVR(gdcm::VR::US);
    uint16_t bits_alloc_val = 16;
    bits_allocated.SetByteValue(reinterpret_cast<const char*>(&bits_alloc_val), sizeof(uint16_t));
    ds.Insert(bits_allocated);
    
    gdcm::DataElement bits_stored(gdcm::Tag(0x0028, 0x0101));
    bits_stored.SetVR(gdcm::VR::US);
    uint16_t bits_stored_val = 16;
    bits_stored.SetByteValue(reinterpret_cast<const char*>(&bits_stored_val), sizeof(uint16_t));
    ds.Insert(bits_stored);
    
    gdcm::DataElement high_bit(gdcm::Tag(0x0028, 0x0102));
    high_bit.SetVR(gdcm::VR::US);
    uint16_t high_bit_val = 15;
    high_bit.SetByteValue(reinterpret_cast<const char*>(&high_bit_val), sizeof(uint16_t));
    ds.Insert(high_bit);
    
    gdcm::DataElement pixel_representation(gdcm::Tag(0x0028, 0x0103));
    pixel_representation.SetVR(gdcm::VR::US);
    uint16_t pixel_rep_val = 0; // unsigned
    pixel_representation.SetByteValue(reinterpret_cast<const char*>(&pixel_rep_val), sizeof(uint16_t));
    ds.Insert(pixel_representation);
    
    gdcm::DataElement rescale_intercept(gdcm::Tag(0x0028, 0x1052));
    rescale_intercept.SetVR(gdcm::VR::DS);
    rescale_intercept.SetByteValue("0.0", strlen("0.0"));
    ds.Insert(rescale_intercept);
    
    gdcm::DataElement rescale_slope(gdcm::Tag(0x0028, 0x1053));
    rescale_slope.SetVR(gdcm::VR::DS);
    rescale_slope.SetByteValue(dose_grid_str.c_str(), dose_grid_str.length());
    ds.Insert(rescale_slope);
    
    // Number of Frames
    gdcm::DataElement number_of_frames(gdcm::Tag(0x0028, 0x0008));
    number_of_frames.SetVR(gdcm::VR::IS);
    std::string frames_str = is_2cm_mode ? "1" : std::to_string(dim.z);
    number_of_frames.SetByteValue(frames_str.c_str(), frames_str.length());
    ds.Insert(number_of_frames);
    
    // Pixel Data
    gdcm::DataElement pixel_data_elem(gdcm::Tag(0x7FE0, 0x0010));
    pixel_data_elem.SetVR(gdcm::VR::OW);
    pixel_data_elem.SetByteValue(reinterpret_cast<const char*>(pixel_data.data()),
                               pixel_data.size() * sizeof(uint16_t));
    ds.Insert(pixel_data_elem);
    
    // Write the file
    gdcm::Writer writer;
    writer.SetFile(file);
    writer.SetFileName((filepath + "/" + filename + ".dcm").c_str());
    
    if (!writer.Write()) {
        std::cout << "Error: Failed to write DICOM file: " << filepath + "/" + filename + ".dcm" << std::endl;
    } else {
        std::cout << "Successfully wrote DICOM file: " << filepath + "/" + filename + ".dcm" << std::endl;
    }
}

#endif
