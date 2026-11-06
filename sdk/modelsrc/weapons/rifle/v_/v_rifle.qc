$modelname "v_rifle.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "plasmaref"
}

$flags 0

$attachment 0 "Fire" 0 0 0

$texturegroup "skinfamilies"
{
	{ "skinplasma2.bmp" }
	{ "skinplasma2_lh.bmp" }
}

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 L Arm2" -2.52 -1.38 -1.25 10.39 1.07 1.38
$hbox 0 "Bip01 R Arm2" -2.4 -1.4 -1.74 10.23 1.04 1.03
$hbox 0 "Bip01 R Hand" -0.64 -0.32 -1.31 38.886753 2.096555 7.165126
$hbox 0 "Fire" -41.349998 -3.75 -6.01 21.629999 2.39 3.41
$hbox 0 "Bone01" -0.79 -1.25 -0.57 2.23 1.02 0.59

$sequence "idle" {
	"..\anims\idle"
	fps 10
	loop
}
$sequence "longidleidle" {
	"..\anims\longidleidle"
	{ event 5001 0 "34" }
	fps 25
}
$sequence "shoot" {
	"..\anims\shoot"
	{ event 5001 0 "34" }
	fps 25
}
$sequence "shootempty" {
	"..\anims\shootempty"
	{ event 5001 0 "34" }
	fps 25
}
$sequence "reload" {
	"..\anims\reload"
	fps 30
}
$sequence "draw" {
	"..\anims\draw"
	fps 21
}
