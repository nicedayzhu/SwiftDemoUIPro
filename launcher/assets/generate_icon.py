from pathlib import Path

from PIL import Image, ImageDraw


ASSET_DIR = Path(__file__).resolve().parent
CANVAS_SIZE = 1024

image = Image.new("RGBA", (CANVAS_SIZE, CANVAS_SIZE), (0, 0, 0, 0))
draw = ImageDraw.Draw(image)
draw.rounded_rectangle((24, 24, 1000, 1000), radius=272, fill="#5aa9ff")
draw.polygon(((424, 312), (424, 712), (752, 512)), fill="#ffffff")

image.save(
    ASSET_DIR / "app_icon.ico",
    format="ICO",
    sizes=((16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)),
)
