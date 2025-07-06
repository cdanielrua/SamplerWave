target_sr = 16000  # Target sample rate
target_n_bits = 12 # Target bit depth (8, 16, or 12)

audio_file_path = 'g.wav'
y, sr = librosa.load(audio_file_path, sr=None)

print(f"Original Sample Rate: {sr} Hz")
print(f"Number of Samples: {len(y)}")

start_time = 0
end_time = 0.150

y = y[int(start_time * sr):int(end_time * sr)]

print(f"Trimmed Sample Rate: {sr} Hz")
print(f"Trimmed Number of Samples: {len(y)}")


target_type = 'uint' # 'signed' or 'uint'

y_resampled = y

if sr != target_sr:
    y_resampled = librosa.resample(y=y, orig_sr=sr, target_sr=target_sr)

if target_n_bits == 8 and target_type == 'uint':
    # Scale to [0, 255] for 8-bit unsigned
    y_resampled_scaled = (y_resampled * 127.5 + 128).astype(np.uint8)
elif target_n_bits == 16 and target_type == 'signed':
    # Scale to [-32768, 32767] for 16-bit signed
    y_resampled_scaled = (y_resampled * 32767.0).astype(np.int16)
elif target_n_bits == 16 and target_type == 'uint':
    # Scale to [0, 65535] for 16-bit unsigned
    y_resampled_scaled = (y_resampled * 32767.5 + 32768).astype(np.uint16)
elif target_n_bits == 12 and target_type == 'uint':
    # Scale to [0, 4095] for 12-bit unsigned, stored in uint16
    y_resampled_scaled = (y_resampled * 2047.5 + 2048).astype(np.uint16)
else:
    raise ValueError("Unsupported target bit depth and type combination.")


print(f"Resampled Sample Rate: {target_sr} Hz")
print(f"Resampled Number of Samples: {len(y_resampled_scaled)}")
print(f"Resampled Bit Depth: {target_n_bits} bits")
print(f"Resampled Type: {target_type}")
header_content = ""
header_content += "#ifndef AUDIO_DATA_H\n"
header_content += "#define AUDIO_DATA_H\n\n"

header_content += f"#define AUDIO_SAMPLE_RATE {target_sr}\n"
header_content += f"#define AUDIO_NUM_SAMPLES {len(y_resampled_scaled)}\n"
header_content += f"#define AUDIO_BITS_PER_SAMPLE {target_n_bits}\n"
header_content += f"#define AUDIO_TYPE \"{target_type}\"\n\n"


if target_n_bits == 8 and target_type == 'uint':
    header_content += "const uint8_t audio_data[] = {\n"
    # Format data for C array, adding line breaks for readability
    data_str = ",\n".join([", ".join(map(str, y_resampled_scaled[i:i+20])) for i in range(0, len(y_resampled_scaled), 20)])
    header_content += data_str
    header_content += "\n};\n\n"
elif target_n_bits == 16 and target_type == 'signed':
    header_content += "const int16_t audio_data[] = {\n"
    # Format data for C array, adding line breaks for readability
    data_str = ",\n".join([", ".join(map(str, y_resampled_scaled[i:i+10])) for i in range(0, len(y_resampled_scaled), 10)])
    header_content += data_str
    header_content += "\n};\n\n"
elif target_n_bits == 16 and target_type == 'uint':
    header_content += "const uint16_t audio_data[] = {\n"
    # Format data for C array, adding line breaks for readability
    data_str = ",\n".join([", ".join(map(str, y_resampled_scaled[i:i+10])) for i in range(0, len(y_resampled_scaled), 10)])
    header_content += data_str
    header_content += "\n};\n\n"
elif target_n_bits == 12 and target_type == 'uint':
    header_content += "const uint16_t audio_data[] = {\n"
    # Format data for C array, adding line breaks for readability
    data_str = ",\n".join([", ".join(map(str, y_resampled_scaled[i:i+10])) for i in range(0, len(y_resampled_scaled), 10)])
    header_content += data_str
    header_content += "\n};\n\n"


header_content += "#endif // AUDIO_DATA_H\n"

# Write the header content to a file
with open("audio_data.h", "w") as f:
    f.write(header_content)

print("audio_data.h created successfully.")
