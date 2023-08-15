# Run demo with pre-built docker image

1. pull image

    ```
    $ docker pull snake7gun/yolo-v8
    ```

2. run container
    
    for iGPU:
    ```
    $ DEVICE=${DEVICE:-/dev/dri/renderD128}
    ```

    for dGPU:
    ```
    $ DEVICE=${DEVICE:-/dev/dri/renderD129}
    ```

    ```
    $ DEVICE_GRP=$(ls -g $DEVICE | awk '{print $3}' | \
    xargs getent group | awk -F: '{print $3}')
    ```

    ```
    $ docker run -it --rm --net=host -e no_proxy=$no_proxy -e https_proxy=$https_proxy -e socks_proxy=$socks_proxy -e http_proxy=$http_proxy \
    -v ~/.Xauthority:/home/dlstreamer/.Xauthority -v /tmp/.X11-unix -e DISPLAY=$DISPLAY \
    --device $DEVICE --group-add $DEVICE_GRP \
    snake7gun/yolo-v8 /bin/bash
    ```

3. run single channel sample in container

    ```
    $ source /home/dlstreamer/dlstreamer_gst/scripts/setup_env.sh
    ```

    ```
    $ gst-launch-1.0 filesrc location=./pexels_1721294.mp4 ! decodebin ! video/x-raw\(memory:VASurface\) ! gvadetect model=./models/yolov8n_int8_ppp.xml model_proc=./dlstreamer_gst/samples/gstreamer/model_proc/public/yolo-v8.json pre-process-backend=vaapi-surface-sharing device=GPU ! queue ! gvawatermark ! videoconvert ! fpsdisplaysink video-sink=ximagesink sync=false
    ```

4. run multiple channel demo in container

    ```
    $ cd dlstreamer_gst/demo/
    ```

    ```
    $ python3 pipeline.py
    ```
