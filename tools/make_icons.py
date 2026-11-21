"""Draws the Pawse paw icons and writes them as multi-size .ico files.

Run from the repository root:

    python tools/make_icons.py

It needs Pillow (pip install pillow). The generated icons are committed, so
this only has to run again after changing the artwork.
"""

from pathlib import Path

from PIL import Image, ImageDraw

CANVAS_SIZE = 1024
ICON_SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]
RESOURCES_DIRECTORY = Path(__file__).resolve().parent.parent / "resources"

APP_ICON_COLOR = (242, 140, 40, 255)

# One tray icon per lock state. The colors match kLockStateColors in include/common.h.
TRAY_ICONS = {
    "tray-unlocked.ico": (74, 158, 255, 255),
    "tray-keyboard-locked.ico": (242, 140, 40, 255),
    "tray-mouse-locked.ico": (245, 197, 66, 255),
    "tray-both-locked.ico": (229, 72, 77, 255),
}


def draw_rotated_ellipse(canvas, center, size, angle_degrees, color):
    """Draws a filled ellipse rotated around its own center onto the canvas."""
    width, height = size
    layer = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    ImageDraw.Draw(layer).ellipse((0, 0, width - 1, height - 1), fill=color)
    rotated_layer = layer.rotate(angle_degrees, resample=Image.BICUBIC, expand=True)
    top_left = (
        int(center[0] - rotated_layer.width / 2),
        int(center[1] - rotated_layer.height / 2),
    )
    canvas.alpha_composite(rotated_layer, top_left)


def draw_paw(color):
    """Returns a square canvas with a paw print: four toes and a main pad."""
    canvas = Image.new("RGBA", (CANVAS_SIZE, CANVAS_SIZE), (0, 0, 0, 0))
    toe_size = (190, 250)
    draw_rotated_ellipse(canvas, (205, 430), toe_size, 28, color)
    draw_rotated_ellipse(canvas, (395, 235), toe_size, 8, color)
    draw_rotated_ellipse(canvas, (629, 235), toe_size, -8, color)
    draw_rotated_ellipse(canvas, (819, 430), toe_size, -28, color)

    pad_drawing = ImageDraw.Draw(canvas)
    pad_drawing.ellipse((262, 470, 762, 900), fill=color)
    pad_drawing.ellipse((190, 600, 470, 900), fill=color)
    pad_drawing.ellipse((554, 600, 834, 900), fill=color)
    return canvas


def save_icon(canvas, file_name):
    """Saves the canvas as an .ico file that holds every size in ICON_SIZES."""
    output_path = RESOURCES_DIRECTORY / file_name
    canvas.save(output_path, format="ICO", sizes=[(size, size) for size in ICON_SIZES])
    print(f"Wrote {output_path}")


def main():
    """Writes the app icon, its preview image and one tray icon per lock state."""
    RESOURCES_DIRECTORY.mkdir(parents=True, exist_ok=True)
    save_icon(draw_paw(APP_ICON_COLOR), "pawse.ico")
    for file_name, color in TRAY_ICONS.items():
        save_icon(draw_paw(color), file_name)
    draw_paw(APP_ICON_COLOR).resize((256, 256), Image.LANCZOS).save(
        RESOURCES_DIRECTORY / "pawse.png"
    )


if __name__ == "__main__":
    main()
