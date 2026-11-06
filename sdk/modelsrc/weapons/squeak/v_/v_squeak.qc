$modelname "v_squeak.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "pit1_ref"
}

$flags 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 L Arm2" -2.28 -1.54 -1.36 11.86 1.15 1.68
$hbox 0 "Bip01 R Arm2" -2.34 -1.6 -2 11.89 1.15 1.13
$hbox 0 "Bip01 R Hand" -0.8 -0.4 -1.64 8.667434 4.631524 1.73
$hbox 0 "Bone03" -5.22 -5.81 -5.72 23.889999 5.47 5.72
$hbox 0 "Bone04" -0.102319 0 -4.205177 3.729266 4.283881 4.468233
$hbox 0 "Bone14" -9.5 -2.9 -1.7 10.282 2.66 1.36
$hbox 0 "Bone13" -25.48 -7.21 -1.56 6.292303 0 1.3
$hbox 0 "Bone11" -17.120001 0 -1.74 0 31.959999 0.07
$hbox 0 "Bone05" 0 -1.2 -2.16 4.58 1.7 2.06
$hbox 0 "Bone07" 0 -2.65 -1.56 10.281996 2.91 1.5
$hbox 0 "Bone08" -0.15 -2.74 -1.5 6.292304 1.23 1.38
$hbox 0 "Bone09" -0.25 -1.53 -1.05 4.9 0.91 0.59
$hbox 0 "Bone22" 0 -3.12 -1.29 10.004332 4.48 2.06
$hbox 0 "Bone19" 0 -2.52 -1.36 4.79 0.92 1.23
$hbox 0 "Bone20" 0 -1.01 -0.9 4.41 0.71 0.59
$hbox 0 "Bone01" -0.95 -3.51 -4.34 10.22 7.16 4.21
$hbox 0 "Bone15" 0 -3.11 -1.87 10.004333 4.49 1.56
$hbox 0 "Bone16" 0 -2.51 -0.96 4.76 0.93 1.63
$hbox 0 "Bone17" 0 -0.97 -0.57 4.41 0.75 1.02

$sequence "idle1" {
	"..\anims\idle1"
	fps 16
	loop
}
$sequence "fidgetfit" {
	"..\anims\fidgetfit"
	fps 16
}
$sequence "fidgetnip" {
	"..\anims\fidgetnip"
	fps 16
}
$sequence "down" {
	"..\anims\down"
	fps 16
}
$sequence "up" {
	"..\anims\up"
	fps 30
}
$sequence "throw" {
	"..\anims\throw"
	fps 24
}
