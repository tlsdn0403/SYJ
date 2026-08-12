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
        lines.append(f"SECTION_CLASS={section.get_class().get_name()}")
        for name in dir(section):
            if "channel" in name.lower() or "key" in name.lower() or "transform" in name.lower() or "script" in name.lower():
                lines.append(name)

with open(r"D:\SYJ\FPSProject\Tools\Codex\section_api.txt", "w", encoding="utf-8") as output_file:
    output_file.write("\n".join(lines))

