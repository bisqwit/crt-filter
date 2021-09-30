# Bisqwit’s CRT filter

This is the CRT filter that I used in my ”What is That Editor” video,
at https://www.youtube.com/watch?v=ZMBQmhO8KqI.

It received some accolades, but I forgot to publish it.
Here it is finally.

## To build

Run this command to build the filter:

    g++ -o crt-filter crt-filter.cc -fopenmp -Ofast -march=native -Wall -Wextra

## Usage

The filter takes BGRA (RGB32) video (RAW!) from stdin,
and produces BGRA video (RAW!) into stdout.

The filter takes five commandline parameters:

    ./crt-filter <sourcewidth> <sourceheight> <outputwidth> <outputheight> <scanlines>

The sourcewidth and sourceheight denote the size of the original video.
The outputwidth and outputheight denote the size that you want to produce.
Generally speaking you want to produce as high quality as possible.
Vertical resolution is more important than horizontal resolution.

Scanlines is the number of scanlines you wish to simulate.
Generally that would be the same as the vertical resolution of the source video,
but that is not a requirement.

For best quality, the number of scanlines should be chosen
such that the intermediate height (see Hardcoded constants)
is its integer multiple.
The intermediate width should ideally also be an integer
multiple of the source width. None of this is required though.

IMPORTANT: This filter does *not* decode or produce video formats like avi/mp4/mkv/whatever.
It only deals with raw video frames. You need to use an external program,
like ffmpeg, to perform the conversions.
See `make-reencoded.sh` and `reencode.sh` for a practical example.

## Screenshots

![Original1](img/mpv-shot0001.jpg)
![Filtered1](img/mpv-shot0002.jpg)

![Original2](img/mpv-shot0003.jpg)
![Filtered2](img/mpv-shot0004.jpg)

## How it works

### Hardcoded constants

These constants specify the pixel grid (shadow mask) used by the simulated CRT monitor.

Currently they are hardcoded in the program,
but they are easy to find if you want to tweak the source code.

![width](https://render.githubusercontent.com/render/math?math=\begin{align*}npix_{width}%26=640+%5C%5C+npix_{height}%26=400+%5C%5C+cellwidth_{red}%26=cellwidth_{green}=cellwidth_{blue}=2+%5C%5C+cellblank_{red}%26=cellblank_{green}=1+%5C%5C+cellblank_{blue}%26=2+%5C%5C+cellheight_{vert}%26=5+%5C%5C+cellblank_{vert}%26=1+%5C%5C+cellstagger%26=3+%5C%5C+intermediatewidth%26=npix_{width}\cdot%28cellwidth_{red}%2Bcellblank_{red}%2Bcellheight_{green}%2Bcellblank_{green}%2Bcellwidth_{blue}%2Bcellblank_{blue}%29=6400+%5C%5C+intermediateheight%26=npix_{height}\cdot%28cellheight_{vert}%2Bcellblank_{vert}%29=2400\end{align*})

The cell widths and heights and staggering specify the geometry of the shadow
mask. See Filtering, below, for an example of what it looks like.

### Hashing

The filter is designed for DOS videos, and specifically for sessions
involving the text mode. Because chances are that successive frames are
identical or almost identical, the filter calculates a hash of every source frame.

If the hash is found to be identical to some previous frame,
the filtered result of the previous frame is sent.
Otherwise, the new frame is processed, and saved into a cache with the hash of the input image.

Four previous unique frames are cached. This accounts e.g. for blinking cursors.

### Converting into linear colors

First, the image is un-gammacorrected.

![1/gamma](https://render.githubusercontent.com/render/math?math=value\leftarrow+value^{\gamma^{-1}}\text{ where }\gamma=2\text{ for every color channel }value\text{ in the picture})

### Rescaling to scanline count

Then, the image is rescaled to the height of number of given scanlines using a Lanczos filter.
The Lanczos filter has filter width set as 2.

If your source height is greater than the number of scanlines you specified, you will lose detail.

### Rescaling to intermediate size

Next, the image is rescaled to the intermediate width and height using a nearest-neighbor filter.

The scaling is performed first vertically and then horizontally.
Before horizontal scaling, the brightness of each row of pixels
is adjusted by a constant factor that is calculated by

![formula](https://render.githubusercontent.com/render/math?math=e^{-\frac{%28n-0.5%29^2}{2+c^2}}\text{ where }c=0.3\text{ and }n\text{ is+the+fractional+part+of+the+source+Y+coordinate.})

### Filtering

Each color channel and each pixel of the picture — now intermediate width and height — is multiplied by a mask
that is either one or zero, depending on whether that pixel belongs inside a
cell of that color according to the hardcoded cell geometry.

The mask is a repeating pattern that essentially looks like this:

![Mask](img/mask.png)

Where red pixels are 1 for red channel, green pixels are 1 for green channel, and blue pixels are 1 for blue channel, and everything else for everyone is 0.

The mask is generated procedurally from the cell parameters.

### Rescaling to target size

Then the image is rescaled to the target picture width and target picture height using a Lanczos filter.
The scaling is performed first vertically and the horizontally.

### Bloom

First, the brightness of each pixel is normalized so that the sum of masks
and scanline magnitudes does not change the overall brightness of the picture.

Then, a copy is created of the picture.
This copy is gamma-corrected and amplified with a significant factor, to promote bloom.

![gamma](https://render.githubusercontent.com/render/math?math=value_{copy}=\frac{600}{255}value^\gamma\text{ for every color channel }value\text{ in the picture})

This copy is gaussian-blurred using a three-step box filter,
where the blur width is set as output-width / 640.

Then, the actual picture is gamma-corrected, this time without a brightening factor.

![gamma](https://render.githubusercontent.com/render/math?math=value\leftarrow+value^\gamma\text{ for every color channel }value\text{ in the picture})

Then, the picture and its blurry copy are added together,
and each pixel is clamped to the target range using a desaturation formula.

### The desaturation formula

The desaturation formula first calculates a luminosity value from the input R,G,B
components using ITU coefficients (see [sRGB on Wikipedia](https://en.wikipedia.org/wiki/SRGB)):

![luma calculation](https://render.githubusercontent.com/render/math?math=luma=0.2126\cdot+value_{red}%2B0.7152\cdot+value_{green}%2B0.0722\cdot+value_{blue})

* If the luminosity is less than 0, black is returned.
* If the luminosity is more than 1, white is returned.
* Otherwise, a saturation value is initialized as 1, and then adjusted by inspecting each color channel value separately:

![adjust](https://render.githubusercontent.com/render/math?math=saturation\leftarrow\begin{cases}\min%28saturation,\frac{luma-1}{luma-value_{channel}}%29,+%26+\text{if }value_{channel}\gt+1%5C%5C%0D%0A\min%28saturation,\frac{luma}{luma-value_{channel}}%29,+%26+\text{if }value_{channel}\lt+0%5C%5Csaturation%26\text{otherwise}\end{cases})

After analyzing all color channels,
if the saturation still remains as 1, the input color is returned verbatim.
Otherwise each color channel is readjusted as:

![adjust](https://render.githubusercontent.com/render/math?math=value_{channel}\prime=\min%281,\max%280,%28value_{channel}-luma%29\cdot+saturation%2Bluma%29%29)

The readjusted color channel values are then joined together to form the returned color.
