scanlines=$1
outputfile="$3"

w=$4
h=$5

ow=$7
oh=$8
r=$6

f="$2"

ffmpeg -i "$f" -sws_flags lanczos -vf scale=$w:$h -pix_fmt bgra \
	-f rawvideo -threads 14 -r $r -y /dev/stdout \
| ./crt-filter $w $h $ow $oh $scanlines \
| ffmpeg -f rawvideo -pixel_format bgra -video_size $ow"x"$oh \
	 -framerate $r -i /dev/stdin \
	 -c:v h264 -pix_fmt yuv444p -crf 14 -threads 14 \
	 -g $((r/2)) -preset veryslow "$outputfile"


# The first ffmpeg just converts the colorspace into BGRA.
# It should not change the resolution.

# The second one does the rescaling.

# The third one compresses as H.264 — again, without rescaling.
