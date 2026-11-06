$modelname "v_nm33.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "v_nm33"
}

$flags 0

$attachment 0 "Bone06" -31 2 0

$texturegroup "skinfamilies"
{
	{ "v_nm33-null.bmp" }
	{ "v_nm33-null_lh.bmp" }
}

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bone06" -31 -5 -1 15 5 1
$hbox 0 "Bip01 R Forearm" 0 -2 -2 12 2 2
$hbox 0 "Bip01 L Forearm" 0 -2 -2 12 2 2
$hbox 0 "Bip01 R Hand" 0 -2 -2 6 3 4
$hbox 0 "Bip01 L Hand" 0 -2 -4 7 3 2

$sequence "idle1" {
	"..\anims\idle1"
	fps 16
}
$sequence "shoot" {
	"..\anims\shoot"
	{ event 5001 1 "30" }
	fps 45
}
$sequence "shoot2" {
	"..\anims\shoot2"
	{ event 5001 1 "30" }
	fps 45
}
$sequence "reload" {
	"..\anims\reload"
	{ event 5004 15 "weapons/nm33_clipout.wav" }
	{ event 5004 43 "weapons/nm33_clipin.wav" }
	{ event 5004 57 "weapons/nm33_hit.wav" }
	fps 36
}
$sequence "draw" {
	"..\anims\draw"
	fps 34
}
