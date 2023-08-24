# Copyright (C) 2023 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

import json
import gi
gi.require_version('Gst', '1.0')
from gi.repository import Gst, GObject, GLib


with open("config.json") as file:
    config = json.load(file)

num_cam = config["number_cameras"]
cams = config["cam_url"]
display = config["display"]
target_device = config["inference_device"]
tracker = "inference-interval=10 ! queue ! gvatrack tracking-type=short-term-imageless"
decode_element=""
params=""

if (target_device == "GPU"):
    target_device = "GPU"
    batch_size = " batch-size=" + str(num_cam)
    nireq = " nireq=2"
    decode_element = " decodebin ! video/x-raw(memory:VASurface) "
    DL_MODEL='"/home/dlstreamer/models/yolov8n_int8_ppp.xml" model-proc="/home/dlstreamer/dlstreamer_gst/samples/gstreamer/model_proc/public/yolo-v8.json" pre-process-backend=vaapi-surface-sharing'
    params=batch_size + nireq + " model-instance-id=1 ! meta_overlay device=GPU preprocess-queue-size=25 process-queue-size=25 postprocess-queue-size=25 "
    # params=" nireq=9 inference-interval=5 ! queue ! gvatrack tracking-type=short-term-imageless ! queue ! gvawatermark "
else:
    target_device="CPU"
    decode_element = "decodebin"
    DL_MODEL='"/home/dlstreamer/models/yolov8n_int8_ppp.xml" model-proc="/home/dlstreamer/dlstreamer_gst/samples/gstreamer/model_proc/public/yolo-v8.json"'
    params=" ! gvawatermark"

if (type(num_cam) == str):
    print(f"    \nnumber_cameras should only be integer value\n")
elif(num_cam > 9):
    print(f'Demo currently supportls 9 streams only')
else:
    composite_sinks = {
        1:"sink_1::alpha=1", 
        2:"sink_1::xpos=0 sink_2::xpos=640", 
        3:"sink_1::xpos=0 sink_2::xpos=640 sink_3::ypos=360", 
        4:"sink_1::xpos=0 sink_2::xpos=640 sink_3::ypos=360 sink_4::xpos=640 sink_4::ypos=360",
        5:"sink_1::xpos=0 sink_2::xpos=640 sink_3::ypos=360 sink_4::xpos=640 sink_4::ypos=360 sink_5::xpos=1280",
        6:"sink_1::xpos=0 sink_2::xpos=640 sink_3::ypos=360 sink_4::xpos=640 sink_4::ypos=360 sink_5::xpos=1280 sink_6::xpos=1280 sink_6::ypos=360",
        7:"sink_1::xpos=0 sink_2::xpos=640 sink_3::ypos=360 sink_4::xpos=640 sink_4::ypos=360 sink_5::xpos=1280 sink_6::xpos=1280 sink_6::ypos=360 sink_7::ypos=720",
        8:"sink_1::xpos=0 sink_2::xpos=640 sink_3::ypos=360 sink_4::xpos=640 sink_4::ypos=360 sink_5::xpos=1280 sink_6::xpos=1280 sink_6::ypos=360 sink_7::ypos=720 sink_8::xpos=640 sink_8::ypos=720",
        9:"sink_1::xpos=0 sink_2::xpos=640 sink_3::ypos=360 sink_4::xpos=640 sink_4::ypos=360 sink_5::xpos=1280 sink_6::xpos=1280 sink_6::ypos=360 sink_7::ypos=720 sink_8::xpos=640 sink_8::ypos=720 sink_9::xpos=1280 sink_9::ypos=720"
        }
    scales = {
        1:"video/x-raw,width=640,height=360", 
        2:"video/x-raw,width=640,height=360", 
        3:"video/x-raw,width=640,height=360", 
        4:"video/x-raw,width=640,height=360",
        5:"video/x-raw,width=640,height=360",
        6:"video/x-raw,width=640,height=360",
        7:"video/x-raw,width=640,height=360",
        8:"video/x-raw,width=640,height=360",
        9:"video/x-raw,width=640,height=360"
        }

    csink = composite_sinks[num_cam]
    dsink = ""

    pipeline=""
    
    if (display == "yes"):
        if(target_device=="GPU"):
            dsink = "videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false"
        else:
            dsink = "videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false"
    else:
        dsink = "gvafpscounter ! fakesink async=false"

    def input_format(input):
        input = f'"{input}"'
        if ("/dev/video" in input):
            stream = "v4l2src device="+input
        elif ("://" in input):
            stream = "urisourcebin buffer-size=4096 uri=" +input
        else:
            stream = "filesrc location="+input
            
        return stream

    for i in range(num_cam):
        formatted_input = input_format(list(cams.values())[i])
        pipeline = pipeline + formatted_input + " ! " + decode_element + " ! gvadetect model="+DL_MODEL+" device=" +target_device+ ""+params+ "" +" ! " + scales[num_cam] + " ! mix.sink_"+str(i+1) + " "

    final_pipeline = "compositor name=mix background=1 " + csink + " ! " + dsink + " " + pipeline
    print(final_pipeline)

    pipeline = None
    bus = None
    message = None
    
    Gst.init(None)
    pipeline = Gst.parse_launch(final_pipeline)

    # start playing
    pipeline.set_state(Gst.State.PLAYING)

    # wait until EOS or error
    bus = pipeline.get_bus()

    msg = bus.timed_pop_filtered(Gst.CLOCK_TIME_NONE, Gst.MessageType.ERROR | Gst.MessageType.EOS)

    pipeline.set_state(Gst.State.NULL)
