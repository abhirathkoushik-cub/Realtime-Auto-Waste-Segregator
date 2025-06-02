import tensorflow as tf

# Load the .h5 model
# model = tf.keras.models.load_model('retrained_model.h5')
model = tf.keras.models.load_model('retrained_model_new_kaggle_dataset.h5')

# Convert to TFLite
converter = tf.lite.TFLiteConverter.from_keras_model(model)
tflite_model = converter.convert()

# Save the TFLite model
with open('model_new_kaggle_dataset.tflite', 'wb') as f:
    f.write(tflite_model)

print("? Converted model saved as model_new_kaggle_dataset.tflite")
