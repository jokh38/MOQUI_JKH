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
#include "gdcmDataElement.h"
#include "gdcmDataSet.h"
#include "gdcmFile.h"
#include "gdcmImage.h"
#include "gdcmImageWriter.h"
#include "gdcmUIDGenerator.h"

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
    // ================================================================================
    // PHASE 1: Data Preparation
    // ================================================================================
    // Extract dose data from scorer hash table and convert to DICOM-compliant format.
    // This phase follows the same pattern as save_to_bin():
    // - Extract scorer data into std::vector
    // - Apply user-specified scaling factor
    // - Convert to 16-bit unsigned integer (DICOM RT Dose standard)
    // - Calculate Dose Grid Scaling for accurate dose reconstruction

    // Create dose data array from sparse scorer structure
    std::vector<double> dose_data;
    size_t actual_size = static_cast<size_t>(dim.x) * dim.y * dim.z;
    dose_data.resize(actual_size, 0.0);

    // Extract and accumulate dose values from hash table
    for (int ind = 0; ind < src->max_capacity_; ind++) {
        if (src->data_[ind].key1 != mqi::empty_pair &&
            src->data_[ind].key2 != mqi::empty_pair &&
            src->data_[ind].value > 0) {
            mqi::key_t key = src->data_[ind].key1;
            if (key < actual_size) {
                dose_data[key] += src->data_[ind].value * scale;
            } else {
                std::cout << "Warning: key out of bounds: " << key << " >= " << actual_size << std::endl;
            }
        }
    }

    // Find maximum dose for dynamic range scaling
    double max_dose = 0.0;
    for (size_t i = 0; i < dose_data.size(); i++) {
        if (dose_data[i] > max_dose) max_dose = dose_data[i];
    }

    std::cout << "DCM Save Info - Dimension: (" << dim.x << ", " << dim.y << ", " << dim.z << ")" << std::endl;
    std::cout << "DCM Save Info - Data size: " << dose_data.size() << " voxels" << std::endl;
    std::cout << "DCM Save Info - Max dose: " << max_dose << std::endl;
    std::cout << "DCM Save Info - 2cm mode: " << (is_2cm_mode ? "true" : "false") << std::endl;

    // Scale dose values to 16-bit unsigned integer range [0, 65535]
    // Dose Grid Scaling tag (0x3004,0x000A) stores the inverse to reconstruct actual dose
    double scale_factor = (max_dose > 0) ? 65535.0 / max_dose : 1.0;
    double dose_grid_scaling = (max_dose > 0) ? 1.0 / scale_factor : 1.0;

    // Convert floating-point dose to 16-bit unsigned integer pixel data
    std::vector<uint16_t> pixel_data;
    pixel_data.resize(dose_data.size());

    for (size_t i = 0; i < dose_data.size(); i++) {
        pixel_data[i] = static_cast<uint16_t>(dose_data[i] * scale_factor);
    }

    // ================================================================================
    // PHASE 2: DICOM Metadata Preparation
    // ================================================================================
    // Prepare all metadata strings with proper lifetime management.
    // All strings must remain valid until ImageWriter.Write() completes.

    // Generate unique DICOM identifiers
    gdcm::UIDGenerator uid_generator;
    std::string sop_instance_uid = uid_generator.Generate();
    std::string study_instance_uid = uid_generator.Generate();
    std::string series_instance_uid = uid_generator.Generate();

    // Format spatial information strings (DICOM uses backslash separator)
    std::ostringstream pixel_spacing_stream;
    pixel_spacing_stream << std::fixed << std::setprecision(6) << "1.0\\1.0";
    std::string pixel_spacing_str = pixel_spacing_stream.str();

    std::ostringstream image_pos_stream;
    image_pos_stream << std::fixed << std::setprecision(6) << "0.0\\0.0\\0.0";
    std::string image_pos_str = image_pos_stream.str();

    // Format Dose Grid Scaling with high precision
    std::ostringstream dose_grid_stream;
    dose_grid_stream << std::fixed << std::setprecision(10) << dose_grid_scaling;
    std::string dose_grid_str = dose_grid_stream.str();

    std::string frames_str = std::to_string(dim.z);
    std::string output_filename = filepath + "/" + filename + ".dcm";

    // ================================================================================
    // PHASE 3: DICOM File Writing using Image API (Memory-Safe Approach)
    // ================================================================================
    //
    // MEMORY MANAGEMENT STRATEGY:
    // ---------------------------
    // This implementation uses gdcm::Image API instead of low-level DataElement API
    // to solve the "free(): invalid size" memory corruption error.
    //
    // ROOT CAUSE OF PREVIOUS ERROR:
    // - Old approach: DataElement::SetByteValue(pixel_data.data(), size)
    // - Problem: GDCM stored a POINTER to std::vector's internal buffer
    // - When File destructs, GDCM tries to free() the buffer
    // - But the buffer was allocated by std::vector (C++ new[])
    // - Allocator mismatch causes: "free(): invalid size" crash
    //
    // SOLUTION - Image API:
    // - gdcm::Image::SetBuffer() COPIES pixel data into GDCM's internal storage
    // - GDCM owns the copied data and manages its lifetime
    // - Our std::vector destructs independently without conflict
    // - Clean separation of ownership prevents memory corruption
    //
    // IMPLEMENTATION PATTERN:
    // 1. Create gdcm::Image and configure pixel format
    // 2. Image copies pixel data (via SetBuffer)
    // 3. Create gdcm::File and add RT Dose specific tags manually
    // 4. Use gdcm::ImageWriter to write File + Image together
    // 5. Safe destruction: Writer → Image → File → pixel_data vector
    //
    // NOTE: We use HYBRID approach because:
    // - Image API handles pixel data memory safely
    // - Manual tags preserve RT Dose Storage (SOP Class 1.2.840.10008.5.1.4.1.1.481.2)
    // - Pure Image API would create CT/MR images, not RT Dose
    // ================================================================================

    {   // Scope for GDCM objects (ensures proper destruction order)

        // Step 1: Create Image object and configure pixel data
        // -------------------------------------------------------
        // gdcm::Image manages pixel data with proper memory ownership
        gdcm::Image image;
        image.SetNumberOfDimensions(3);  // RT Dose is always 3D (multi-frame)
        image.SetDimension(0, static_cast<unsigned int>(dim.x));  // Columns
        image.SetDimension(1, static_cast<unsigned int>(dim.y));  // Rows
        image.SetDimension(2, static_cast<unsigned int>(dim.z));  // Frames

        // Configure pixel format for 16-bit grayscale
        image.SetPixelFormat(gdcm::PixelFormat::UINT16);
        image.SetPhotometricInterpretation(gdcm::PhotometricInterpretation::MONOCHROME2);

        // CRITICAL: In GDCM 3.0, SetBuffer() was removed. We need to create a DataElement
        // for pixel data and insert it into the DataSet directly
        size_t pixel_data_size = pixel_data.size() * sizeof(uint16_t);
        
        // Create Pixel Data element (0x7FE0, 0x0010) and set the pixel data
        gdcm::DataElement pixel_element(gdcm::Tag(0x7FE0, 0x0010));
        pixel_element.SetVR(gdcm::VR::OB);
        pixel_element.SetByteValue(reinterpret_cast<const char*>(pixel_data.data()), pixel_data_size);
        

        // Step 2: Create File and set File Meta Information
        // ---------------------------------------------------
        gdcm::File file;
        gdcm::FileMetaInformation& fmi = file.GetHeader();

        // In GDCM 3.0, SetMediaStorageSOPClassUID and SetMediaStorageSOPInstanceUID were removed.
        // We need to create DataElements for these and insert them into FileMetaInformation
        
        // Media Storage SOP Class UID (0x0002, 0x0002)
        gdcm::DataElement media_storage_class(gdcm::Tag(0x0002, 0x0002));
        media_storage_class.SetVR(gdcm::VR::UI);
        media_storage_class.SetByteValue("1.2.840.10008.5.1.4.1.1.481.2",
                                   strlen("1.2.840.10008.5.1.4.1.1.481.2"));
        fmi.Insert(media_storage_class);
        
        // Media Storage SOP Instance UID (0x0002, 0x0003)
        gdcm::DataElement media_storage_instance(gdcm::Tag(0x0002, 0x0003));
        media_storage_instance.SetVR(gdcm::VR::UI);
        media_storage_instance.SetByteValue(sop_instance_uid.c_str(), sop_instance_uid.length());
        fmi.Insert(media_storage_instance);
        
        fmi.SetDataSetTransferSyntax(gdcm::TransferSyntax::ImplicitVRLittleEndian);

        // Step 3: Add DICOM tags to DataSet
        // ---------------------------------------------------
        // Image API doesn't automatically create RT Dose specific tags,
        // so we add them manually to ensure DICOM compliance
        gdcm::DataSet& ds = file.GetDataSet();

        // Insert the pixel data element into the file's dataset
        ds.Insert(pixel_element);

        // SOP Class UID (must match FileMetaInformation)
        gdcm::DataElement sop_class_uid(gdcm::Tag(0x0008, 0x0016));
        sop_class_uid.SetVR(gdcm::VR::UI);
        sop_class_uid.SetByteValue("1.2.840.10008.5.1.4.1.1.481.2", strlen("1.2.840.10008.5.1.4.1.1.481.2"));
        ds.Insert(sop_class_uid);

        // SOP Instance UID
        gdcm::DataElement sop_instance_uid_elem(gdcm::Tag(0x0008, 0x0018));
        sop_instance_uid_elem.SetVR(gdcm::VR::UI);
        sop_instance_uid_elem.SetByteValue(sop_instance_uid.c_str(), sop_instance_uid.length());
        ds.Insert(sop_instance_uid_elem);

        // Study Instance UID
        gdcm::DataElement study_instance_uid_elem(gdcm::Tag(0x0020, 0x000D));
        study_instance_uid_elem.SetVR(gdcm::VR::UI);
        study_instance_uid_elem.SetByteValue(study_instance_uid.c_str(), study_instance_uid.length());
        ds.Insert(study_instance_uid_elem);

        // Series Instance UID
        gdcm::DataElement series_instance_uid_elem(gdcm::Tag(0x0020, 0x000E));
        series_instance_uid_elem.SetVR(gdcm::VR::UI);
        series_instance_uid_elem.SetByteValue(series_instance_uid.c_str(), series_instance_uid.length());
        ds.Insert(series_instance_uid_elem);

        // Modality (RTDOSE for RT Dose Storage)
        gdcm::DataElement modality(gdcm::Tag(0x0008, 0x0060));
        modality.SetVR(gdcm::VR::CS);
        modality.SetByteValue("RTDOSE", strlen("RTDOSE"));
        ds.Insert(modality);

        // Series Number
        gdcm::DataElement series_number(gdcm::Tag(0x0020, 0x0011));
        series_number.SetVR(gdcm::VR::IS);
        series_number.SetByteValue("1", strlen("1"));
        ds.Insert(series_number);

        // Pixel Spacing (row spacing \ column spacing)
        gdcm::DataElement pixel_spacing(gdcm::Tag(0x0028, 0x0030));
        pixel_spacing.SetVR(gdcm::VR::DS);
        pixel_spacing.SetByteValue(pixel_spacing_str.c_str(), pixel_spacing_str.length());
        ds.Insert(pixel_spacing);

        // Image Position Patient (x \ y \ z)
        gdcm::DataElement image_position(gdcm::Tag(0x0020, 0x0032));
        image_position.SetVR(gdcm::VR::DS);
        image_position.SetByteValue(image_pos_str.c_str(), image_pos_str.length());
        ds.Insert(image_position);

        // Slice Thickness
        gdcm::DataElement slice_thickness(gdcm::Tag(0x0018, 0x0050));
        slice_thickness.SetVR(gdcm::VR::DS);
        slice_thickness.SetByteValue("1.0", strlen("1.0"));
        ds.Insert(slice_thickness);

        // RT Dose Module - REQUIRED tags for RT Dose Storage
        // ---------------------------------------------------

        // Dose Units (0x300A, 0x0002) - Type 1 (Required)
        gdcm::DataElement dose_units(gdcm::Tag(0x300A, 0x0002));
        dose_units.SetVR(gdcm::VR::CS);
        dose_units.SetByteValue("GY", strlen("GY"));
        ds.Insert(dose_units);

        // Dose Type (0x300A, 0x0004) - Type 1 (Required)
        gdcm::DataElement dose_type(gdcm::Tag(0x300A, 0x0004));
        dose_type.SetVR(gdcm::VR::CS);
        dose_type.SetByteValue("PHYSICAL", strlen("PHYSICAL"));
        ds.Insert(dose_type);

        // Dose Summation Type (0x300A, 0x0006) - Type 1 (Required)
        gdcm::DataElement dose_summation_type(gdcm::Tag(0x300A, 0x0006));
        dose_summation_type.SetVR(gdcm::VR::CS);
        dose_summation_type.SetByteValue("VOLUME", strlen("VOLUME"));
        ds.Insert(dose_summation_type);

        // Dose Grid Scaling (0x3004, 0x000A) - Type 1 (Required)
        // CRITICAL: This value determines dose accuracy
        // Actual dose = Pixel Value × Dose Grid Scaling
        gdcm::DataElement dose_grid_scaling_elem(gdcm::Tag(0x3004, 0x000A));
        dose_grid_scaling_elem.SetVR(gdcm::VR::DS);
        dose_grid_scaling_elem.SetByteValue(dose_grid_str.c_str(), dose_grid_str.length());
        ds.Insert(dose_grid_scaling_elem);

        // Step 4: Write DICOM file using ImageWriter
        // ---------------------------------------------------
        // ImageWriter handles both File metadata and Image pixel data
        {   // Inner scope ensures Writer destructs before Image and File
            gdcm::ImageWriter writer;
            writer.SetFileName(output_filename.c_str());
            writer.SetFile(file);
            writer.SetImage(image);  // Image contains copied pixel data

            bool write_success = writer.Write();

            if (!write_success) {
                std::cerr << "Failed to write DICOM file: " << output_filename << std::endl;
                return;
            }

            std::cout << "Successfully wrote DICOM file: " << output_filename << std::endl;

        }   // Writer destructs here (releases File and Image references)

    }   // Image and File destruct here (GDCM frees its copied pixel data)

    // pixel_data vector destructs here (independent from GDCM) - NO CRASH
}

#endif
