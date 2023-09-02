#!/bin/bash
VIDEO=(${*})
NUM_PANES=$#
STREAM=2

shift 2

source $HOME/dlstreamer_gst/scripts/setup_env.sh
source /opt/intel/oneapi/compiler/latest/env/vars.sh

MODEL=$HOME/models/yolov8n_int8_ppp.xml
MODEL_PROC=$HOME/dlstreamer_gst/samples/gstreamer/model_proc/public/yolo-v8.json

RES_WIDTH=1920
RES_HEIGHT=1080

case ${NUM_PANES} in
    1)
    PANE_WIDTH=$((${RES_WIDTH}/1))
    PANE_HEIGHT=$((${RES_HEIGHT}/1))
    function compositor(){
        echo " compositor name=comp0 \
        \
        sink_0::xpos=0                    sink_0::ypos=0                 sink_0::alpha=1 "
    }
    ;;

    2|3|4)
    PANE_WIDTH=$((${RES_WIDTH}/2))
    PANE_HEIGHT=$((${RES_HEIGHT}/2))
    function compositor(){
        echo " compositor name=comp0 \
        \
        sink_0::xpos=0                    sink_0::ypos=0                 sink_0::alpha=1 \
        sink_1::xpos=$((${PANE_WIDTH}*1)) sink_1::ypos=0                 sink_1::alpha=1 \
        sink_2::xpos=0                    sink_2::ypos=$((${PANE_HEIGHT}*1)) sink_2::alpha=1 \
        sink_3::xpos=$((${PANE_WIDTH}*1)) sink_3::ypos=$((${PANE_HEIGHT}*1)) sink_3::alpha=1 "
    }
    ;;

    5|6|7|8|9)
    PANE_WIDTH=$((${RES_WIDTH}/3))
    PANE_HEIGHT=$((${RES_HEIGHT}/3))
    function compositor(){
        echo " compositor name=comp0 \
        \
        sink_0::xpos=0                    sink_0::ypos=0                 sink_0::alpha=1 \
        sink_1::xpos=$((${PANE_WIDTH}*1)) sink_1::ypos=0                 sink_1::alpha=1 \
        sink_2::xpos=$((${PANE_WIDTH}*2)) sink_2::ypos=0                 sink_2::alpha=1 \
        \
        sink_3::xpos=0                    sink_3::ypos=$((${PANE_HEIGHT}*1)) sink_3::alpha=1 \
        sink_4::xpos=$((${PANE_WIDTH}*1)) sink_4::ypos=$((${PANE_HEIGHT}*1)) sink_4::alpha=1 \
        sink_5::xpos=$((${PANE_WIDTH}*2)) sink_5::ypos=$((${PANE_HEIGHT}*1)) sink_5::alpha=1 \
        \
        sink_6::xpos=0                    sink_6::ypos=$((${PANE_HEIGHT}*2)) sink_6::alpha=1 \
        sink_7::xpos=$((${PANE_WIDTH}*1)) sink_7::ypos=$((${PANE_HEIGHT}*2)) sink_7::alpha=1 \
        sink_8::xpos=$((${PANE_WIDTH}*2)) sink_8::ypos=$((${PANE_HEIGHT}*2)) sink_8::alpha=1 "
    }
    ;;

    10|11|12|13|14|15|16)
    PANE_WIDTH=$((${RES_WIDTH}/4))
    PANE_HEIGHT=$((${RES_HEIGHT}/4))
    function compositor(){
        echo " compositor name=comp0 \
        \
        sink_0::xpos=0                    sink_0::ypos=0                 sink_0::alpha=1 \
        sink_1::xpos=$((${PANE_WIDTH}*1)) sink_1::ypos=0                 sink_1::alpha=1 \
        sink_2::xpos=$((${PANE_WIDTH}*2)) sink_2::ypos=0                 sink_2::alpha=1 \
        sink_3::xpos=$((${PANE_WIDTH}*3)) sink_3::ypos=0                 sink_3::alpha=1 \
        \
        sink_4::xpos=0                    sink_4::ypos=$((${PANE_HEIGHT}*1)) sink_4::alpha=1 \
        sink_5::xpos=$((${PANE_WIDTH}*1)) sink_5::ypos=$((${PANE_HEIGHT}*1)) sink_5::alpha=1 \
        sink_6::xpos=$((${PANE_WIDTH}*2)) sink_6::ypos=$((${PANE_HEIGHT}*1)) sink_6::alpha=1 \
        sink_7::xpos=$((${PANE_WIDTH}*3)) sink_7::ypos=$((${PANE_HEIGHT}*1)) sink_7::alpha=1 \
        \
        sink_8::xpos=0                    sink_8::ypos=$((${PANE_HEIGHT}*2)) sink_8::alpha=1 \
        sink_9::xpos=$((${PANE_WIDTH}*1)) sink_9::ypos=$((${PANE_HEIGHT}*2)) sink_9::alpha=1 \
        sink_10::xpos=$((${PANE_WIDTH}*2)) sink_10::ypos=$((${PANE_HEIGHT}*2)) sink_10::alpha=1 \
        sink_11::xpos=$((${PANE_WIDTH}*3)) sink_11::ypos=$((${PANE_HEIGHT}*2)) sink_11::alpha=1 \
                \
        sink_12::xpos=0                    sink_12::ypos=$((${PANE_HEIGHT}*3)) sink_12::alpha=1 \
        sink_13::xpos=$((${PANE_WIDTH}*1)) sink_13::ypos=$((${PANE_HEIGHT}*3)) sink_13::alpha=1 \
        sink_14::xpos=$((${PANE_WIDTH}*2)) sink_14::ypos=$((${PANE_HEIGHT}*3)) sink_14::alpha=1 \
        sink_15::xpos=$((${PANE_WIDTH}*3)) sink_15::ypos=$((${PANE_HEIGHT}*3)) sink_15::alpha=1 "
    }
    ;;

    *)
    echo "NUM_PANES
 must be between 1 to 16 inclusive!"
    exit -1
    ;;
esac

function sub_pipeline() {
    sub_pipe+="decodebin ! video/x-raw(memory:VASurface) ! "
    sub_pipe+="gvadetect model=${MODEL} model_proc=${MODEL_PROC} "
    # sub_pipe+="ie-config=CACHE_DIR=./cl_cache "
    sub_pipe+="nireq=$((${STREAM}*2)) gpu-throughput-streams=${STREAM} batch-size=${NUM_PANES} model-instance-id=1 "
    sub_pipe+="pre-process-backend=vaapi-surface-sharing device=GPU ! "
    sub_pipe+="queue ! "
    sub_pipe+="meta_overlay device=GPU preprocess-queue-size=25 process-queue-size=25 postprocess-queue-size=25 ! video/x-raw,width=${PANE_WIDTH},height=${PANE_HEIGHT} "
    echo ${sub_pipe}
}

pipeline="$(compositor) ! "
pipeline+="gvafpscounter ! "
# pipeline+="gvafpscounter ! fakesink sync=false "
# pipeline+="videoconvert ! fpsdisplaysink video-sink=ximagesink sync=false "
pipeline+="videoconvert ! vaapih264enc ! h264parse ! mpegtsmux ! rtpmp2tpay ! udpsink host=10.254.213.228 port=5004 "

for i in `seq 0 $((${NUM_PANES}-1))`; do
    if [[ $INPUT == "/dev/video"* ]]; then
        pipeline+="v4l2src device=${VIDEO[${i}]} ! "
    elif [[ $INPUT == *"://"* ]]; then
        pipeline+="urisourcebin buffer-size=4096 uri=${VIDEO[${i}]} ! "
    else
        pipeline+="filesrc location=${VIDEO[${i}]} ! "
    fi
#    pipeline+="rtspsrc location=rtsp://10.3.233.52:8554/CH001.sdp onvif-mode=true ! "
#    pipeline+="rtponvifparse ! application/x-rtp,media=video ! "
    pipeline+="$(sub_pipeline) ! "
    pipeline+="queue ! "
    pipeline+="comp0.sink_${i} "
done

echo "Running pipeline: ${pipeline}"
echo ${pipeline} > running_pipeline.txt

export GST_DEBUG_DUMP_DOT_DIR=/tmp/
gst-launch-1.0 ${pipeline}

# Generate pipeline diagram
# if [[ -x /usr/bin/dot ]]; then
#     latest_dot=`ls -1t /tmp/*.PLAYING_PAUSED.dot | head -1`
#     dot -Tpng ${latest_dot} > running_pipeline.png
# fi
