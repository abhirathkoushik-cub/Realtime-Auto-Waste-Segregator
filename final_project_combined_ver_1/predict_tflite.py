import tflite_runtime.interpreter as tflite
from PIL import Image
import numpy as np
import sys
import time
import json

print("PYTHON:", sys.executable)

# ---- Config ----
IMAGE_PATH = sys.argv[1]
MODEL_PATH = "model_new_kaggle_dataset.tflite"
LABELS_PATH = "labels.txt"
IMAGE_SIZE = (224, 224)

# ---- Load labels ----
with open(LABELS_PATH, "r") as f:
    labels = [line.strip() for line in f.readlines()]

# ---- Load image and preprocess ----
img = Image.open(IMAGE_PATH).convert('RGB')
img = img.resize(IMAGE_SIZE)
img = np.array(img).astype(np.float32)
img = img / 127.5 - 1.0  # MobileNetV2 preprocess_input
img = np.expand_dims(img, axis=0)

# ---- Load TFLite model ----
interpreter = tflite.Interpreter(model_path=MODEL_PATH)
interpreter.allocate_tensors()

input_index = interpreter.get_input_details()[0]['index']
output_index = interpreter.get_output_details()[0]['index']

# ---- Run inference with timing ----
start_time = time.time()
interpreter.set_tensor(input_index, img)
interpreter.invoke()
preds = interpreter.get_tensor(output_index)[0]
end_time = time.time()

# ---- Output prediction ----
top = np.argmax(preds)
confidence = float(preds[top])  # <-- Convert to Python float
elapsed_time_ms = float((end_time - start_time) * 1000)  # <-- Also convert

# Standardize label
if labels[top] == "biodegradeable":
    label = "biodegradable"
elif labels[top] == "nonbio":
    label = "nonbiodegradable"
else:
    label = labels[top]

# Output as JSON
result = {
    "class": label,
    "confidence": confidence,
    "inference_time_ms": elapsed_time_ms
}
print(json.dumps(result)) 

# print(f"Predicted: {labels[top]} (Confidence: {confidence:.2f})")
# print(f"Inference time: {elapsed_time_ms:.2f} ms")
