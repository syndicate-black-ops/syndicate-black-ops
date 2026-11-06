$modelname "v_pm66.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "newpm"
}

$flags 0

$attachment 0 "Bone03" 1.7 -1.5 0

$texturegroup "skinfamilies"
{
	{ "test.bmp" }
	{ "test_lh.bmp" }
}

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 L Forearm" 0 -2.55 -2.63 11.693445 3.35 1.75
$hbox 0 "Bip01 L Hand" -1.37 -1.27 -2.35 4.06 1.18 1.69
$hbox 0 "Bip01 L Finger0" 0 -0.64 -0.79 1.71 0.62 0.66
$hbox 0 "Bip01 L Finger01" 0 -0.56 -0.7 1.18 0.63 0.56
$hbox 0 "Bip01 R Forearm" 0 -2.44 -1.48 11.7 2.14 1.58
$hbox 0 "Bip01 R Hand" 0 -0.99 -2.196014 4.07 0.93 2.34
$hbox 0 "Bip01 R Finger0" 0 -0.67 -0.71 1.72 0.59 0.75
$hbox 0 "Bip01 R Finger01" 0 -0.6 -0.6 1.19 0.6 0.67
$hbox 0 "Bone03" -12.74 -1.46 -1.2 0 6.73 1.16

$sequence "idle" {
	"..\anims\idle"
	fps 15
	loop
}
$sequence "reload" {
	"..\anims\reload"
	fps 49
}
$sequence "draw" {
	"..\anims\draw"
	fps 30
}
$sequence "shoot1" {
	"..\anims\shoot1"
	{ event 5001 0 "12" }
	fps 30
}
$sequence "shoot2" {
	"..\anims\shoot2"
	{ event 5001 0 "12" }
	fps 30
}
$sequence "shoot3" {
	"..\anims\shoot3"
	{ event 5001 0 "12" }
	fps 30
}
