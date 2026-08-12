import unreal

SEQUENCE_PATH = "/Game/Maps/map_level2/LS_Ending"
OUTPUT_PATH = r"D:\SYJ\FPSProject\Tools\Codex\validate_ls_ending_result.txt"
TARGET_BINDINGS = {"SK_Insurgent_P7", "SK_Insurgent_P8"}

lines = []


def emit(message):
    lines.append(str(message))
    unreal.log(str(message))


sequence = unreal.EditorAssetLibrary.load_asset(SEQUENCE_PATH)
emit(f"SEQUENCE={sequence}")

for binding in sequence.get_bindings():
    binding_name = str(binding.get_name())
    if binding_name not in TARGET_BINDINGS:
        continue

    for track in binding.get_tracks():
        if track.get_class().get_name() != "MovieScene3DTransformTrack":
            continue

        for section_index, section in enumerate(track.get_sections()):
            channels = section.get_all_channels()
            values = []
            for channel_index in range(3):
                keys = channels[channel_index].get_keys()
                if not keys:
                    values.append(None)
                    continue
                values.append(keys[0].get_value())

            emit(
                f"{binding_name} section={section_index} "
                f"first_location=({values[0]}, {values[1]}, {values[2]})"
            )

with open(OUTPUT_PATH, "w", encoding="utf-8") as output_file:
    output_file.write("\n".join(lines))
