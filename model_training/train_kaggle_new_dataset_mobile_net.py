import os
import tensorflow as tf
from tensorflow.keras.applications.mobilenet_v2 import MobileNetV2, preprocess_input
from tensorflow.keras.layers import Dense, GlobalAveragePooling2D
from tensorflow.keras.models import Model
from tensorflow.keras.optimizers import Adam
from tensorflow.data import AUTOTUNE

# ---- Configuration ----
IMAGE_SIZE = (224, 224)
BATCH_SIZE = 32
EPOCHS = 10
LEARNING_RATE = 0.0001
TRAIN_DIR = 'kaggle_new_dataset'  # with "biodegradable" and "nonbio" folders directly under this
VAL_DIR = None
MODEL_SAVE_PATH = 'retrained_model_new_kaggle_dataset.h5'
LABELS_SAVE_PATH = 'labels.txt'

# ---- Load datasets with 2 top-level categories ----
def load_dataset(directory):
    raw_dataset = tf.keras.preprocessing.image_dataset_from_directory(
        directory,
        image_size=IMAGE_SIZE,
        batch_size=BATCH_SIZE,
        label_mode='categorical'
    )

    class_names = raw_dataset.class_names

    dataset = raw_dataset.map(lambda x, y: (preprocess_input(x), y), num_parallel_calls=AUTOTUNE)
    dataset = dataset.apply(tf.data.experimental.ignore_errors())
    dataset = dataset.prefetch(buffer_size=AUTOTUNE)

    return dataset, class_names

# ---- Load Datasets ----
train_ds, train_class_names = load_dataset(TRAIN_DIR)
#val_ds, _ = load_dataset(VAL_DIR)

# ---- Build Model ----
base_model = MobileNetV2(weights='imagenet', include_top=False, input_shape=(*IMAGE_SIZE, 3))
base_model.trainable = False

x = base_model.output
x = GlobalAveragePooling2D()(x)
x = Dense(128, activation='relu')(x)
predictions = Dense(len(train_class_names), activation='softmax')(x)

model = Model(inputs=base_model.input, outputs=predictions)
model.compile(optimizer=Adam(learning_rate=LEARNING_RATE),
              loss='categorical_crossentropy',
              metrics=['accuracy'])

# ---- Train ----
model.fit(train_ds, epochs=EPOCHS)

# ---- Save Model and Labels ----
model.save(MODEL_SAVE_PATH)

with open(LABELS_SAVE_PATH, 'w') as f:
    for label in train_class_names:
        f.write(f"{label}\n")

print(f"? Model saved to: {MODEL_SAVE_PATH}")
print(f"? Labels saved to: {LABELS_SAVE_PATH}")
