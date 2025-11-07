
#ifndef MQI_IO_DCM_HPP
#define MQI_IO_DCM_HPP

#include <algorithm>
#include <complex>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <valarray>
#include <zlib.h>
#include <sstream>
#include <ctime>

#include <sys/mman.h>

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
#include "gdcmItem.h"
#include "gdcmSequenceOfItems.h"

namespace mqi {
namespace io {

template<typename R>
void
save_to_dcm(const mqi::scorer<R>* src,
            const R               scale,
            const std::string&    filepath,
            const std::string&    filename,
            const uint32_t        length,
            const mqi::vec3<ijk_t>& dim,
            const bool            is_2cm_mode = false) {
    gdcm::UIDGenerator uid_generator;
    std::string sop_instance_uid = uid_generator.Generate();
    std::string study_instance_uid = uid_generator.Generate();
    std::string series_instance_uid = uid_generator.Generate();

    std::ostringstream pixel_spacing_stream;
    pixel_spacing_stream << std::fixed << std::setprecision(6) << "1.0\\1.0";
    std::string pixel_spacing_str = pixel_spacing_stream.str();

    std::ostringstream image_pos_stream;
    image_pos_stream << std::fixed << std::setprecision(6) << "0.0\\0.0\\0.0";
    std::string image_pos_str = image_pos_stream.str();

    std::string frames_str = std::to_string(dim.z);
    
    std::vector<double> dose_data;
    size_t actual_size = static_cast<size_t>(dim.x) * dim.y * dim.z;
    dose_data.resize(actual_size, 0.0);

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

    double max_dose = 0.0;
    for (size_t i = 0; i < dose_data.size(); i++) {
        if (dose_data[i] > max_dose) max_dose = dose_data[i];
    }

    std::cout << "DCM Save Info - Dimension: (" << dim.x << ", " << dim.y << ", " << dim.z << ")" << std::endl;
    std::cout << "DCM Save Info - Data size: " << dose_data.size() << " voxels" << std::endl;
    std::cout << "DCM Save Info - Max dose: " << max_dose << std::endl;
    std::cout << "DCM Save Info - 2cm mode: " << (is_2cm_mode ? "true" : "false") << std::endl;

    double scale_factor = (max_dose > 0) ? 65535.0 / max_dose : 1.0;
    
    double dose_grid_scaling = (max_dose > 0) ? 1.0 / scale_factor : 1.0;
    std::ostringstream dose_grid_stream;
    dose_grid_stream << std::fixed << std::setprecision(10) << dose_grid_scaling;
    std::string dose_grid_str = dose_grid_stream.str();

    std::vector<uint16_t> pixel_data;
    pixel_data.resize(dose_data.size());

    for (size_t i = 0; i < dose_data.size(); i++) {
        pixel_data[i] = static_cast<uint16_t>(dose_data[i] * scale_factor);
    }
    
    gdcm::SmartPointer<gdcm::File> file = new gdcm::File;
    gdcm::DataSet& ds = file->GetDataSet();
    
    gdcm::FileMetaInformation& fmi = file->GetHeader();
    fmi.SetDataSetTransferSyntax(gdcm::TransferSyntax::ImplicitVRLittleEndian);

    gdcm::DataElement sop_class_uid(gdcm::Tag(0x0008, 0x0016));
    sop_class_uid.SetVR(gdcm::VR::UI);
    const char* sop_class_uid_val = "1.2.840.10008.5.1.4.1.1.481.2";
    sop_class_uid.SetByteValue(sop_class_uid_val, strlen(sop_class_uid_val));
    ds.Insert(sop_class_uid);

    gdcm::DataElement sop_instance_uid_elem(gdcm::Tag(0x0008, 0x0018));
    sop_instance_uid_elem.SetVR(gdcm::VR::UI);
    sop_instance_uid_elem.SetByteValue(sop_instance_uid.c_str(), sop_instance_uid.length());
    ds.Insert(sop_instance_uid_elem);

    gdcm::DataElement study_instance_uid_elem(gdcm::Tag(0x0020, 0x000D));
    study_instance_uid_elem.SetVR(gdcm::VR::UI);
    study_instance_uid_elem.SetByteValue(study_instance_uid.c_str(), study_instance_uid.length());
    ds.Insert(study_instance_uid_elem);

    gdcm::DataElement series_instance_uid_elem(gdcm::Tag(0x0020, 0x000E));
    series_instance_uid_elem.SetVR(gdcm::VR::UI);
    series_instance_uid_elem.SetByteValue(series_instance_uid.c_str(), series_instance_uid.length());
    ds.Insert(series_instance_uid_elem);

    gdcm::DataElement modality(gdcm::Tag(0x0008, 0x0060));
    modality.SetVR(gdcm::VR::CS);
    modality.SetByteValue("RTDOSE", strlen("RTDOSE"));
    ds.Insert(modality);
    
    gdcm::DataElement series_number(gdcm::Tag(0x0020, 0x0011));
    series_number.SetVR(gdcm::VR::IS);
    series_number.SetByteValue("1", strlen("1"));
    ds.Insert(series_number);
    
    gdcm::DataElement rows(gdcm::Tag(0x0028, 0x0010));
    rows.SetVR(gdcm::VR::US);
    uint16_t rows_val = static_cast<uint16_t>(dim.y);
    rows.SetByteValue((char*)&rows_val, sizeof(uint16_t));
    ds.Insert(rows);

    gdcm::DataElement columns(gdcm::Tag(0x0028, 0x0011));
    columns.SetVR(gdcm::VR::US);
    uint16_t cols_val = static_cast<uint16_t>(dim.x);
    columns.SetByteValue((char*)&cols_val, sizeof(uint16_t));
    ds.Insert(columns);

    gdcm::DataElement pixel_spacing(gdcm::Tag(0x0028, 0x0030));
    pixel_spacing.SetVR(gdcm::VR::DS);
    pixel_spacing.SetByteValue(pixel_spacing_str.c_str(), pixel_spacing_str.length());
    ds.Insert(pixel_spacing);

    gdcm::DataElement image_position(gdcm::Tag(0x0020, 0x0032));
    image_position.SetVR(gdcm::VR::DS);
    image_position.SetByteValue(image_pos_str.c_str(), image_pos_str.length());
    ds.Insert(image_position);

    gdcm::DataElement slice_thickness(gdcm::Tag(0x0018, 0x0050));
    slice_thickness.SetVR(gdcm::VR::DS);
    slice_thickness.SetByteValue("1.0", strlen("1.0"));
    ds.Insert(slice_thickness);

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

    gdcm::DataElement dose_grid_scaling_elem(gdcm::Tag(0x3004, 0x000A));
    dose_grid_scaling_elem.SetVR(gdcm::VR::DS);
    dose_grid_scaling_elem.SetByteValue(dose_grid_str.c_str(), dose_grid_str.length());
    ds.Insert(dose_grid_scaling_elem);

    gdcm::DataElement samples_per_pixel(gdcm::Tag(0x0028, 0x0002));
    samples_per_pixel.SetVR(gdcm::VR::US);
    uint16_t samples_val = 1;
    samples_per_pixel.SetByteValue((char*)&samples_val, sizeof(uint16_t));
    ds.Insert(samples_per_pixel);

    gdcm::DataElement photometric_interpretation(gdcm::Tag(0x0028, 0x0004));
    photometric_interpretation.SetVR(gdcm::VR::CS);
    photometric_interpretation.SetByteValue("MONOCHROME2", strlen("MONOCHROME2"));
    ds.Insert(photometric_interpretation);

    gdcm::DataElement bits_allocated(gdcm::Tag(0x0028, 0x0100));
    bits_allocated.SetVR(gdcm::VR::US);
    uint16_t bits_alloc_val = 16;
    bits_allocated.SetByteValue((char*)&bits_alloc_val, sizeof(uint16_t));
    ds.Insert(bits_allocated);

    gdcm::DataElement bits_stored(gdcm::Tag(0x0028, 0x0101));
    bits_stored.SetVR(gdcm::VR::US);
    uint16_t bits_stored_val = 16;
    bits_stored.SetByteValue((char*)&bits_stored_val, sizeof(uint16_t));
    ds.Insert(bits_stored);

    gdcm::DataElement high_bit(gdcm::Tag(0x0028, 0x0102));
    high_bit.SetVR(gdcm::VR::US);
    uint16_t high_bit_val = 15;
    high_bit.SetByteValue((char*)&high_bit_val, sizeof(uint16_t));
    ds.Insert(high_bit);
    
    gdcm::DataElement pixel_representation(gdcm::Tag(0x0028, 0x0103));
    pixel_representation.SetVR(gdcm::VR::US);
    uint16_t pixel_rep_val = 0;
    pixel_representation.SetByteValue((char*)&pixel_rep_val, sizeof(uint16_t));
    ds.Insert(pixel_representation);

    gdcm::DataElement rescale_intercept(gdcm::Tag(0x0028, 0x1052));
    rescale_intercept.SetVR(gdcm::VR::DS);
    rescale_intercept.SetByteValue("0.0", strlen("0.0"));
    ds.Insert(rescale_intercept);

    gdcm::DataElement rescale_slope(gdcm::Tag(0x0028, 0x1053));
    rescale_slope.SetVR(gdcm::VR::DS);
    rescale_slope.SetByteValue(dose_grid_str.c_str(), dose_grid_str.length());
    ds.Insert(rescale_slope);

    gdcm::DataElement number_of_frames(gdcm::Tag(0x0028, 0x0008));
    number_of_frames.SetVR(gdcm::VR::IS);
    number_of_frames.SetByteValue(frames_str.c_str(), frames_str.length());
    ds.Insert(number_of_frames);

    gdcm::DataElement pixel_data_elem(gdcm::Tag(0x7FE0, 0x0010));
    pixel_data_elem.SetVR(gdcm::VR::OW);
    pixel_data_elem.SetByteValue((const char*)pixel_data.data(), pixel_data.size() * sizeof(uint16_t));
    ds.Insert(pixel_data_elem);

    gdcm::Writer writer;
    writer.SetFile(*file);
    std::string output_filename = filepath + "/" + filename + ".dcm";
    writer.SetFileName(output_filename.c_str());

    if (!writer.Write()) {
        std::cerr << "Failed to write DICOM file: " << output_filename << std::endl;
        return;
    }

    std::cout << "Successfully wrote DICOM file: " << output_filename << std::endl;
}

} 
}
#endif
