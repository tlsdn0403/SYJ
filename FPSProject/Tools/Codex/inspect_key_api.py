import unreal

sequence = unreal.EditorAssetLibrary.load_asset("/Game/Maps/map_level2/LS_Ending")
lines = []

for binding in sequence.get_bindings():
    if str(binding.get_name()) != "SK_Insurgent_P7":
        continue
    for track in binding.get_tracks():
        if track.get_class().get_name() != "MovieScene3DTransformTrack":
            continue
        section = track.get_sections()[0]
        channel = section.get_all_channels()[0]
        key = channel.get_keys()[0]
        lines.append(f"CHANNEL_CLASS={channel.get_class().get_name()}")
        lines.append(f"KEY_CLASS={key.get_class().get_name()}")
        lines.append(f"KEY_VALUE={key.get_value()}")
        for name in dir(key):
            if "value" in name.lower() or "time" in name.lower() or "interp" in name.lower():
                lines.append(name)

with open(r"D:\SYJ\FPSProject\Tools\Codex\key_api.txt", "w", encoding="utf-8") as output_file:
    output_file.write("\n".join(lines))

