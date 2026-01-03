from PIL import Image, ImageDraw
import os
import math

# Configuration
ICON_SIZE = (96, 96)
OUTPUT_DIR = "data"

# Colors (6-Color Palette)
WHITE = (255, 255, 255)
BLACK = (0, 0, 0)
RED = (255, 0, 0)
GREEN = (0, 255, 0)
BLUE = (0, 0, 255)
YELLOW = (255, 255, 0)

# Ensure output directory exists
os.makedirs(OUTPUT_DIR, exist_ok=True)

def create_base_image():
    return Image.new("RGB", ICON_SIZE, WHITE)

def draw_sun(draw, x, y, radius):
    # Red outline (thick)
    draw.ellipse([x-radius-3, y-radius-3, x+radius+3, y+radius+3], fill=RED)
    # Yellow fill
    draw.ellipse([x-radius, y-radius, x+radius, y+radius], fill=YELLOW)

def draw_cloud(draw, x, y, scale=1.0):
    # Simple cloud shape composed of circles
    circles = [
        (x, y, 20 * scale),
        (x + 15 * scale, y - 10 * scale, 25 * scale),
        (x + 35 * scale, y, 20 * scale),
        (x + 15 * scale, y + 5 * scale, 20 * scale)
    ]
    
    # Draw black outline (larger circles)
    for cx, cy, r in circles:
        draw.ellipse([cx-r-3, cy-r-3, cx+r+3, cy+r+3], fill=BLACK)
        
    # Draw white fill
    for cx, cy, r in circles:
        draw.ellipse([cx-r, cy-r, cx+r, cy+r], fill=WHITE)

def draw_rain(draw, x, y):
    # Blue drops
    for i in range(3):
        dx = x + i * 15
        draw.line([dx, y, dx - 5, y + 15], fill=BLUE, width=4)

def draw_snow(draw, x, y):
    # Blue flakes
    for i in range(3):
        dx = x + i * 15
        dy = y + 10
        # Cross shape
        draw.line([dx-5, dy, dx+5, dy], fill=BLUE, width=3)
        draw.line([dx, dy-5, dx, dy+5], fill=BLUE, width=3)

def draw_lightning(draw, x, y):
    # Yellow bolt with Red outline
    points = [
        (x, y), (x-15, y+25), (x+5, y+25), (x-5, y+50)
    ]
    # Outline
    draw.line(points, fill=RED, width=6)
    # Fill
    draw.line(points, fill=YELLOW, width=2)

def draw_fog(draw, x, y):
    # Wavy lines
    for i in range(3):
        base_y = y + i * 12
        points = []
        for px in range(0, 55, 2):
            # Sine wave approximation
            py = base_y + 4 * math.sin(px * 0.2)
            points.append((x + px, py))
        draw.line(points, fill=BLACK, width=2)

def draw_wind(draw, x, y):
    # Curling lines
    # Top line
    draw.line([(x, y), (x+35, y)], fill=BLUE, width=3)
    # Curl back
    draw.arc([x+25, y-10, x+45, y+10], start=270, end=100, fill=BLUE, width=3)
    
    # Bottom line offset
    y2 = y + 20
    x2 = x + 15
    draw.line([(x2, y2), (x2+35, y2)], fill=BLUE, width=3)
    draw.arc([x2+25, y2-10, x2+45, y2+10], start=270, end=100, fill=BLUE, width=3)

# --- Icon Generators ---

def gen_clear():
    img = create_base_image()
    draw = ImageDraw.Draw(img)
    draw_sun(draw, 48, 48, 30)
    return img

def gen_partly_cloudy():
    img = create_base_image()
    draw = ImageDraw.Draw(img)
    draw_sun(draw, 65, 30, 20)
    draw_cloud(draw, 20, 55)
    return img

def gen_cloudy():
    img = create_base_image()
    draw = ImageDraw.Draw(img)
    draw_cloud(draw, 25, 45, 1.2)
    return img

def gen_rain():
    img = create_base_image()
    draw = ImageDraw.Draw(img)
    draw_cloud(draw, 25, 45, 1.1) # Moved down
    draw_rain(draw, 35, 75) # Moved down
    return img

def gen_snow():
    img = create_base_image()
    draw = ImageDraw.Draw(img)
    draw_cloud(draw, 25, 45, 1.1) # Moved down
    draw_snow(draw, 35, 75) # Moved down
    return img

def gen_storm():
    img = create_base_image()
    draw = ImageDraw.Draw(img)
    draw_cloud(draw, 25, 45, 1.1) # Moved down
    draw_lightning(draw, 48, 75) # Moved down
    return img

def gen_fog():
    img = create_base_image()
    draw = ImageDraw.Draw(img)
    draw_fog(draw, 20, 35)
    return img

def gen_wind():
    img = create_base_image()
    draw = ImageDraw.Draw(img)
    draw_wind(draw, 20, 40)
    return img

# Mapping Google API names to generators
icons = {
    "clear_day": gen_clear,
    "clear_night": gen_clear, # Use sun for now, or make a moon
    "sunny": gen_clear,
    "partly_cloudy": gen_partly_cloudy, # Added generic name
    "partly_cloudy_day": gen_partly_cloudy,
    "partly_cloudy_night": gen_partly_cloudy,
    "partly_clear": gen_partly_cloudy,
    "mostly_clear": gen_partly_cloudy,
    "cloudy": gen_cloudy,
    "mostly_cloudy": gen_cloudy,
    "rain": gen_rain,
    "showers": gen_rain,
    "heavy_rain": gen_rain,
    "light_rain": gen_rain,
    "scattered_showers": gen_rain,
    "snow": gen_snow,
    "snow_showers": gen_snow,
    "flurries": gen_snow,
    "thunderstorm": gen_storm,
    "fog": gen_fog,
    "mist": gen_fog,
    "haze": gen_fog,
    "wind": gen_wind,
    "windy": gen_wind
}

# Generate
if __name__ == "__main__":
    print(f"Generating icons in '{OUTPUT_DIR}'...")
    for name, func in icons.items():
        img = func()
        img.save(f"{OUTPUT_DIR}/{name}.png")
        print(f"  - {name}.png")
    print("Done.")
