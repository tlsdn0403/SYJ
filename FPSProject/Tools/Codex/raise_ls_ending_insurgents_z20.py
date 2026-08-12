import unreal

SEQUENCE_PATH = "/Game/Maps/map_level2/LS_Ending"
OUTPUT_PATH = r"D:\SYJ\FPSProject\Tools\Codex\raise_ls_ending_z20_result.txt"
TARGET_BINDINGS = {"SK_Insurgent_P7", "SK_Insurgent_P8"}
Z_CHANNEL = 2
Z_OFFSET = 20.0

lines = []


def emit(message):
    lines.append(str(message))
    unreal.log(str(message))


def get_transform_section(binding):
    for track in binding.get_tracks():
        if track.get_class().get_name() != "MovieScene3DTransformTrack":
            continue
        sections = track.get_sections()
        if sections:
            return sections[0]
    return None


sequence = unreal.EditorAssetLibrary.load_asset(SEQUENCE_PATH)
if sequence is None:
    raise RuntimeError(f"Failed to load {SEQUENCE_PATH}")

emit(f"SEQUENCE={sequence}")
emit(f"Z_OFFSET={Z_OFFSET}")

sequence.modify()
modified_bindings = set()
modified_keys = 0

for binding in sequence.get_bindings():
    binding_name = str(binding.get_name())
    if binding_name not in TARGET_BINDINGS:
        continue

    section = get_transform_section(binding)
    if section is None:
        raise RuntimeError(f"Failed to find transform section for {binding_name}")

    section.modify()
    channels = section.get_all_channels()
    if len(channels) <= Z_CHANNEL:
        raise RuntimeError(f"{binding_name} transform section has too few channels")

    z_channel = channels[Z_CHANNEL]
    z_keys = z_channel.get_keys()
    changes = []

    for key in z_keys:
        old_z = float(key.get_value())
        new_z = old_z + Z_OFFSET
        key.set_value(new_z)
        changes.append((old_z, new_z))
        modified_keys += 1

    modified_bindings.add(binding_name)

    if changes:
        first_old, first_new = changes[0]
        last_old, last_new = changes[-1]
        emit(
            f"{binding_name} Location.Z: keys={len(changes)} "
            f"first={first_old}->{first_new} "
            f"last={last_old}->{last_new}"
        )

if modified_bindings != TARGET_BINDINGS:
    raise RuntimeError(
        f"Expected bindings {sorted(TARGET_BINDINGS)}, modified {sorted(modified_bindings)}"
    )

saved = unreal.EditorAssetLibrary.save_loaded_asset(sequence, only_if_is_dirty=False)
if not saved:
    saved = unreal.EditorAssetLibrary.save_asset(SEQUENCE_PATH, only_if_is_dirty=False)

emit(f"TOTAL_MODIFIED_Z_KEYS={modified_keys}")
emit(f"SAVED={saved}")

with open(OUTPUT_PATH, "w", encoding="utf-8") as output_file:
    output_file.write("\n".join(lines))

if not saved:
    raise RuntimeError("Failed to save LS_Ending after raising Z keys.")
