import unreal

output_lines = []


def emit(message):
    output_lines.append(str(message))
    unreal.log(str(message))


sequence = unreal.EditorAssetLibrary.load_asset("/Game/Maps/map_level2/LS_Ending")
emit(f"CODEX_SEQUENCE={sequence}")

target_names = {"SK_Insurgent_P7", "SK_Insurgent_P8"}
for binding in sequence.get_bindings():
    binding_name = str(binding.get_name())
    emit(f"CODEX_BINDING_SCAN name={binding_name}")
    if not any(target_name in binding_name for target_name in target_names):
        continue

    emit(f"CODEX_BINDING name={binding_name} id={binding.get_id()}")
    for track in binding.get_tracks():
        emit(f"  CODEX_TRACK class={track.get_class().get_name()} name={track.get_display_name()}")
        for section_index, section in enumerate(track.get_sections()):
            emit(f"    CODEX_SECTION[{section_index}] class={section.get_class().get_name()}")
            try:
                channels = section.get_all_channels()
            except Exception as exc:
                emit(f"      CODEX_CHANNELS_ERROR {exc}")
                continue

            for channel_index, channel in enumerate(channels):
                channel_name = ""
                for attr_name in ("get_name", "get_display_name"):
                    if hasattr(channel, attr_name):
                        try:
                            channel_name = str(getattr(channel, attr_name)())
                            break
                        except Exception:
                            pass

                key_count = "?"
                try:
                    key_count = len(channel.get_keys())
                except Exception:
                    pass
                emit(
                    f"      CODEX_CHANNEL[{channel_index}] class={channel.get_class().get_name()} "
                    f"name={channel_name} keys={key_count}"
                )

with open(r"D:\SYJ\FPSProject\Tools\Codex\ls_ending_inspect.txt", "w", encoding="utf-8") as output_file:
    output_file.write("\n".join(output_lines))
