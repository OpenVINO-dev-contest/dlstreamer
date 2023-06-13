/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "blob_to_roi_converter.h"

#include "inference_backend/image_inference.h"

#include <gst/gst.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace post_processing {

class YOLOv8Converter : public BlobToROIConverter {
  protected:
    // FIXME: move roi_scale to coordinates restorer or attacher
    void parseOutputBlob(const InferenceBackend::OutputBlob::Ptr &blob, DetectedObjectsTable &objects) const;

  public:
    YOLOv8Converter(BlobToMetaConverter::Initializer initializer, double confidence_threshold)
        : BlobToROIConverter(std::move(initializer), confidence_threshold, true, 0.4) {
    }

    TensorsTable convert(const OutputBlobs &output_blobs) const override;

    static const size_t model_object_size = 84; 

    static bool isValidModelOutputs(const std::map<std::string, std::vector<size_t>> &model_outputs_info) {
        bool result = false;

        for (const auto &output : model_outputs_info) {
            const std::vector<size_t> &dims = output.second;
            if (dims.size() < BlobToROIConverter::min_dims_size)
                continue;

            if (dims[dims.size() - 2] == YOLOv8Converter::model_object_size) {
                result = true;
                break;
            }
        }

        return result;
    }

    static std::string getName() {
        return "yolo_v8";
    }

    static std::string getDepricatedName() {
        return "tensor_to_bbox_yolo_v8";
    }
};
} // namespace post_processing
