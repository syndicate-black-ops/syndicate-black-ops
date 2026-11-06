$modelname "v_fa18.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "v_fa18"
}

$flags 0

$texrendermode "fa18_02.bmp" chrome

$attachment 0 "Bone12" -14 0 0
$attachment 1 "Bone16" 10 0 0
$attachment 2 "Bone12" 0 0 0
$attachment 3 "Bone16" 5 0 0

$texturegroup "skinfamilies"
{
	{ "fa18_01.bmp" }
	{ "fa18_01_lh.bmp" }
}

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bone12" -15 -7 -1 15 1 1
$hbox 0 "Bip01 R Forearm" 0 -2 -2 12 2 2
$hbox 0 "Bip01 L Forearm" 0 -2 -2 12 2 2
$hbox 0 "Bip01 R Hand" 0 -2 -2 6 3 4
$hbox 0 "Bip01 L Hand" 0 -2 -4 7 3 2

$sequence "idle" {
	"..\anims\idle"
	fps 20
}
$sequence "shotgun" {
	"..\anims\shotgun"
	{ event 5011 1 "22" }
	{ event 5004 1 "weapons/fa18_pump.wav" }
	fps 24
}
$sequence "reload" {
	"..\anims\reload"
	{ event 5004 27 "weapons/fa18_clipout.wav" }
	{ event 5004 48 "weapons/fa18_clipin.wav" }
	{ event 5004 66 "weapons/fa18_reload.wav" }
	fps 36
}
$sequence "draw" {
	"..\anims\draw"
	fps 36
}
$sequence "holster" {
	"..\anims\holster"
	fps 16
}
$sequence "shoot" {
	"..\anims\shoot"
	{ event 5001 1 "30" }
	fps 24
}
