$modelname "v_k444.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "br_ref"
}

$flags 0

$attachment 0 "Bone02" 11 -1 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 L Forearm" 0 -2.54 -2.63 11.693444 3.35 1.75
$hbox 0 "Bip01 L Hand" -1.37 -1.27 -2.36 4.06 1.18 1.68
$hbox 0 "Bip01 L Finger0" 0 -0.63 -0.8 1.71 0.62 0.66
$hbox 0 "Bip01 L Finger01" 0 -0.56 -0.71 1.18 0.64 0.56
$hbox 0 "Bip01 R Forearm" 0 -2.44 -1.48 11.71 2.14 1.57
$hbox 0 "Bip01 R Hand" 0 -0.99 -1.71 6.672049 1.509265 3.915262
$hbox 0 "Bip01 R Finger0" 0 -0.67 -0.71 1.72 0.59 0.75
$hbox 0 "Bip01 R Finger01" 0 -0.6 -0.6 1.19 0.6 0.67
$hbox 0 "Bone02" -4.55 -2.63 -1.87 10.41 5.47 1.68
$hbox 0 "Bone11" -2.2 0 -0.54 0.57 2.79 1.48
$hbox 0 "Bone17" -1.09 -1.83 -2.52 1.296608 0.32 0.07
$hbox 0 "Bone19" -1.56 -1.64 -2.42 0.38 0.36 0
$hbox 0 "Bone23" 0 -1.17 -1.88 3.45 1.19 1.7

$sequence "idle" {
	"..\anims\idle"
	fps 15
	loop
}
$sequence "idle" {
	"..\anims\idle_2"
	fps 15
	loop
}
$sequence "shoot1" {
	"..\anims\shoot1"
	{ event 5001 0 "22" }
	fps 13
}
$sequence "shoot2" {
	"..\anims\shoot2"
	{ event 5001 0 "22" }
	fps 13
}
$sequence "reload" {
	"..\anims\reload"
	fps 33
}
$sequence "reload" {
	"..\anims\reload_2"
	fps 33
}
$sequence "draw" {
	"..\anims\draw"
	fps 30
}
$sequence "draw" {
	"..\anims\draw_2"
	fps 30
}
