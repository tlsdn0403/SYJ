import math
import unreal

SEQUENCE_PATH = "/Game/Maps/map_level2/LS_Ending"
OUTPUT_PATH = r"D:\SYJ\FPSProject\Tools\Codex\rotate_ls_ending_yaw90_result.txt"
TARGET_BINDINGS = {"SK_Insurgent_P7", "SK_Insurgent_P8"}
PIVOT_BINDING = "SK_Insurgent_P7"
YAW_DEGREES = 90.0

LOCATION_X_CHANNEL = 0
LOCATION_Y_CHANNEL = 1
LOCATION_Z_CHANNEL = 2
YAW_CHANNEL = 5


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


def find_binding(sequence, target_name):
    for binding in sequence.get_bindings():
        if str(binding.get_name()) == target_name:
            return binding
    return None


def rotate_xy(x_value, y_value, pivot_x, pivot_y, radians):
    dx = x_value - pivot_x
    dy = y_value - pivot_y
    cos_yaw = math.cos(radians)
    sin_yaw = math.sin(radians)
    return (
        pivot_x + (dx * cos_yaw - dy * sin_yaw),
        pivot_y + (dx * sin_yaw + dy * cos_yaw),
    )


sequence = unreal.EditorAssetLibrary.load_asset(SEQUENCE_PATH)
if sequence is None:
    raise RuntimeError(f"Failed to load {SEQUENCE_PATH}")

pivot_binding = find_binding(sequence, PIVOT_BINDING)
if pivot_binding is None:
    raise RuntimeError(f"Failed to find pivot binding {PIVOT_BINDING}")

pivot_section = get_transform_section(pivot_binding)
if pivot_section is None:
    raise RuntimeError(f"Failed to find transform section for {PIVOT_BINDING}")

pivot_channels = pivot_section.get_all_channels()
pivot_x_keys = pivot_channels[LOCATION_X_CHANNEL].get_keys()
pivot_y_keys = pivot_channels[LOCATION_Y_CHANNEL].get_keys()
pivot_z_keys = pivot_channels[LOCATION_Z_CHANNEL].get_keys()
if not pivot_x_keys or not pivot_y_keys or not pivot_z_keys:
    raise RuntimeError(f"{PIVOT_BINDING} does not have enough location keys")

pivot_x = float(pivot_x_keys[0].get_value())
pivot_y = float(pivot_y_keys[0].get_value())
pivot_z = float(pivot_z_keys[0].get_value())
radians = math.radians(YAW_DEGREES)

emit(f"SEQUENCE={sequence}")
emit(f"PIVOT_BINDING={PIVOT_BINDING}")
emit(f"PIVOT=({pivot_x}, {pivot_y}, {pivot_z})")
emit(f"YAW_DEGREES={YAW_DEGREES}")

sequence.modify()

modified_location_keys = 0
modified_yaw_keys = 0
modified_bindings = set()

for binding in sequence.get_bindings():
    binding_name = str(binding.get_name())
    if binding_name not in TARGET_BINDINGS:
        continue

    section = get_transform_section(binding)
    if section is None:
        raise RuntimeError(f"Failed to find transform section for {binding_name}")

    section.modify()
    channels = section.get_all_channels()
    if len(channels) <= YAW_CHANNEL:
        raise RuntimeError(f"{binding_name} transform section has too few channels")

    x_channel = channels[LOCATION_X_CHANNEL]
    y_channel = channels[LOCATION_Y_CHANNEL]
    yaw_channel = channels[YAW_CHANNEL]
    x_keys = x_channel.get_keys()
    y_keys = y_channel.get_keys()
    yaw_keys = yaw_channel.get_keys()

    if len(x_keys) != len(y_keys):
        raise RuntimeError(
            f"{binding_name} Location.X/Y key counts differ: {len(x_keys)} != {len(y_keys)}"
        )

    location_changes = []
    for x_key, y_key in zip(x_keys, y_keys):
        old_x = float(x_key.get_value())
        old_y = float(y_key.get_value())
        new_x, new_y = rotate_xy(old_x, old_y, pivot_x, pivot_y, radians)
        x_key.set_value(new_x)
        y_key.set_value(new_y)
        location_changes.append((old_x, old_y, new_x, new_y))
        modified_location_keys += 2

    yaw_changes = []
    for yaw_key in yaw_keys:
        old_yaw = float(yaw_key.get_value())
        new_yaw = old_yaw + YAW_DEGREES
        yaw_key.set_value(new_yaw)
        yaw_changes.append((old_yaw, new_yaw))
        modified_yaw_keys += 1

    modified_bindings.add(binding_name)

    if location_changes:
        first_old_x, first_old_y, first_new_x, first_new_y = location_changes[0]
        last_old_x, last_old_y, last_new_x, last_new_y = location_changes[-1]
        emit(
            f"{binding_name} Location.XY: keys={len(location_changes)} "
            f"first=({first_old_x}, {first_old_y})->({first_new_x}, {first_new_y}) "
            f"last=({last_old_x}, {last_old_y})->({last_new_x}, {last_new_y})"
        )

    if yaw_changes:
        first_old_yaw, first_new_yaw = yaw_changes[0]
        last_old_yaw, last_new_yaw = yaw_changes[-1]
        emit(
            f"{binding_name} Rotation.Z: keys={len(yaw_changes)} "
            f"first={first_old_yaw}->{first_new_yaw} "
            f"last={last_old_yaw}->{last_new_yaw}"
        )

if modified_bindings != TARGET_BINDINGS:
    raise RuntimeError(
        f"Expected bindings {sorted(TARGET_BINDINGS)}, modified {sorted(modified_bindings)}"
    )

saved = unreal.EditorAssetLibrary.save_loaded_asset(sequence, only_if_is_dirty=False)
if not saved:
    saved = unreal.EditorAssetLibrary.save_asset(SEQUENCE_PATH, only_if_is_dirty=False)

emit(f"TOTAL_MODIFIED_LOCATION_CHANNEL_KEYS={modified_location_keys}")
emit(f"TOTAL_MODIFIED_YAW_KEYS={modified_yaw_keys}")
emit(f"SAVED={saved}")

with open(OUTPUT_PATH, "w", encoding="utf-8") as output_file:
    output_file.write("\n".join(lines))

if not saved:
    raise RuntimeError("Failed to save LS_Ending after rotating keys.")
