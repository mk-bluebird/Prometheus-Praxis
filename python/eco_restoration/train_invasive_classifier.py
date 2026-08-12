# File: python/eco_restoration/train_invasive_classifier.py

import argparse
import pathlib
import tensorflow as tf

parser = argparse.ArgumentParser()
parser.add_argument("dataset")
parser.add_argument("model_output")
args = parser.parse_args()

dataset_path = pathlib.Path(args.dataset)
train = tf.keras.utils.image_dataset_from_directory(
    dataset_path, validation_split=0.2, subset="training", seed=2026,
    image_size=(128, 128), batch_size=32
)
validation = tf.keras.utils.image_dataset_from_directory(
    dataset_path, validation_split=0.2, subset="validation", seed=2026,
    image_size=(128, 128), batch_size=32
)

model = tf.keras.Sequential([
    tf.keras.layers.Input((128, 128, 3)),
    tf.keras.layers.Rescaling(1.0 / 255.0),
    tf.keras.applications.MobileNetV2(include_top=False, weights=None, alpha=0.35),
    tf.keras.layers.GlobalAveragePooling2D(),
    tf.keras.layers.Dropout(0.20),
    tf.keras.layers.Dense(1, activation="sigmoid")
])

model.compile(
    optimizer=tf.keras.optimizers.Adam(1e-3),
    loss=tf.keras.losses.BinaryCrossentropy(),
    metrics=[tf.keras.metrics.BinaryAccuracy(), tf.keras.metrics.AUC()]
)

model.fit(
    train.prefetch(tf.data.AUTOTUNE),
    validation_data=validation.prefetch(tf.data.AUTOTUNE),
    epochs=20
)

converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
pathlib.Path(args.model_output).write_bytes(converter.convert())
