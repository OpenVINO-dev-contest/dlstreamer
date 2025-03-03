#!/bin/bash
# ==============================================================================
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

VIDEO_FILE=${1}
MODEL=${2:-${MODELS_PATH}/intel/face-detection-adas-0001/FP32/face-detection-adas-0001.xml}
OUTPUT=${3:-STDOUT} #Valid STDOUT FILE

EXE_NAME=ffmpeg_openvino_decode_resize_inference
BUILD_DIR=$HOME/intel/dl_streamer/samples/${EXE_NAME}/build
BASE_DIR=$(realpath $(dirname "$0"))

rm -rf ${BUILD_DIR}
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

if [ -f /etc/lsb-release ]; then
    cmake ${BASE_DIR}
else
    cmake3 ${BASE_DIR}
fi

make -j $(nproc)

if [ "${OUTPUT}" = "FILE" ]; then
    ${BUILD_DIR}/${EXE_NAME} -i ${VIDEO_FILE} -m ${MODEL} > output.txt
else
    ${BUILD_DIR}/${EXE_NAME} -i ${VIDEO_FILE} -m ${MODEL}
fi
