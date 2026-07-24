from pathlib import Path

path = Path("main/moonraker_live_transport.c")
text = path.read_text()

old = """        "&display_status"
        "&fan"
        "&extruder"
"""

new = """        "&display_status"
        "&gcode_move"
        "&fan"
        "&extruder"
"""

count = text.count(old)

if count != 1:
    raise RuntimeError(
        f"expected one Moonraker object-query anchor, found {count}")

text = text.replace(old, new, 1)
path.write_text(text)

print("Added gcode_move to the Moonraker live-object query.")
print("Shared speed_factor and extrude_factor will now refresh.")
