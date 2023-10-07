# ==============================================================================
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

import sys
import gi
import cv2
import numpy as np
gi.require_version('Gst', '1.0')
from gi.repository import Gst, GObject

from gstgva import VideoFrame

DETECT_THRESHOLD = 0.5

Gst.init(sys.argv)


def process_frame(frame: VideoFrame, threshold: float = DETECT_THRESHOLD) -> bool:
    width = frame.video_info().width
    height = frame.video_info().height

    for tensor in frame.tensors():
        data = np.array(tensor.data())
        data=data.reshape(1,84,-1)
        outputs = np.array([cv2.transpose(data)])
        rows = outputs.shape[1]
        
        boxes = []
        scores = []
        class_ids = []

        for i in range(rows):
            classes_scores = outputs[0][i][4:]
            (minScore, maxScore, minClassLoc, (x, maxClassIndex)) = cv2.minMaxLoc(classes_scores)
            if maxScore >= 0.25:
                box = [
                    outputs[0][i][0] - (0.5 * outputs[0][i][2]), outputs[0][i][1] - (0.5 * outputs[0][i][3]),
                    outputs[0][i][2], outputs[0][i][3]]
                boxes.append(box)
                scores.append(maxScore)
                class_ids.append(maxClassIndex)

        result_boxes = cv2.dnn.NMSBoxes(boxes, scores, 0.25, 0.45, 0.5)
        for i in range(len(result_boxes)):
            index = result_boxes[i]
            box = boxes[index]
            roi = frame.add_region(
                    box[0]*width ,box[1]*height,box[2]*width,box[3]*height, str(class_ids[index]), scores[index])
    return True
