<?php

$a = 2126;
$b = 7152;
$c = 722;

$abc = $a+$b+$c+0.0;

function hsv_to_bgr($h,$s, $v)
{
    $v *= 255.0;

    if ($s == 0.0)
        return Array($v,$v,$v);
    else
    {
        while ($h < 0) $h += 360;
        $h = fmod($h, 360.0) / 60.0;
        $i = (int)$h;
        $frac = $h - $i;
        $p    = $v - $v*$s;
        $qt   = $v*$s*$frac;
        
        $bgr = Array();
        $bgr[ ((10-$i)%6)>>1 ] = ($v); // max  
        $bgr[ ( (7-$i)%6)>>1 ] = ($p); // min
        $bgr[    ($i+1)%3 ] = (
            ($i&1) ? $v - $qt    // max downto min
                   : $p + $qt ); // min   upto max
        return $bgr;
    }
}

$dither = Array(1,49,13,61,4,52,16,64,33,17,45,29,36,20,48,32,9,57,5,53,12,60,8,56,41,25,37,21,44,28,40,24,3,51,15,63,2,50,14,62,35,19,47,31,34,18,46,30,11,59,7,55,10,58,6,54,43,27,39,23,42,26,38,22);
foreach($dither as &$v)$v /= 64.; unset($v);

function clamp($pix, $x,$y)
{
  global $gamma,$dither;

  if($pix[0] < 0) $pix[0] = 0;
  if($pix[1] < 0) $pix[1] = 0;
  if($pix[2] < 0) $pix[2] = 0;
  $pix[0] = pow($pix[0]/255, $gamma)*255;
  $pix[1] = pow($pix[1]/255, $gamma)*255;
  $pix[2] = pow($pix[2]/255, $gamma)*255;

  $d = $dither[($x&7)*8+($y&7)];
  $pix[0] += $d;
  $pix[1] += $d;
  $pix[2] += $d;
  if($pix[0] > 255) $pix[0] = 255;
  if($pix[1] > 255) $pix[1] = 255;
  if($pix[2] > 255) $pix[2] = 255;
  
  return (int)$pix[2] * 65536
       + (int)$pix[1] * 256
       + (int)$pix[0];
}
function clamp2($pix, $x,$y)
{
  global $a,$b,$c,$abc;
  $lum = $pix[2]*$a + $pix[1]*$b + $pix[0]*$c;
  if($lum > 255*$abc) return 0xFFFFFF;
  if($lum <= 0)       return 0x000000;
  $lum /= ($abc*1.0);

  $aa=$a; $bb=$b; $cc=$c;
  #$aa=1111; $bb=3333; $cc=5334;
  for($round=0; $round<3; ++$round)
  {
    $excess = $aa*max(0, $pix[2]-255)
            + $bb*max(0, $pix[1]-255)
            + $cc*max(0, $pix[0]-255);
    // $excess is the amount of excess color energy that
    // we must dissipate.
    if($excess > 0)
    {
      // Check how much capacity there is on each channel.
      $capacity = 0;
      $cap2 = (255-$pix[2])*$aa;
      $cap1 = (255-$pix[1])*$bb;
      $cap0 = (255-$pix[0])*$cc;
      $capacity = max(0,$cap2) + max(0,$cap1) + max(0,$cap0);
      if($capacity > 0)
      {
        $distribute = min($capacity, $excess);
        $factor1    = $distribute/$capacity;

        // Add the color energy to capable channels
        if($cap2 > 0) $pix[2] += ($cap2*$factor1)/$aa;
        if($cap1 > 0) $pix[1] += ($cap1*$factor1)/$bb;
        if($cap0 > 0) $pix[0] += ($cap0*$factor1)/$cc;

        // And take it away from channels that had excess
        $factor2    = $distribute/$excess;
        if($cap2 < 0) $pix[2] += ($cap2*$factor2)/$aa;
        if($cap1 < 0) $pix[1] += ($cap1*$factor2)/$bb;
        if($cap0 < 0) $pix[0] += ($cap0*$factor2)/$cc;
      }
    }
    
    $debt = $aa*min(0, $pix[2])
          + $bb*min(0, $pix[1])
          + $cc*min(0, $pix[0]);
    // $debt is the amount of debt color energy
    // that we must borrow.
    if($debt < 0)
    {
      // Check how much capacity there is on each channel.
      $capacity = 0;
      $cap2 = ($pix[2])*$aa;
      $cap1 = ($pix[1])*$bb;
      $cap0 = ($pix[0])*$cc;
      $capacity = max(0,$cap2) + max(0,$cap1) + max(0,$cap0);
      if($capacity > 0)
      {
        $distribute = min($capacity, $excess);
        $factor1    = $distribute/$capacity;

        // Take away color energy from capable channels
        if($cap2 > 0) $pix[2] -= ($cap2*$factor1)/$aa;
        if($cap1 > 0) $pix[1] -= ($cap1*$factor1)/$bb;
        if($cap0 > 0) $pix[0] -= ($cap0*$factor1)/$cc;

        // And give it to channels that need it
        $factor2    = $distribute/$excess;
        if($cap2 < 0) $pix[2] -= ($cap2*$factor2)/$aa;
        if($cap1 < 0) $pix[1] -= ($cap1*$factor2)/$bb;
        if($cap0 < 0) $pix[0] -= ($cap0*$factor2)/$cc;
      }
    }
    if(!$excess && !$debt) break;
  }
  return clamp($pix,$x,$y);
}

$w  = 848;
$h  = 480;
$im = ImageCreateTrueColor($w*2, $h);

$gamma  = 1/2.0;
$gamma2 = 2.0;
for($y=0; $y<$h; ++$y)
{
  #$bright = pow($y/$h, 2.0)*3;
  $bright = pow($y/$h, $gamma2)*1.0;
  for($x=0; $x<$w; ++$x)
  {
    $pix = hsv_to_bgr($x * 1.5*360 / $w - 180,
                      min(1, $x*1.9/$w),
                      0.1);
    $pix[0] = pow($pix[0]/255., 1/$gamma);
    $pix[1] = pow($pix[1]/255., 1/$gamma);
    $pix[2] = pow($pix[2]/255., 1/$gamma);

    $lum = ($pix[2]*$a + $pix[1]*$b + $pix[0]*$c) / $abc;

    $pix[0] = ($pix[0] * $bright/$lum)*255;
    $pix[1] = ($pix[1] * $bright/$lum)*255;
    $pix[2] = ($pix[2] * $bright/$lum)*255;
    
    $color = clamp($pix, $x,$y);
    ImageSetPixel($im, $x,$y, $color);

    $color = clamp2($pix, $x,$y);
    ImageSetPixel($im, $x+$w+16,$y, $color);
  }
}
ImagePng($im, 'test.png');

print "done\n";

