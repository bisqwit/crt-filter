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

## Screenshots

![Original1](img/mpv-shot0001.jpg)
![Filtered1](img/mpv-shot0002.jpg)

![Original2](img/mpv-shot0003.jpg)
![Filtered2](img/mpv-shot0004.jpg)

## How it works

### Hardcoded parameters

* The number of simulated horizontal pixels is hard-coded at 640.
* The number of simulated vertical pixels is hard-coded at 400.

The simulated cell geometry is hardcoded as:

* Red cell   is 2 pixels of red and 1 pixel of black
* Green cell is 2 pixels of green and 1 pixel of black
* Blue cell  is 2 pixels of blue and 2 pixels of black
* Each cell  is 5 pixels tall followed by 1 pixel of black
* Successive columns are staggered 3 pixels apart vertically

The intermediate geometry,
as it appears below in the algorithm description,
is calculated as follows:

The intermediate width is the number of pixel cells on the screen
multiplied by the sum of cell widths.

![width](https://render.githubusercontent.com/render/math?math=640\times%282%2B1+%2B+2%2B1+%2B+2%2B2%29=6400)

The intermediate height is the number of vertical pixels on screen
multiplied by the cell height.

![height](https://render.githubusercontent.com/render/math?math=400\times%285%2B1%29=2400)

### Hashing

The filter is designed for DOS videos, and specifically for sessions
involving the text mode. Because chances are that successive frames are
identical or almost identical, the filter calculates a hash of every source frame.

If the hash is found to be identical to some previous frame,
the filtered result of the previous frame is sent.
Otherwise, the new frame is processed, and saved into a cache with the hash of the input image.

### Converting into linear colors

First, the image is un-gammacorrected by exponentiating every color component with

![1/gamma](https://render.githubusercontent.com/render/math?math=\gamma^{-1})

Crt-filter uses

![gamma=2](https://render.githubusercontent.com/render/math?math=\gamma=2)

### Rescaling, part 1

Then, the image is rescaled to the height of number of given scanlines
using a Lanczos filter.

The Lanczos filter has filter width set as 2.

### Rescaling, part 2

Next, the image is rescaled to the intermediate height using a nearest-neighbor filter.
The brightness of each row of pixels is adjusted by a constant factor
that is calculated by

![formula](https://render.githubusercontent.com/render/math?math=e^{-\frac{%28n-0.5%29^2}{2+c^2}})

where c=0.3, and n is the decimal part of the source Y coordinate.

### Rescaling, part 3

Then, the image is rescaled to an intermediate width using a nearest-neighbor filter.


### Filtering

Each color channel and each pixel of the picture — now intermediate width and height — is multiplied by a mask
that is either one or zero, depending on whether that pixel belongs inside a
cell of that color according to the hardcoded cell geometry.

The mask is a repeating pattern that essentially looks like this:

![Mask](img/mask.png)

Where red pixels are 1 for red channel, green pixels are 1 for green channel, and blue pixels are 1 for blue channel, and everything else for everyone is 0.

The mask is generated procedurally from the cell parameters.

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

Then, the picture and its blurry copy are added together,
and each pixel is clamped to the target range using a desaturation formula.

### The desaturation formula

The desaturation formula calculates a luminosity value from the input R,G,B
components using ITU coefficients (see [sRGB on Wikipedia](https://en.wikipedia.org/wiki/SRGB)):

![luma calculation](https://render.githubusercontent.com/render/math?math=luma=0.2126\cdot+value_{red}%2B0.7152\cdot+value_{green}%2B0.0722\cdot+value_{blue})

* If the luminosity is less than 0, black is returned.
* If the luminosity is more than 1, white is returned.
* Otherwise, a saturation value is initialized as 1, and then adjusted by inspecting each color channel value separately:

![adjust](https://render.githubusercontent.com/render/math?math=saturation\leftarrow\begin{cases}\min%28saturation,\frac{luma-1}{luma-value_{channel}}%29,+%26+\text{if+}value_{channel}\gt+1%5C%5C%0D%0A\min%28saturation,\frac{luma}{luma-value_{channel}}%29,+%26+\text{if+}value_{channel}\lt+0%5C%5Csaturation%26\text{otherwise}\end{cases})

After analyzing all color channels,
if the saturation still remains as 1, the input color is returned verbatim.
Otherwise each color channel is readjusted as:

![adjust](https://render.githubusercontent.com/render/math?math=value_{channel}\prime=\min%281,\max%280,%28value_{channel}-luma%29\cdot+saturation%2Bluma%29%29)

The readjusted color channel values are then joined together to form the returned color.
