from openvino.runtime import Core, serialize, PartialShape, Layout
from openvino.preprocess import PrePostProcessor, ColorFormat

core = Core()

model = core.read_model(model='/home/ethan/Intel ON/yolov8n_int8.xml')
input_layer = model.input(0)
model.reshape({input_layer.any_name: PartialShape([1, 3, 640, 640])})

ppp = PrePostProcessor(model)

ppp.input().tensor().set_color_format(ColorFormat.BGR).set_layout(Layout("NCHW")) 
ppp.input().preprocess().convert_color(ColorFormat.RGB).scale([255, 255, 255])
model = ppp.build()

serialize(model, 'yolov8_int8_ppp.xml')
