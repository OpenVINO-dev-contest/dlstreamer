# Run demo with pre-built docker image

## 1. Set-up environments ##
   
   ```
   $ xhost local:root
   ```
   
   ```
   $ setfacl -m user:1000:r ~/.Xauthority
   ```
   
   You can also switch it with your second GPU by ```renderD128``` --> ```renderD129```
   ```
   $ DEVICE=${DEVICE:-/dev/dri/renderD128}
   ```
   
   ```
   $ DEVICE_GRP=$(ls -g $DEVICE | awk '{print $3}' | \
   xargs getent group | awk -F: '{print $3}')
   ```
## 2. Pull docker images(optional)

   ```
   $ docker pull snake7gun/yolov8-dpcpp:latest
   ```

   For dGPU:
   
   ```
   $ docker pull snake7gun/yolov8-dgpu:latest
   ```
    
## 3. Run container
   
   ```
   $ docker run -it --rm --net=host -e no_proxy=$no_proxy -e https_proxy=$https_proxy -e socks_proxy=$socks_proxy -e http_proxy=$http_proxy \
   -v ~/.Xauthority:/home/dlstreamer/.Xauthority -v /tmp/.X11-unix -e DISPLAY=$DISPLAY \
   --device $DEVICE --group-add $DEVICE_GRP \
   snake7gun/yolov8-dpcpp /bin/bash
   ```
   
   For dGPU, please run container by:
   
   ```
   $ docker run -it --rm --net=host -e no_proxy=$no_proxy -e https_proxy=$https_proxy -e socks_proxy=$socks_proxy -e http_proxy=$http_proxy \
   -v ~/.Xauthority:/home/dlstreamer/.Xauthority -v /tmp/.X11-unix -e DISPLAY=$DISPLAY \
   --device $DEVICE --group-add $DEVICE_GRP \
   snake7gun/yolov8-dgpu /bin/bash
   ```

## 4. Run single channel sample in container

   ```
   $ source /home/dlstreamer/dlstreamer_gst/scripts/setup_env.sh
   ```
   
   
   ```
   $ gst-launch-1.0 filesrc location=./TownCentreXVID.mp4 ! decodebin ! video/x-raw\(memory:VASurface\) ! gvadetect model=./models/yolov8n_int8_ppp.xml model_proc=./dlstreamer_gst/samples/gstreamer/model_proc/public/yolo-v8.json pre-process-backend=vaapi-surface-sharing device=GPU ! queue ! meta_overlay device=GPU preprocess-queue-size=25 process-queue-size=25 postprocess-queue-size=25 ! videoconvert ! fpsdisplaysink video-sink=ximagesink sync=false
   ```

   To test the performance:
   
   ```
   $ gst-launch-1.0 filesrc location=./TownCentreXVID.mp4 ! decodebin ! video/x-raw\(memory:VASurface\) ! gvadetect model=./models/yolov8n_int8_ppp.xml model_proc=./dlstreamer_gst/samples/gstreamer/model_proc/public/yolo-v8.json pre-process-backend=vaapi-surface-sharing device=GPU ! queue ! meta_overlay device=GPU preprocess-queue-size=25 process-queue-size=25 postprocess-queue-size=25 ! gvafpscounter ! fakesink sync=false 
   ```

   To push the stream to specific address, e.g ```rtp://192.168.3.9:5004```, you can run:


   ```
   $ gst-launch-1.0 filesrc location=./TownCentreXVID.mp4 ! decodebin ! video/x-raw\(memory:VASurface\) ! gvadetect model=./models/yolov8n_int8_ppp.xml model_proc=./dlstreamer_gst/samples/gstreamer/model_proc/public/yolo-v8.json pre-process-backend=vaapi-surface-sharing device=GPU ! queue ! meta_overlay device=GPU preprocess-queue-size=25 process-queue-size=25 postprocess-queue-size=25 ! videoconvert ! vaapih264enc ! h264parse ! mpegtsmux ! rtpmp2tpay ! udpsink host=192.168.3.9 port=5004
   ```

   To further improve the performance on iGPU, you can switch the ```meta_overlay device=GPU preprocess-queue-size=25 process-queue-size=25 postprocess-queue-size=25``` with ```gvawatermark```

## 5. Run multiple channel demo in container

   ```
   $ cd dlstreamer_gst/demo/
   ```
   

   ```
   $ ./pipeline.sh ~/TownCentreXVID.mp4 ~/TownCentreXVID.mp4 ~/TownCentreXVID.mp4 ~/TownCentreXVID.mp4 ~/TownCentreXVID.mp4 ~/TownCentreXVID.mp4 ~/TownCentreXVID.mp4 ~/TownCentreXVID.mp4 ~/TownCentreXVID.mp4

   ```

   You can change this [parameter](https://github.com/OpenVINO-dev-contest/dlstreamer/blob/yolov8/demo/pipeline.sh#L4) in your docker container, to optimize the performance according to worklord and HW spec
