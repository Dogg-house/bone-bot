import logging
from PIL import Image
import numpy as np
import subprocess
import os
import sys
import typing
import pathlib
import logging
import argparse
import tempfile

twerk_frame_count = 6  # 0.png to 5.png

# Load twerk frames 🥵
# Modification from the original,
# I changed the backgrounds of the images
# to be transparent
twerk_frames = []
twerk_frames_data = []  # Image as numpy array, pre-calculated for performance
for i in range(6):
    try:
        img = Image.open(f"resources/twerk_imgs/{i}.png").convert("RGBA")
    except Exception as e:
        print(f"Error loading twerk frames! Filename = {i}.png")
        print("Probably you renamed the twerk_imgs folder or forgot to set twerk_frame_count. baka")
        print(e)
        exit()
    twerk_frames.append(img)
    twerk_frames_data.append(np.array(img))

# Get dimensions of first twerk frame. Assume all frames have same dimensions
twerk_width, twerk_height = twerk_frames[0].size

# Modification from the original,
# Generate a transparent frame to
# use for transparent pixels in the
# original image
transparent_frame = Image.new(mode='RGBA', size=twerk_frames[0].size, color=(0, 0, 0, 0))


def to_sus(input_image: Image, output_filename: str, output_width: int):
    input_image.convert("RGBA")
    input_width, input_height = input_image.size

    # Height of output gif (in crewmates)
    output_height = int(output_width * (input_height / input_width) * (twerk_width / twerk_height))

    # Width, height of output in pixels
    output_px = (int(output_width * twerk_width), int(output_height * twerk_height))

    # Scale image to number of crewmates, so each crewmate gets one color
    input_image_scaled = input_image.resize((output_width, output_height), Image.NEAREST)

    with tempfile.TemporaryDirectory() as tmpDir:
        for frame_number in range(twerk_frame_count):
            print("Sussying frame #", frame_number)

            # Create blank canvas
            background = Image.new(mode="RGBA", size=output_px)
            for y in range(output_height):
                for x in range(output_width):
                    r, g, b, a = input_image_scaled.getpixel((x, y))

                    # Modification from the original,
                    # if a pixel is invisible, don't
                    # put in a crew mate
                    if a == 0:
                        background.paste(transparent_frame, (x * twerk_width, y * twerk_height))
                    else:
                        # Grab that twerk data we calculated earlier
                        # (x - y + frame_number) is the animation frame index,
                        # we use the position and frame number as offsets to produce the wave-like effect
                        sussified_frame_data = np.copy(twerk_frames_data[(x - y + frame_number) % len(twerk_frames)])
                        red, green, blue, alpha = sussified_frame_data.T
                        # Replace all pixels with color (214,224,240) with the input image color at that location
                        color_1 = (red == 214) & (green == 224) & (blue == 240)
                        sussified_frame_data[..., :-1][color_1.T] = (r, g, b)  # thx stackoverflow
                        # Repeat with color (131,148,191) but use two thirds of the input image color to get a darker color
                        color_2 = (red == 131) & (green == 148) & (blue == 191)
                        sussified_frame_data[..., :-1][color_2.T] = (int(r * 2 / 3), int(g * 2 / 3), int(b * 2 / 3))

                        # Convert sussy frame data back to sussy frame
                        sussified_frame = Image.fromarray(sussified_frame_data)

                        # Slap said frame onto the background
                        background.paste(sussified_frame, (x * twerk_width, y * twerk_height))
            logging.info(f'Writting suss-ed frame \'{tmpDir}/sussified_{frame_number}.png\'')
            background.save(f"{tmpDir}/sussified_{frame_number}.png")

        logging.info("Converting sussy frames to sussy gif")
        subprocess.run(f'gifski --repeat 0 -o {output_filename} {tmpDir}/sussified_*.png', shell=True)

    # lamkas a cute


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        prog="sus.py",
        description="Get sussy with an image"
    )
    parser.add_argument('image_file')
    parser.add_argument('output_filename')
    parser.add_argument('width', type=int)

    args = parser.parse_args()

    input_image = Image.open(args.image_file).convert(mode="RGBA")
    to_sus(input_image=input_image, output_filename=args.output_filename, output_width=args.width)
