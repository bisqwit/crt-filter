<?php

$c=1;
function eq($n)
{
  global $c;
  return exp(-0.5*($n-0.5)*($n-0.5)/($c*$c));
}

$w = 80;
$h = 392;

$im    = ImageCreateTrueColor($w*3+4, $h);
$lines = 4;

$s=0;
foreach(Array(0.1, 0.3, 0.5) as $c)
{
  for($y=0; $y<$h; ++$y)
  {
    $line = $y*$lines/$h;
    $eq   = eq($line - (int)$line);
    
    $color = ImageColorAllocate($im, $eq*200, $eq*50, $eq*255);
    ImageLine($im, $s, $y, $s+$w, $y, $color);
  }
  for($l=0; $l<$lines; ++$l)
  {
    for($d = 0; $d < 4; ++$d)
    {
      $y = ($l + $d/4) * $h/$lines;
      
      $wid = 1/16;
      if($d) $wid /= 4;
      if($d==2) $wid *= 2;
      
      ImageLine($im, $s+0, $y-1, $s+$w*$wid, $y-1, 0xAA55AA);
      ImageLine($im, $s+0, $y  , $s+$w*$wid, $y  , 0xFFFFFF);
      ImageLine($im, $s+0, $y+1, $s+$w*$wid, $y+1, 0xAA55AA);
      
      ImageString($im, 2, $s+$w*$wid+3, $y, $l+$d/4, 0xFFFFFF);
    }
  }
  ImageString($im, 3, $s+$w*0.4, 0, "c = $c", 0xFFFF55);

  $s += $w+2;
}

ImagePng($im, 'test3.png');

