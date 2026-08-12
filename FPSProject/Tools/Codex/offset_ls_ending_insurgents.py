import unreal

SEQUENCE_PATH = "/Game/Maps/map_level2/LS_Ending"
TARGET_BINDINGS = {"SK_Insurgent_P7", "SK_Insurgent_P8"}
LOCATION_OFFSETS = (-39363.259885, -123245.006102, 73.0)
OUTPUT_PATH = r"D:\SYJ\FPSProject\Tools\Codex\offset_ls_ending_result.txt"

sequence = unreal.EditorAssetLibrary.load_asset(SEQUENCE_PATH)
if sequence is None:
    raise RuntimeError(f"Failed to load {SEQUENCE_PATH}")

lines = [
    f"SEQUENCE={sequence}",
    f"OFFSETS={LOCATION_OFFSETS}",
]

modified_bindings = 0
modified_keys = 0

sequence.modify()

for binding in sequence.get_bindings():
    binding_name = str(binding.get_name())
    if binding_name not in TARGET_BINDINGS:
        continue

    binding_modified_keys = 0
    for track in binding.get_tracks():
        if track.get_class().get_name() != "MovieScene3DTransformTrack":
            continue

        for section in track.get_sections():
            section.modify()
            channels = section.get_all_channels()
            if len(channels) < 3:
                continue

            for channel_index, offset in enumerate(LOCATION_OFFSETS):
                channel = channels[channel_index]
                channel_name = str(channel.get_name()) if hasattr(channel, "get_name") else str(channel_index)
                key_values_before_after = []

                for key in channel.get_keys():
                    old_value = float(key.get_value())
                    new_value = old_value + offset
                    key.set_value(new_value)
                    key_values_before_after.append((old_value, new_value))
                    modified_keys += 1
                    binding_modified_keys += 1

                if key_values_before_after:
                    first_old, first_new = key_values_before_after[0]
                    last_old, last_new = key_values_before_after[-1]
                    lines.append(
                        f"{binding_name} {channel_name}: "
                        f"keys={len(key_values_before_after)} "
                        f"first={first_old}->{first_new} "
                        f"last={last_old}->{last_new}"
                    )

    if binding_modified_keys > 0:
        modified_bindings += 1
        lines.append(f"{binding_name}: modified_keys={binding_modified_keys}")

if modified_bindings != len(TARGET_BINDINGS):
    raise RuntimeError(
        f"Expected {len(TARGET_BINDINGS)} target bindings, modified {modified_bindings}."
    )

saved = unreal.EditorAssetLibrary.save_loaded_asset(sequence, only_if_is_dirty=False)
if not saved:
    saved = unreal.EditorAssetLibrary.save_asset(SEQUENCE_PATH, only_if_is_dirty=False)
lines.append(f"TOTAL_MODIFIED_KEYS={modified_keys}")
lines.append(f"SAVED={saved}")

with open(OUTPUT_PATH, "w", encoding="utf-8") as output_file:
    output_file.write("\n".join(lines))

if not saved:
    raise RuntimeError("Failed to save LS_Ending after modifying keys.")
