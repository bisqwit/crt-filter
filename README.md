<script type="text/javascript" charset="utf-8" src="https://cdn.mathjax.org/mathjax/latest/MathJax.js?config=TeX-AMS-MML_HTMLorMML, https://vincenttam.github.io/javascripts/MathJaxLocal.js"></script>

# Bisqwit’s CRT filter

This is the CRT filter that I used in my ”What is That Editor” video,
at https://www.youtube.com/watch?v=ZMBQmhO8KqI.

It received some accolades, but I forgot to publish it.
Here it is finally.

## To build

Run this command to build the filter:

    g++ -o crt-filter crt-filter.cc -fopenmp -Ofast -march=native -Wall -Wextra

## Usage

You can find some example usage in make-reencoded.sh.

The filter takes BGRA (RGB32) video from stdin,
and produces BGRA video into stdout.

The filter takes five commandline parameters:

    ./crt-filter <sourcewidth> <sourceheight> <outputwidth> <outputheight> <scanlines>

The sourcewidth and sourceheight denote the size of the original video.
The outputwidth and outputheight denote the size that you want to produce.
Generally speaking you want to produce as high quality as possible.
Vertical resolution is more important than horizontal resolution.

Scanlines is the number of scanlines you wish to simulate.
Generally that would be the same as the vertical resolution of the source video,
but that is not a requirement.

* The number of horizontal pixels simulated is hard-coded at 640.
* The number of vertical pixels simulated is hard-coded at 400.
* The simulated cell geometry is hardcoded as:
  * Red cell is 2 pixels of red and 1 pixel of black
  * Green cell is 2 pixels of green and 1 pixel of black
  * Blue cell is 2 pixels of blue and 2 pixels of black
  * Each cell is 5 pixels tall followed by 1 pixel of black
  * Successive columns are staggered 3 pixels apart vertically

## Screenshots

![Original1](img/mpv-shot0001.jpg)
![Filtered1](img/mpv-shot0002.jpg)

![Original2](img/mpv-shot0003.jpg)
![Filtered2](img/mpv-shot0004.jpg)

## How it works

### Hashing

The filter is designed for DOS videos, and specifically for sessions
involving the text mode. Because chances are that successive frames are
identical or almost identical, the filter calculates a hash of every source frame.

If the hash is found to be identical to some previous frame,
the filtered result of the previous frame is sent.
Otherwise, the new frame is processed, and saved into a cache with the hash of the input image.

### Converting into linear colors

First, the image is un-gammacorrected by exponentiating every color component with γ⁻¹.

Crt-filter uses $\gamma = 2$.

### Rescaling, part 1

Then, the image is rescaled to the height of number of given scanlines
using a Lanczos filter.

The Lanczos filter has filter width set as 2.

### Rescaling, part 2

Next, the image is rescaled to the intermediate height using a nearest-neighbor filter.
The brightness of each row of pixels is adjusted by a constant factor
that is calculated by $\e^{-\frac{(n-0.5)^2}{2 c^2}}$ where $c = 0.3$
and $n$ is the decimal part of the source Y coordinate.

The intermediate height is the number of vertical pixels on screen
multiplied by the cell height. This number is hardcoded
as $400 \times (5+1) = 2400$.

### Rescaling, part 3

Then, the image is rescaled to an intermediate width using a nearest-neighbor filter.

The intermediate width is the number of pixel cells on the screen
multiplied by the sum of cell widths. This number is hardcoded
as $640 \times (2+1 + 2+1 + 2+2) = 6400$.

### Filtering

Each color channel and each pixel of the picture — now intermediate width and height — is multiplied by a mask
that is either one or zero, depending on whether that pixel belongs inside a
cell of that color according to the hardcoded cell geometry.

### Rescaling, part 4

Then the image is rescaled to the target picture width 
and target picture height using a Lanczos filter.

### Bloom

First, the brightness of each pixel is normalized so that the sum of masks
and scanline magnitudes does not change the overall brightness of the picture.

Then, the picture is gamma-corrected by exponentiating each color component with γ.

Then, a copy is created of the picture.
This copy is gaussian-blurred using a three-step box filter,
where the blur width is set as output-width / 640.

Then, the picture is gamma-corrected (again? I am not sure why).

Then, the picture and its copy are added together, and each pixel is clamped
to the target range using a desaturation formula for overflows.

### The desaturation formula

The desaturation formula calculates a luminosity value from the input R,G,B
components using ITU coefficients 0.2126, 0.7152 and 0.0722.
* If the luminosity is in 0…1 range, nothing needs to be done and the input color is returned.    
* If the luminosity is less than 0, black is returned.
* If the luminosity is more than 1, white is returned.

Otherwise, each color channel is inspected separately.
* First, a saturation value is assigned as 1.
* If a color channel exceeds 1, saturation is adjusted as $min(saturation, (luma-1) / (luma-channelvalue))$.
* If a color channel preceeds 0, saturation is adjusted as $min(saturation, luma / (luma-channelvalue))$.

If the saturation is still 1, the input color is returned verbatim.
Otherwise each color channel is adjusted as $channelvalue = (channelvalue - luma)\times saturation + luma$.
The resulting value is then clamped to 0…1 range.

The color channel values are then joined together to form the returned color.
