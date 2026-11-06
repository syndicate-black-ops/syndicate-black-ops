$modelname "w_squeak.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "pit_ref"
}

$flags 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bone03" -5.22 -5.81 -5.72 23.889999 5.47 5.72
$hbox 0 "Bone04" -0.100028 0 0 3.729268 4.283882 4.468233
$hbox 0 "Bone05" 0 -1.2 -2.16 4.58 1.7 2.06
$hbox 0 "Bone07" -0.96 -2.65 -11.97 10.281999 2.91 1.5
$hbox 0 "Bone08" -1.36 -2.98 -12.73 6.292304 1.23 1.38
$hbox 0 "Bone09" -0.25 -1.53 -12.75 5.28 1.99 0.59
$hbox 0 "Bone22" 0 -3.12 -1.29 10.004332 4.48 2.06
$hbox 0 "Bone19" 0 -2.52 -1.36 4.79 0.92 1.23
$hbox 0 "Bone20" 0 -1.01 -0.9 4.41 0.71 0.59
$hbox 0 "Bone01" -0.95 -3.51 -4.34 10.22 7.16 4.21
$hbox 0 "Bone15" 0 -3.11 -1.87 10.004333 4.49 1.56
$hbox 0 "Bone16" 0 -2.51 -0.96 4.76 0.93 1.63
$hbox 0 "Bone17" 0 -0.97 -0.57 4.41 0.75 1.02

$sequence "idle" {
	"..\anims\idle"
	fps 107
}
$sequence "fidget" {
	"..\anims\fidget"
	fps 11
}
$sequence "jump" {
	"..\anims\jump"
	fps 13
}
$sequence "run" {
	"..\anims\run"
	fps 57
	loop
}
