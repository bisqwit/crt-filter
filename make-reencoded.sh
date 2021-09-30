parallel -j6 -- \
	"./reencode.sh 200 sync-gwbasic.avi sync-gwbasic-fancy.mkv 640 200   60 2880 2160" \
	"./reencode.sh 400 tp5.avi tp5-fancy.mkv                   2880 400  60 2880 2160" \
	"./reencode.sh 400 sync-qbasic.avi sync-qbasic-fancy.mkv   2880 400  60 2880 2160" \
	"./reencode.sh 400 sync-editv2.avi sync-editv2-fancy.mkv   2880 400  60 2880 2160" \
	"./reencode.sh 400 sync-bc3.avi sync-bc3-fancy.mkv         2880 400  60 2880 2160" \
	"./reencode.sh 400 sync-bp7.avi sync-bp7-fancy.mkv         2880 400  60 2880 2160" \
	"./reencode.sh 400 bp7conf.avi bp7conf-fancy.mkv           2880 400  60 2880 2160" \
	"./reencode.sh 350 q.avi q-fancy.mkv                       640 350   60 2880 2160"
