/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "yolo_v8.h"

#include "inference_backend/image_inference.h"
#include "inference_backend/logger.h"
#include "safe_arithmetic.hpp"

#include <gst/gst.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace post_processing;

double getIOUThreshold(GstStructure *s) {
    double iou_threshold = 0.5;
    if (gst_structure_has_field(s, "iou_threshold")) {
        gst_structure_get_double(s, "iou_threshold", &iou_threshold);
    }

    return iou_threshold;
}

void YOLOv8Converter::parseOutputBlob(const float *data, const std::vector<size_t> &dims,
                                               std::vector<DetectedObject> &objects) const {

    size_t dims_size = dims.size();
    size_t input_width = getModelInputImageInfo().width;
    size_t input_height = getModelInputImageInfo().height;

    if (dims_size < BlobToROIConverter::min_dims_size)
        throw std::invalid_argument("Output blob dimensions size " + std::to_string(dims_size) +
                                    " is not supported (less than " +
                                    std::to_string(BlobToROIConverter::min_dims_size) + ").");

    size_t object_size = dims[dims_size - 2];
    if (object_size != YOLOv8Converter::model_object_size)
        throw std::invalid_argument("Object size dimension of output blob is set to " + std::to_string(object_size) +
                                    ", but only " + std::to_string(YOLOv8Converter::model_object_size) +
                                    " supported.");

    size_t max_proposal_count = dims[dims_size - 1];

    std::vector<int> class_ids;
    std::vector<float> confidences;
    for (size_t i = 0; i < max_proposal_count; ++i) {
        // float *classes_scores = data+4;
        // cv::Mat scores(1, classes.size(), CV_32FC1, classes_scores);
        // cv::Point class_id;
        // double maxClassScore;

        // cv::minMaxLoc(scores, 0, &maxClassScore, 0, &class_id);
        double maxClassScore = 0;
        int idx = 0;

        for (size_t t = 4; t < max_proposal_count; ++t) {
            double tp = data[t];
            if (tp > maxClassScore) {
                maxClassScore = tp;
                idx = t;
            }
        }

        if (maxClassScore > confidence_threshold) {
            float x = data[0];
            float y = data[1];
            float w = data[2];
            float h = data[3];
            objects.push_back(DetectedObject(x, y, w, h, maxClassScore, idx-4,
                                                    getLabelByLabelId(idx-4), 1.0f / input_width,
                            1.0f / input_height, true));
        }
        data += object_size;
    }
}

TensorsTable YOLOv8Converter::convert(const OutputBlobs &output_blobs) const {
    ITT_TASK(__FUNCTION__);
    try {
        const auto &model_input_image_info = getModelInputImageInfo();
        size_t batch_size = model_input_image_info.batch_size;

        DetectedObjectsTable objects_table(batch_size);

        for (size_t batch_number = 0; batch_number < batch_size; ++batch_number) {
            auto &objects = objects_table[batch_number];

            for (const auto &blob_iter : output_blobs) {
                const InferenceBackend::OutputBlob::Ptr &blob = blob_iter.second;
                if (not blob)
                    throw std::invalid_argument("Output blob is nullptr.");

                size_t unbatched_size = blob->GetSize() / batch_size;
                parseOutputBlob(reinterpret_cast<const float *>(blob->GetData()) + unbatched_size * batch_number,
                                blob->GetDims(), objects);
            }
        }

        return storeObjects(objects_table);
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to do YoloV3 post-processing."));
    }
    return TensorsTable{};
}
