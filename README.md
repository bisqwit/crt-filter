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

(Click to enlarge the filtered pictures)

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

**NB: This page uses GitHub’s own LaTeX math renderer to show equations.
Unfortunately, this renderer produces transparent pictures with black text,
and has very poor usability on *dark mode.*
I am aware of this problem, but there is very little I can do about it,
until GitHub itself fixes it!
Sorry. Please view this site on desktop with non-dark mode.**

### Hashing

The filter is designed for DOS videos, and specifically for sessions
involving the text mode. Because chances are that successive frames are
often identical, the filter calculates a hash of every source frame.

If the hash is found to be identical to some previous frame,
the filtered result of the previous frame is sent.
Otherwise, the new frame is processed, and saved into a cache with the hash of the input image.

Four previous unique frames are cached. This accounts e.g. for blinking cursors.

### Converting into linear colors

First, the image is un-gammacorrected.

![1/gamma](https://render.githubusercontent.com/render/math?math=value\leftarrow+value^{\gamma^{-1}}\text{ where }\gamma=2\text{ for every color channel }value\text{ in the picture})

### Rescaling to scanline count

Then, the image is rescaled to the height of number of given scanlines using a Lanczos filter.
Kernel size 2 was was selected for the Lanczos filter.

If your source height is greater than the number of scanlines you specified, you will lose detail.

### Rescaling to intermediate size

Next, the image is rescaled to the intermediate width and height using a nearest-neighbor filter.

The scaling is performed first vertically and then horizontally.
Before horizontal scaling, the brightness of each row of pixels
is adjusted by a constant factor that is calculated by

![formula](https://render.githubusercontent.com/render/math?math=e^{-0.5%28n-0.5%29^{2}c^{-2}}\text{ where }c=0.3\text{ and }n\text{ is+the+fractional+part+of+the+source+Y+coordinate.})

This formula gives a gaussian distribution that looks like a hill,
that peaks in the middle and fades smoothly to the sides. 
This hill represents the brightness curve of each scanline.
Plotted in a graphing calculator, it looks like this.
The c constant controls how steep that hill is. A small value like 0.1
produces a very narrow hill with very sharp and narrow scanlines,
and bigger values produce flatter hills and less pronounced scanlines.
0.3 looked like a good compromise.

![Gaussian](img/weights.png)

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

A Lanczos filter was chosen because it is generally deemed the
best compromise between blurring and fringing
among several simple filters
([Wikipedia](https://en.wikipedia.org/wiki/Lanczos_resampling)).
I have been using it for years for interpolating all sorts of signals
from pictures to sounds.

### Bloom

First, the brightness of each pixel is normalized so that the sum of masks
and scanline magnitudes does not change the overall brightness of the picture.

Then, a copy is created of the picture.
This copy is gamma-corrected and amplified with a significant factor, to promote bloom.

![gamma](https://render.githubusercontent.com/render/math?math=value_{copy}=\frac{600}{255}value^\gamma\text{ for every color channel }value\text{ in the picture})

This copy is 2D-gaussian-blurred using a three-step box filter,
where the blur width is set as output-width / 640.
The blur algorithm is very fast and works in linear time,
adapted from http://blog.ivank.net/fastest-gaussian-blur.html .

Then, the actual picture is gamma-corrected, this time without a brightening factor.

![gamma](https://render.githubusercontent.com/render/math?math=value\leftarrow+value^\gamma\text{ for every color channel }value\text{ in the picture})

Then, blurry copy is added into with the picture,
by literally adding its pixel values into the target pixel values
and writing the result to the target.

![gamma](https://render.githubusercontent.com/render/math?math=value\leftarrow+value%2Bvalue_{copy}\text{ for every color channel }value\text{ in the picture})

Because of the combination of amplification and blurring,
if there are isolated bright pixels in the scene,
their power is spread out on big area
and thus do not contribute much to the final picture,
but if there is a large cluster of bright pixels closeby,
they remain bright even after blurring,
and will influence the final picture a lot.
This produces a bloom effect.

### Clamping

Finally, before quantizing the floating-point colors and sending the frame to output,
each pixel is clamped to the target range using a desaturation formula.

#### The desaturation formula

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

The advantage of desaturation-aware clamping over naïve clamping
is that it does a much better job at preserving energy.
To illustrate, here is a picture with two color ramps.
The brightness of the color ramp increases linearly along the Y axis.
That is, top is darkest (0) and bottom is brightest (1, i.e. full).
Every pixel on each scanline should be approximately same brightness.

The brightness scaling is done by simply multiplying the RGB color with the brightness value,
even if it produces out-of-range colors.

![Rainbow illustration](img/rainbow.png)

In the leftside picture with naïve clamping, you can see that the further
down you go in the picture, the more different the color brightnesses are.
The blue stripe is much, much darker than anything else in the picture,
even though it is fully saturated and as bright as your screen can make it.*

However, on the right side, with the desaturation aware clamping formula,
every scanline remains at perfectly even brightness, even
when you exceed the maximum possible brightness of the screen colors.

(Note: “Perfectly” was a hyperbole.
The colors are not quite the same brightness especially near the top
of the picture, because of differences in screen calibration and because of
differences in human individual eyes. This is more of an illustration.)
You can download the source code of this illustration in
[img/rainbow.php](img/rainbow.php).

Note that this does *not* mean that all colors become more washed out.
You may come to this mistaken conclusion, because this illustration is
fixed for perceptual brightness. Colors that are within the RGB color
range will be kept perfectly intact. The only colors that will be
desaturated are those that are have out-of-range values
(i.e. individual channel values are greater than 255 or smaller than 0).

*) Note that \#0000FF is not blue at brightness 1. While it is maximally bright
fully saturated blue, its brightness is only about 10 % of the brightness of
\#00FF00, maximally bright fully saturated green, and only about 7 % of the
brightness of \#FFFFFF, a maximally bright white pixel (which does have
brightness level of 1).

This is trivial to
prove: \#FFFFFF is a color where you light up all the LEDs that comprise
color \#0000FF, but you also light up all the LEDs that comprise \#FF0000
and all the LEDs that comprise \#00FF00. Because there are three times as
many LEDs shining as when just \#0000FF is shown, the brightness of \#FFFFFF
cannot be the same, but has to be much higher. Therefore, \#0000FF cannot
have brightness level of 1.

