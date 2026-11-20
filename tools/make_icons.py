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

PAW_COLOR = (242, 140, 40, 255)
LOCKED_PAW_COLOR = (229, 72, 77, 255)
BADGE_RING_COLOR = (255, 255, 255, 255)
BADGE_FILL_COLOR = (40, 40, 46, 255)


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


def draw_lock_badge(canvas):
    """Adds a small padlock badge to the bottom-right corner of the canvas."""
    drawing = ImageDraw.Draw(canvas)
    badge_box = (560, 560, 1020, 1020)
    drawing.ellipse(badge_box, fill=BADGE_RING_COLOR)
    drawing.ellipse((600, 600, 980, 980), fill=BADGE_FILL_COLOR)
    drawing.rounded_rectangle((680, 770, 900, 920), radius=24, fill=BADGE_RING_COLOR)
    drawing.arc((715, 660, 865, 830), start=180, end=360, fill=BADGE_RING_COLOR, width=34)
    drawing.line((732, 745, 732, 780), fill=BADGE_RING_COLOR, width=34)
    drawing.line((848, 745, 848, 780), fill=BADGE_RING_COLOR, width=34)
    return canvas


def save_icon(canvas, file_name):
    """Saves the canvas as an .ico file that holds every size in ICON_SIZES."""
    output_path = RESOURCES_DIRECTORY / file_name
    canvas.save(output_path, format="ICO", sizes=[(size, size) for size in ICON_SIZES])
    print(f"Wrote {output_path}")


def main():
    """Writes the normal and the locked tray icons."""
    RESOURCES_DIRECTORY.mkdir(parents=True, exist_ok=True)
    save_icon(draw_paw(PAW_COLOR), "pawse.ico")
    save_icon(draw_lock_badge(draw_paw(LOCKED_PAW_COLOR)), "pawse-locked.ico")
    draw_paw(PAW_COLOR).resize((256, 256), Image.LANCZOS).save(
        RESOURCES_DIRECTORY / "pawse.png"
    )


if __name__ == "__main__":
    main()
